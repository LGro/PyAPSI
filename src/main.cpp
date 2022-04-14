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
#include "apsi/log.h"
#include "apsi/zmq/sender_dispatcher.h"
#include "apsi/receiver.h"
#include "apsi/sender.h"
#include "apsi/item.h"
#include "apsi/network/stream_channel.h"
#include "apsi/psi_params.h"
#include "apsi/sender_db.h"
#include "apsi/thread_pool_mgr.h"

using namespace std;
using namespace apsi;
using namespace apsi::receiver;
using namespace apsi::sender;
using namespace apsi::network;
using namespace apsi::util;
using namespace apsi::oprf;
using namespace seal;

namespace py = pybind11;

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

void sigint_handler(int param [[maybe_unused]])
{
    exit(0);
}

void print_intersection_results(const vector<Item> &items, const vector<MatchRecord> &intersection)
{
    for (size_t i = 0; i < intersection.size(); i++)
    {
        std::cout << "Processing intersection " << i << std::endl;
        if (intersection[i].found)
        {
            std::cout << "Found!" << std::endl;
            if (intersection[i].label)
            {
                std::cout << "Label: " << intersection[i].label.to_string() << std::endl;
            }
        }
    }
}

class APSIClient
{
public:
    APSIClient(string &params_json) : _receiver(PSIParams::Load(params_json)) {}

    int query(string &conn_addr, string &input_item, size_t thread_count = 1)
    {
        signal(SIGINT, sigint_handler);

        Item item;
        item = input_item;
        vector<Item> recv_items;
        recv_items.push_back(item);

        // Connect to the network
        ZMQReceiverChannel channel;

        std::cout << "Connecting to " << conn_addr << std::endl;
        channel.connect(conn_addr);
        if (channel.is_connected())
        {
            std::cout << "Successfully connected to " << conn_addr << std::endl;
        }
        else
        {
            std::cout << "Failed to connect to " << conn_addr << std::endl;
            return -1;
        }

        unique_ptr<PSIParams> params;
        try
        {
            std::cout << "Sending parameter request" << std::endl;
            params = make_unique<PSIParams>(Receiver::RequestParams(channel));
            std::cout << "Received valid parameters" << std::endl;
        }
        catch (const exception &ex)
        {
            std::cout << "Failed to receive valid parameters: " << ex.what() << std::endl;
            return -1;
        }

        ThreadPoolMgr::SetThreadCount(thread_count);
        std::cout << "Setting thread count to " << ThreadPoolMgr::GetThreadCount() << std::endl;

        Receiver receiver(*params);

        vector<HashedItem> oprf_items;
        vector<LabelKey> label_keys;
        try
        {
            std::cout << "Sending OPRF request for " << recv_items.size() << " items" << std::endl;
            tie(oprf_items, label_keys) = Receiver::RequestOPRF(recv_items, channel);
            std::cout << "Received OPRF request" << std::endl;
        }
        catch (const exception &ex)
        {
            std::cout << "OPRF request failed: " << ex.what() << std::endl;
            return -1;
        }

        vector<MatchRecord> query_result;
        try
        {
            std::cout << "Sending APSI query" << std::endl;
            query_result = receiver.request_query(oprf_items, label_keys, channel);
            std::cout << "Received APSI query response" << std::endl;
        }
        catch (const exception &ex)
        {
            std::cout << "Failed sending APSI query: " << ex.what() << std::endl;
            return -1;
        }

        print_intersection_results(recv_items, query_result);

        return 0;
    }

    py::bytes oprf_request(string &input_item)
    {
        Item item;
        item = input_item;
        vector<Item> receiver_items;
        receiver_items.push_back(item);

        stringstream channel_stream;
        network::StreamChannel channel(channel_stream);

        _oprf_receiver = Receiver::CreateOPRFReceiver(receiver_items);
        Request request = Receiver::CreateOPRFRequest(_oprf_receiver);

        channel.send(move(request));
        return py::bytes(channel_stream.str());
    }

    py::bytes build_query(string &oprf_response_string)
    {
        stringstream channel_stream;
        network::StreamChannel channel(channel_stream);
        channel_stream << oprf_response_string;
        OPRFResponse oprf_response = to_oprf_response(channel.receive_response());
        tie(_hashed_recv_items, _label_keys) = Receiver::ExtractHashes(oprf_response, _oprf_receiver);

        // Create query and send
        pair<Request, IndexTranslationTable> recv_query = _receiver.create_query(_hashed_recv_items);
        // TODO: Somehow the default constructor of IndexTranslationTable is private(?)
        // so that we can't just create a class attribute here. Or there is another way
        // than doing "IndexTranslationTable _itt;" that I don't know.
        // _itt = move(recv_query.second);

        // Somehow this needs a separate output stream, because otherwise parsing
        // query_request results in nullpointer
        stringstream out_channel_stream;
        network::StreamChannel out_channel(out_channel_stream);
        out_channel.send(move(recv_query.first));
        return py::bytes(out_channel_stream.str());
    }

    py::list extract_result_from_query_response(string &query_response_string)
    {
        signal(SIGINT, sigint_handler);

        py::list labels;

        stringstream channel_stream;
        network::StreamChannel channel(channel_stream);
        channel_stream << query_response_string;
        QueryResponse query_response = to_query_response(channel.receive_response());
        uint32_t package_count = query_response->package_count;

        vector<ResultPart> rps;
        while (package_count--)
        {
            rps.push_back(channel.receive_result(_receiver.get_seal_context()));
        }

        // TODO: See duplication note above
        pair<Request, IndexTranslationTable> recv_query = _receiver.create_query(_hashed_recv_items);
        auto itt = move(recv_query.second);
        vector<MatchRecord> query_result = _receiver.process_result(_label_keys, itt, rps);

        for (auto const &qr : query_result)
            labels.append(qr.label.to_string());

        return labels;
    }

private:
    Receiver _receiver;
    oprf::OPRFReceiver _oprf_receiver = oprf::OPRFReceiver(vector<Item>());
    vector<HashedItem> _hashed_recv_items;
    vector<LabelKey> _label_keys;
};

class APSIServer
{
public:
    APSIServer(size_t thread_count)
    {
        ThreadPoolMgr::SetThreadCount(thread_count);
    }

    void init_db(
        string &params_json, size_t label_byte_count,
        size_t nonce_byte_count, bool compressed)
    {
        auto params = PSIParams::Load(params_json);
        _db = make_shared<SenderDB>(
            params, label_byte_count, nonce_byte_count, compressed);
    }

    void save_db(string &db_file_path)
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
            std::cout << "Failed to save database: " << e.what() << std::endl;
        }
    }

    void load_db(string &db_file_path)
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
            std::cout << "Failed to load database: " << e.what() << std::endl;
        }
    }
    void add_item(string &input_item, string &input_label)
    {
        Item item;
        item = input_item;
        // TODO: does this need to match the label byte count?
        std::vector<unsigned char> label(input_label.begin(), input_label.end());
        _db->insert_or_assign(make_pair(item, label));
    }

    void run(int port)
    {
        signal(SIGINT, sigint_handler);

        atomic<bool> stop = false;
        ZMQSenderDispatcher dispatcher(_db);

        dispatcher.run(stop, port);
    }

    py::bytes handle_oprf_request(string &oprf_request_string)
    {
        stringstream channel_stream;
        network::StreamChannel channel(channel_stream);
        channel_stream << oprf_request_string;
        OPRFRequest oprf_request2 = to_oprf_request(
            channel.receive_operation(nullptr, SenderOperationType::sop_oprf));
        Sender::RunOPRF(oprf_request2, _db->get_oprf_key(), channel);
        return py::bytes(channel_stream.str());
    }

    py::bytes handle_query(string &query_string)
    {
        Log::SetConsoleDisabled(false);
        Log::SetLogLevel(Log::Level::debug);

        stringstream channel_stream;
        channel_stream << query_string;
        network::StreamChannel channel(channel_stream);

        QueryRequest sender_query = to_query_request(
            channel.receive_operation(_db->get_seal_context()));
        Query query(move(sender_query), _db);

        // Somehow a separate output stream is needed, otherwise causes invalid buffer
        stringstream out_channel_stream;
        network::StreamChannel out_channel(out_channel_stream);
        Sender::RunQuery(query, out_channel);
        return py::bytes(out_channel_stream.str());
    }

private:
    shared_ptr<SenderDB> _db;
};

PYBIND11_MODULE(pyapsi, m)
{
    py::class_<APSIServer>(m, "APSIServer")
        .def(py::init<size_t>())
        .def("init_db", &APSIServer::init_db)
        .def("save_db", &APSIServer::save_db)
        .def("load_db", &APSIServer::load_db)
        .def("add_item", &APSIServer::add_item)
        .def("run", &APSIServer::run)
        .def("handle_oprf_request", &APSIServer::handle_oprf_request)
        .def("handle_query", &APSIServer::handle_query);

    py::class_<APSIClient>(m, "APSIClient")
        .def(py::init<string &>())
        .def("query", &APSIClient::query)
        .def("oprf_request", &APSIClient::oprf_request)
        .def("build_query", &APSIClient::build_query)
        .def("extract_result_from_query_response",
             &APSIClient::extract_result_from_query_response);

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
