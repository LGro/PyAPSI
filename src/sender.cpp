#include "sender.h"

using namespace std;
using namespace apsi;
using namespace apsi::oprf;
using namespace apsi::sender;

unique_ptr<CSVReader::DBData> db_data_from_csv(const string &db_file)
{
     CSVReader::DBData db_data;
    try {
        CSVReader reader(db_file);
        tie(db_data, ignore) = reader.read();
    } catch (const exception &ex) {
        APSI_LOG_WARNING("Could not open or read file `" << db_file << "`: " << ex.what());
        return nullptr;
    }

    return make_unique<CSVReader::DBData>(move(db_data));
}

shared_ptr<SenderDB> try_load_csv_db(
    const string &db_file_path,
    const string &params_json, 
    size_t nonce_byte_count, 
    bool compressed)
{
    unique_ptr<PSIParams> params;
    try {
        params = make_unique<PSIParams>(PSIParams::Load(params_json));
    } catch (const exception &ex) {
        APSI_LOG_ERROR("APSI threw an exception creating PSIParams: " << ex.what());
        return nullptr;
    }

    if (!params) {
        // We must have valid parameters given
        APSI_LOG_ERROR("Failed to set PSI parameters");
        return nullptr;
    }

    unique_ptr<CSVReader::DBData> db_data;
    if (db_file_path.empty() || !(db_data = db_data_from_csv(db_file_path))) {
        // Failed to read db file
        APSI_LOG_DEBUG("Failed to load data from a CSV file");
        return nullptr;
    }

    return create_sender_db(
        *db_data, move(params), nonce_byte_count, compressed);
}

shared_ptr<SenderDB> create_sender_db(
    const CSVReader::DBData &db_data, 
    unique_ptr<PSIParams> psi_params, 
    size_t nonce_byte_count, 
    bool compress)
{
    if (!psi_params) {
        APSI_LOG_ERROR("No PSI parameters were given");
        return nullptr;
    }

    shared_ptr<SenderDB> sender_db;
    if (holds_alternative<CSVReader::UnlabeledData>(db_data)) {
        try {
            sender_db = make_shared<SenderDB>(*psi_params, 0, 0, compress);
            sender_db->set_data(get<CSVReader::UnlabeledData>(db_data));

            APSI_LOG_INFO(
                "Created unlabeled SenderDB with " << sender_db->get_item_count() << " items");
        } catch (const exception &ex) {
            APSI_LOG_ERROR("Failed to create SenderDB: " << ex.what());
            return nullptr;
        }
    } else if (holds_alternative<CSVReader::LabeledData>(db_data)) {
        try {
            auto &labeled_db_data = get<CSVReader::LabeledData>(db_data);

            // Find the longest label and use that as label size
            size_t label_byte_count =
                max_element(labeled_db_data.begin(), labeled_db_data.end(), [](auto &a, auto &b) {
                    return a.second.size() < b.second.size();
                })->second.size();

            sender_db =
                make_shared<SenderDB>(*psi_params, label_byte_count, nonce_byte_count, compress);
            sender_db->set_data(labeled_db_data);
            APSI_LOG_INFO(
                "Created labeled SenderDB with " << sender_db->get_item_count() << " items and "
                                                 << label_byte_count << "-byte labels ("
                                                 << nonce_byte_count << "-byte nonces)");
        } catch (const exception &ex) {
            APSI_LOG_ERROR("Failed to create SenderDB: " << ex.what());
            return nullptr;
        }
    } else {
        // Should never reach this point
        APSI_LOG_ERROR("Loaded database is in an invalid state");
        return nullptr;
    }

    if (compress) {
        APSI_LOG_INFO("Using in-memory compression to reduce memory footprint");
    }

    // Read the OPRFKey and strip the SenderDB to reduce memory use , Not NOW
    //oprf_key = sender_db->strip();
    APSI_LOG_INFO("SenderDB packing rate: " << sender_db->get_packing_rate());

    return sender_db;
}
