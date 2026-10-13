/**
 * This file is based on the CLI example and stream_sender_receiver integration test
 * from https://github.com/microsoft/apsi which is licensed as follows:
 *
 * MIT License
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE
 */

// STD
#include <algorithm>
#include <cctype>
#include <sstream>
#include <numeric>
#include <random>
#include <memory>
#include <iostream>
#include <fstream>
#include <csignal>
#include <string>

// pybind11
#include <pybind11/pybind11.h>

// APSI
#include <apsi/item.h>
#include <apsi/log.h>
#include <apsi/receiver.h>
#include <apsi/sender.h>
#include <apsi/network/stream_channel.h>
#include <apsi/psi_params.h>
#include <apsi/sender_db.h>
#include <apsi/thread_pool_mgr.h>

using namespace std;
using namespace apsi;
using namespace apsi::sender;
using namespace apsi::receiver;

namespace py = pybind11;

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

void sigint_handler(int param [[maybe_unused]])
{
    exit(0);
}

/*
APSI 0.13.0 replaced the apsi::Log class with free functions and a pluggable apsi::Logger,
which means Log::SetLogFile and Log::SetConsoleDisabled are gone. PyAPSI keeps offering the
log file and the console as two knobs that can be set independently, so both settings are
remembered here and the logger matching their combination is installed on every change.
*/
string _log_file_path;
bool _console_log_enabled = true;

void refresh_logger()
{
    if (!_log_file_path.empty())
    {
        SetLogger(NewFileLogger(_log_file_path, _console_log_enabled));
    }
    else if (_console_log_enabled)
    {
        ResetDefaultLogger();
    }
    else
    {
        // A logger without any handler discards every message. The level threshold is
        // deliberately left alone, so that it still applies once a sink is installed again.
        SetLogger(Logger::Create({}, {}, {}));
    }
}

void set_log_level(const string &level)
{
    string normalized_level(level.size(), '\0');
    transform(level.begin(), level.end(), normalized_level.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });

    // APSI 0.13.0 renamed the most verbose level from "all" to "trace" and the quietest
    // from "off" to "suppress". Both of the previous names remain accepted here.
    if (normalized_level == "all")
    {
        normalized_level = "trace";
    }
    else if (normalized_level == "off")
    {
        normalized_level = "suppress";
    }

    // Throws std::invalid_argument for anything that is not a level name
    SetLogLevel(normalized_level);
}

void set_console_log_disabled(bool disabled)
{
    _console_log_enabled = !disabled;
    refresh_logger();
}

void set_log_file(const string &log_file_path)
{
    _log_file_path = log_file_path;
    refresh_logger();
}

/*
Custom StreamChannel class that uses separate stringstream objects as the backing streams
input and output and allows the buffers to be easily set (for input) and extracted (for output).
*/
class StringStreamChannel : public network::StreamChannel
{
public:
    StringStreamChannel() : network::StreamChannel(_in_stream, _out_stream) {}

    // Sets the input buffer to hold a given string. The read/get-position is set to beginning.
    void set_in_buffer(const string &str)
    {
        _in_stream.str(str);
        _in_stream.seekg(0);
    }

    // Returns the value in the output buffer as a string. The write/put-position is set to beginning.
    string extract_out_buffer()
    {
        string str(_out_stream.str());
        _out_stream.seekp(0);
        return str;
    }

private:
    stringstream _in_stream;
    stringstream _out_stream;
};

class APSIClient
{
public:
    APSIClient(string &params_json) : _receiver(PSIParams::Load(params_json)) {}

    // TODO: use std::vector<str> in conjunction with "#include <pybind11/stl.h>" for auto conversion
    py::bytes oprf_request(const py::list &input_items)
    {
        vector<Item> receiver_items;
        for (py::handle item : input_items) {
            receiver_items.push_back(item.cast<std::string>());
        }

        if (receiver_items.size() > oprf::oprf_query_count_max)
        {
            throw invalid_argument(
                "a single query cannot hold more than " + to_string(oprf::oprf_query_count_max) +
                " items");
        }

        _oprf_receiver = Receiver::CreateOPRFReceiver(receiver_items);
        Request request = Receiver::CreateOPRFRequest(_oprf_receiver);

        _channel.send(move(request));
        return py::bytes(_channel.extract_out_buffer());
    }

    py::bytes build_query(const string &oprf_response_string)
    {
        _channel.set_in_buffer(oprf_response_string);
        OPRFResponse oprf_response = to_oprf_response(_channel.receive_response());
        if (!oprf_response)
        {
            throw runtime_error("Failed reading the OPRF response");
        }
        auto hashes_and_label_keys = Receiver::ExtractHashes(oprf_response, _oprf_receiver);
        _hashed_recv_items = move(hashes_and_label_keys.first);
        _label_keys = move(hashes_and_label_keys.second);
        if (_hashed_recv_items.size() != _oprf_receiver.item_count())
        {
            // ExtractHashes reports a response that does not match the request by returning
            // nothing, rather than by throwing
            throw runtime_error("Failed extracting the OPRF hashes from the OPRF response");
        }

        // Since APSI 1.0 every query is expected to carry key material of its own, so that a
        // server cannot tell from the public keys that two queries came from the same client.
        // Receiver::create_query does not rotate the keys itself, because the result of an
        // earlier query is encrypted under the key a rotation discards; a caller sending more
        // than one query has to rotate them while none is in flight, which is here.
        if (_query_created)
        {
            _receiver.reset_keys();
        }

        // Create query and send
        pair<Request, IndexTranslationTable> recv_query = _receiver.create_query(_hashed_recv_items);
        _query_created = true;
        _itt = make_shared<IndexTranslationTable>(move(recv_query.second));

        _channel.send(move(recv_query.first));
        return py::bytes(_channel.extract_out_buffer());
    }

    py::list extract_unlabeled_result_from_query_response(const string &query_response_string)
    {
        py::list matches;
        for (auto const &qr : process_query_response(query_response_string))
            matches.append(qr.found);
        return matches;
    }

    py::list extract_labeled_result_from_query_response(const string &query_response_string)
    {
        py::list labels;
        for (auto const &qr : process_query_response(query_response_string))
            labels.append(qr.label.to_string());
        return labels;
    }

private:
    vector<MatchRecord> process_query_response(const string &query_response_string)
    {
        signal(SIGINT, sigint_handler);

        if (!_itt)
        {
            throw runtime_error("You need to build a query first.");
        }

        _channel.set_in_buffer(query_response_string);
        QueryResponse query_response = to_query_response(_channel.receive_response());
        if (!query_response)
        {
            throw runtime_error("Failed reading the query response");
        }
        uint32_t package_count = query_response->package_count;

        vector<ResultPart> rps;
        while (package_count--)
        {
            // Since APSI 1.0 a channel reports a result part it could not read by returning
            // nullptr, which process_result would then skip silently
            ResultPart rp = _channel.receive_result(_receiver.get_seal_context());
            if (!rp)
            {
                throw runtime_error("Failed reading all result parts of the query response");
            }
            rps.push_back(move(rp));
        }

        return _receiver.process_result(_label_keys, *_itt, rps);
    }

    shared_ptr<IndexTranslationTable> _itt;
    Receiver _receiver;
    oprf::OPRFReceiver _oprf_receiver = oprf::OPRFReceiver(vector<Item>());
    vector<HashedItem> _hashed_recv_items;
    LabelKeyVector _label_keys;
    bool _query_created = false;
    StringStreamChannel _channel;
};

class APSIServer
{
public:
    APSIServer() {}

    void init_db(
        string &params_json, size_t label_byte_count,
        size_t nonce_byte_count, bool compressed)
    {
        auto params = PSIParams::Load(params_json);
        _db = make_shared<SenderDB>(
            params, label_byte_count, nonce_byte_count, compressed);
    }

    void save_db(const string &db_file_path)
    {
        try
        {
            ofstream ofs;
            ofs.open(db_file_path, ios::binary);
            if (!ofs.is_open())
            {
                throw runtime_error("cannot open " + db_file_path + " for writing");
            }
            _db->save(ofs);
            ofs.close();
        }
        catch (const exception &e)
        {
            throw runtime_error(string("Failed saving database: ") + e.what());
        }
    }

    void load_db(const string &db_file_path)
    {
        try
        {
            ifstream ifs;
            ifs.open(db_file_path, ios::binary);
            if (!ifs.is_open())
            {
                throw runtime_error("cannot open " + db_file_path + " for reading");
            }
            auto [data, size] = SenderDB::Load(ifs);
            _db = make_shared<SenderDB>(move(data));
            ifs.close();
        }
        catch (const exception &e)
        {
            // APSI 1.0 rejects a serialized SenderDB that omits a required field, so the
            // reason for a failure is worth passing on
            throw runtime_error(string("Failed loading database: ") + e.what());
        }
    }

    size_t get_label_byte_count() const
    {
        // Reads the value from the database itself, so that it is also correct for a
        // database that was loaded rather than initialized here
        return _db ? _db->get_label_byte_count() : 0;
    }

    void add_item(const string &input_item, const string &input_label)
    {
        Item item(input_item);

        if (input_label.length() > 0)
        {
            vector<unsigned char> label(input_label.begin(), input_label.end());
            _db->insert_or_assign(make_pair(item, label));
        }
        else
        {
            _db->insert_or_assign(item);
        }
    }

    void add_unlabeled_items(const py::list &input_items)
    {
        vector<Item> items;
        for (py::handle item : input_items) {
            items.push_back(item.cast<std::string>());
        }
        _db->insert_or_assign(items);
    }

    py::bytes handle_oprf_request(const string &oprf_request_string)
    {
        _channel.set_in_buffer(oprf_request_string);

        OPRFRequest oprf_request = to_oprf_request(_channel.receive_operation(
            nullptr,
            network::SenderOperationType::sop_oprf));
        if (!oprf_request)
        {
            throw runtime_error("Failed reading the OPRF request");
        }
        Sender::RunOPRF(oprf_request, _db->get_oprf_key(), _channel);
        return py::bytes(_channel.extract_out_buffer());
    }

    py::bytes handle_query(const string &query_string)
    {
        _channel.set_in_buffer(query_string);

        QueryRequest sender_query = to_query_request(_channel.receive_operation(
            _db->get_seal_context(),
            network::SenderOperationType::sop_query));
        if (!sender_query)
        {
            throw runtime_error("Failed reading the query request");
        }
        Query query(move(sender_query), _db);
        if (!query)
        {
            // Since APSI 1.0 a query that the SenderDB parameters no longer describe is
            // rejected rather than answered
            throw runtime_error("The query does not match the parameters of this database");
        }

        Sender::RunQuery(query, _channel);
        return py::bytes(_channel.extract_out_buffer());
    }

private:
    shared_ptr<SenderDB> _db;
    StringStreamChannel _channel;
};

PYBIND11_MODULE(_pyapsi, m)
{
    py::module utils = m.def_submodule("utils", "APSI related utilities.");
    utils.def("_set_log_level", &set_log_level,
              "Set APSI log level.");
    utils.def("_set_console_log_disabled", &set_console_log_disabled,
              "Enable or disable standard out console logging.");
    utils.def("_set_log_file", &set_log_file,
              "Set file for APSI log output.");
    utils.def("_set_thread_count", &ThreadPoolMgr::SetThreadCount,
              "Set thread count for parallelization.");
    utils.def("_get_thread_count", &ThreadPoolMgr::GetThreadCount,
              "Get thread count for parallelization.");

    py::class_<APSIServer>(m, "APSIServer")
        .def(py::init())
        .def("_init_db", &APSIServer::init_db)
        .def("_save_db", &APSIServer::save_db)
        .def("_load_db", &APSIServer::load_db)
        .def("_add_item", &APSIServer::add_item)
        .def("_add_unlabeled_items", &APSIServer::add_unlabeled_items)
        .def("_handle_oprf_request", &APSIServer::handle_oprf_request)
        .def("_handle_query", &APSIServer::handle_query)
        .def_property_readonly("_db_label_byte_count", &APSIServer::get_label_byte_count);
    py::class_<APSIClient>(m, "APSIClient")
        .def(py::init<string &>())
        .def("_oprf_request", &APSIClient::oprf_request)
        .def("_build_query", &APSIClient::build_query)
        .def("_extract_labeled_result_from_query_response",
             &APSIClient::extract_labeled_result_from_query_response)
        .def("_extract_unlabeled_result_from_query_response",
             &APSIClient::extract_unlabeled_result_from_query_response);

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
