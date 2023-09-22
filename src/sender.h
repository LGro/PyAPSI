#include <iostream>
#include <memory>
#include <apsi/psi_params.h>
#include <apsi/sender_db.h>
#include <apsi/oprf/oprf_sender.h>

#include "csv_reader.h"



std::unique_ptr<CSVReader::DBData> db_data_from_csv(const std::string &db_file);

std::shared_ptr<apsi::sender::SenderDB> try_load_csv_db(
    const std::string &db_file_path,
    const std::string &params_json, 
    size_t nonce_byte_count, 
    bool compressed);

std::shared_ptr<apsi::sender::SenderDB> create_sender_db(
    const CSVReader::DBData &db_data,
    std::unique_ptr<apsi::PSIParams> psi_params,
    size_t nonce_byte_count,
    bool compress);