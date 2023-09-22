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
#include <sstream>
#include <numeric>
#include <random>
#include <memory>
#include <iostream>
#include <fstream>
#include <csignal>

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
#include "sender.h"

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

void set_log_level(const string &level)
{
    Log::Level ll;

    if (level == "all" || level == "ALL")
    {
        ll = Log::Level::all;
    }
    else if (level == "debug" || level == "DEBUG")
    {
        ll = Log::Level::debug;
    }
    else if (level == "info" || level == "INFO")
    {
        ll = Log::Level::info;
    }
    else if (level == "warning" || level == "WARNING")
    {
        ll = Log::Level::warning;
    }
    else if (level == "error" || level == "ERROR")
    {
        ll = Log::Level::error;
    }
    else if (level == "off" || level == "OFF")
    {
        ll = Log::Level::off;
    }
    else
    {
        throw invalid_argument("unknown log level");
    }

    Log::SetLogLevel(ll);
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

        _oprf_receiver = Receiver::CreateOPRFReceiver(receiver_items);
        Request request = Receiver::CreateOPRFRequest(_oprf_receiver);

        _channel.send(move(request));
        return py::bytes(_channel.extract_out_buffer());
    }

    py::bytes build_query(const string &oprf_response_string)
    {
        _channel.set_in_buffer(oprf_response_string);
        OPRFResponse oprf_response = to_oprf_response(_channel.receive_response());
        tie(_hashed_recv_items, _label_keys) = Receiver::ExtractHashes(oprf_response, _oprf_receiver);

        // Create query and send
        pair<Request, IndexTranslationTable> recv_query = _receiver.create_query(_hashed_recv_items);
        _itt = make_shared<IndexTranslationTable>(move(recv_query.second));

        _channel.send(move(recv_query.first));
        return py::bytes(_channel.extract_out_buffer());
    }

    py::list extract_unlabeled_result_from_query_response(const string &query_response_string)
    {
        signal(SIGINT, sigint_handler);

        _channel.set_in_buffer(query_response_string);
        QueryResponse query_response = to_query_response(_channel.receive_response());
        uint32_t package_count = query_response->package_count;

        vector<ResultPart> rps;
        while (package_count--)
        {
            rps.push_back(_channel.receive_result(_receiver.get_seal_context()));
        }

        vector<MatchRecord> query_result = _receiver.process_result(_label_keys, *_itt, rps);

        py::list matches;
        for (auto const &qr : query_result)
            matches.append(qr.found);
        return matches;
    }

    py::list extract_labeled_result_from_query_response(const string &query_response_string)
    {
        signal(SIGINT, sigint_handler);

        _channel.set_in_buffer(query_response_string);
        QueryResponse query_response = to_query_response(_channel.receive_response());
        uint32_t package_count = query_response->package_count;

        vector<ResultPart> rps;
        while (package_count--)
        {
            rps.push_back(_channel.receive_result(_receiver.get_seal_context()));
        }

        vector<MatchRecord> query_result = _receiver.process_result(_label_keys, *_itt, rps);

        py::list labels;
        for (auto const &qr : query_result)
            labels.append(qr.label.to_string());
        return labels;
    }

private:
    shared_ptr<IndexTranslationTable> _itt;
    Receiver _receiver;
    oprf::OPRFReceiver _oprf_receiver = oprf::OPRFReceiver(vector<Item>());
    vector<HashedItem> _hashed_recv_items;
    vector<LabelKey> _label_keys;
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
        db_label_byte_count = label_byte_count;
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
            _db->save(ofs);
            ofs.close();
        }
        catch (const exception &e)
        {
            throw runtime_error("Failed saving database");
        }
    }

    void load_db(const string &db_file_path)
    {
        try
        {
            ifstream ifs;
            ifs.open(db_file_path, ios::binary);
            auto [data, size] = SenderDB::Load(ifs);
            _db = make_shared<SenderDB>(move(data));
            ifs.close();
        }
        catch (const exception &e)
        {
            throw runtime_error("Failed loading database");
        }
    }

    void load_csv_db(const string &csv_db_file_path, const string &params_json, 
                    size_t nonce_byte_count, bool compressed)
    {
        try
        {
            _db = try_load_csv_db(csv_db_file_path,params_json, nonce_byte_count, compressed);
        }
        catch(const exception &e)
        {
            throw runtime_error("Failed to load data from a CSV file.");
        }    
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

    void add_labeled_items(const py::iterable &input_items_with_label)
    {
        vector<pair<Item,Label>> items_with_label;
        for (py::handle handler : input_items_with_label){
            py::tuple py_tup = handler.cast<py::tuple>();
            if(py::len(py_tup)!=2){
                throw runtime_error("data error, item_with_label should be a tuple with size 2.");
            }
            string label_str = py_tup[1].cast<string>();
            items_with_label.push_back(make_pair(
                    Item(py_tup[0].cast<string>()), 
                    Label(label_str.begin(), label_str.end())
            ));
        }
        _db->insert_or_assign(items_with_label);
    }

    py::bytes handle_oprf_request(const string &oprf_request_string)
    {
        _channel.set_in_buffer(oprf_request_string);

        OPRFRequest oprf_request2 = to_oprf_request(_channel.receive_operation(
            nullptr,
            network::SenderOperationType::sop_oprf));
        Sender::RunOPRF(oprf_request2, _db->get_oprf_key(), _channel);
        return py::bytes(_channel.extract_out_buffer());
    }

    py::bytes handle_query(const string &query_string)
    {
        _channel.set_in_buffer(query_string);

        QueryRequest sender_query = to_query_request(_channel.receive_operation(
            _db->get_seal_context(),
            network::SenderOperationType::sop_query));
        Query query(move(sender_query), _db);

        Sender::RunQuery(query, _channel);
        return py::bytes(_channel.extract_out_buffer());
    }

public:
    size_t db_label_byte_count;

private:
    shared_ptr<SenderDB> _db;
    StringStreamChannel _channel;
};

PYBIND11_MODULE(_pyapsi, m)
{
    py::module utils = m.def_submodule("utils", "APSI related utilities.");
    utils.def("_set_log_level", &set_log_level,
              "Set APSI log level.");
    utils.def("_set_console_log_disabled", &Log::SetConsoleDisabled,
              "Enable or disable standard out console logging.");
    utils.def("_set_log_file", &Log::SetLogFile,
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
        .def("_load_csv_db", &APSIServer::load_csv_db)
        .def("_add_item", &APSIServer::add_item)
        .def("_add_unlabeled_items", &APSIServer::add_unlabeled_items)
        .def("_add_labeled_items", &APSIServer::add_labeled_items)
        .def("_handle_oprf_request", &APSIServer::handle_oprf_request)
        .def("_handle_query", &APSIServer::handle_query)
        // TODO: use def_property_readonly instead
        .def_readwrite("_db_label_byte_count", &APSIServer::db_label_byte_count);
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
