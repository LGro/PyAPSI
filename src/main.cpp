/**
 * This file is based on the CLI example from https://github.com/microsoft/apsi which
 * is licensed as follows.
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

// PyBind11
#include <pybind11/pybind11.h>

// APSI
#include "apsi/network/stream_channel.h"
#include "apsi/oprf/oprf_sender.h"
#include "apsi/zmq/sender_dispatcher.h"
#include "apsi/receiver.h"
#include "apsi/sender.h"
#include "apsi/item.h"
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

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

void sigint_handler(int param [[maybe_unused]])
{
    exit(0);
}

// TODO: Compare these (taken from test suite) against the recommended default for the
// APSI repo's CLI example
PSIParams create_params()
{
    PSIParams::ItemParams item_params;
    item_params.felts_per_item = 8;

    PSIParams::TableParams table_params;
    table_params.hash_func_count = 3;
    table_params.max_items_per_bin = 16;
    table_params.table_size = 4096;

    PSIParams::QueryParams query_params;
    query_params.query_powers = {1, 3, 5};

    PSIParams::SEALParams seal_params;
    seal_params.set_poly_modulus_degree(8192);
    seal_params.set_coeff_modulus(CoeffModulus::BFVDefault(8192));
    seal_params.set_plain_modulus(65537);

    return {item_params, table_params, query_params, seal_params};
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
    APSIClient() {}

    int query(string &conn_addr, string &input_item, size_t thread_count = 1)
    {
        // conn_addr could be "tcp://localhost:1234"

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
};

class APSIServer
{
public:
    APSIServer(size_t thread_count)
    {
        ThreadPoolMgr::SetThreadCount(thread_count);
    }

    void init_db()
    {
        // TODO: make these arguments or initialize differently
        const PSIParams &params = create_params();

        size_t label_byte_count = 10;
        size_t nonce_byte_count = 4;

        _db = make_shared<SenderDB>(params, label_byte_count, nonce_byte_count, true);
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

private:
    const PSIParams &_psi_parameters = create_params();
    shared_ptr<SenderDB> _db;
};

namespace py = pybind11;

PYBIND11_MODULE(pyapsi, m)
{
    py::class_<APSIServer>(m, "APSIServer")
        .def(py::init<size_t>())
        .def("init_db", &APSIServer::init_db)
        .def("save_db", &APSIServer::save_db)
        .def("load_db", &APSIServer::load_db)
        .def("add_item", &APSIServer::add_item)
        .def("run", &APSIServer::run);

    py::class_<APSIClient>(m, "APSIClient")
        .def(py::init())
        .def("query", &APSIClient::query);

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
