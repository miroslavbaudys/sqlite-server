//
// Created by Miroslav Baudys on 05/03/2018.
//

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "Config.h"
#include "RequestHandler.h"
#include "Logger.h"
#include "Response.h"

using json = nlohmann::json;

enum GenericErrorCode {
    INVALID_FORMAT = 0,
    NO_COMMAND_SPECIFIED,
    UNKNOWN_COMMAND,
    NO_DATABASE_SPECIFIED,
    ERROR_READING_FROM_CLIENT,
};

std::unique_ptr<IResponse> RequestHandler::handle_request(const std::string &req) {
    json j;
    try {
        j = parse_request(req);
    } catch (nlohmann::detail::parse_error &e) {
        return std::make_unique<Response>(
            json{
                {"generic_error", INVALID_FORMAT},
                {"message", e.what()},
                {"request", req}
            }
        );
    }

    if (j.find("cmd") == j.end()) {
        return std::make_unique<Response>(
            json{
                {"generic_error", NO_COMMAND_SPECIFIED},
                {"request", req}
            }
        );
    }

    const std::string cmd = j["cmd"];
    if (boost::iequals(cmd, "QUERY")) {
        return handle_query(j);
    } else if (boost::iequals(cmd, "LIST")) {
        return handle_list(j);
    } else if (boost::iequals(cmd, "DELETE_DB")) {
        return handle_delete_db(j);
    }

    return std::make_unique<Response>(
        json{
            {"generic_error", UNKNOWN_COMMAND},
            {"request", req}
        }
    );
}

nlohmann::json RequestHandler::parse_request(const std::string &req) {
    auto j = nlohmann::json::parse(req, nullptr, false);
    if (!j.is_discarded()) {
        return j;
    }

    //fix JSON - there is error in SQLiteStudio {cmd:"LIST"}
    static const boost::regex e("(['\"])?([a-zA-Z0-9]+)(['\"])?:");
    std::string result;
    result.reserve(req.size() + 16);
    boost::regex_replace(std::back_inserter(result), req.begin(), req.end(), e, "\"$2\":");
    return nlohmann::json::parse(result);;
}

std::unique_ptr<IResponse> RequestHandler::handle_query(const nlohmann::json &j) {
    if (j.find("db") == j.end()) {
        return std::make_unique<Response>(
            json{
                {"generic_error", NO_DATABASE_SPECIFIED},
                {"request", j}
            }
        );
    }

    if (j.find("query") == j.end()) {
        return std::make_unique<Response>(
            json{
                {"generic_error", ERROR_READING_FROM_CLIENT},
                {"request", j}
            }
        );
    }

    const std::string database_name = j["db"];
    const std::string query = j["query"];

    try {
        const auto database = get_database_connection(database_name);
        const auto statement = database->prepare(query);

        auto columnNames = json::array();
        auto rowsData = json::array();

        const auto columnCount = statement->column_count();
        for (auto i = 0; i < columnCount; ++i) {
            columnNames.emplace_back(statement->column_name(i));
        }

        while (statement->next_row()) {
            auto rowData = json::object();
            for (auto i = 0; i < columnCount; ++i) {
                const auto name = statement->column_name(i);
                const auto type = statement->column_type(i);
                switch (type) {
                    case SQLITE_INTEGER: {
                        rowData[name] = statement->value_int64(i);
                    }
                    break;
                    case SQLITE_FLOAT: {
                        rowData[name] = statement->value_double(i);
                    }
                    break;
                    case SQLITE3_TEXT: {
                        rowData[name] = statement->value_text(i);
                    }
                    break;
                    case SQLITE_BLOB: {
                        const auto hexStr = [](const char *data, const size_t len) -> std::string {
                            std::string s(len * 2, ' ');
                            for (auto j = 0; j < len; ++j) {
                                constexpr char hexmap[] = {
                                    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c',
                                    'd', 'e', 'f'
                                };
                                s[2 * j] = hexmap[(data[j] & 0xF0) >> 4];
                                s[2 * j + 1] = hexmap[data[j] & 0x0F];
                            }
                            return s;
                        };

                        const auto blobSize = statement->value_bytes(i);
                        const auto blobValue = statement->value_blob(i);
                        rowData[name] = "X'" + hexStr(blobValue, static_cast<size_t>(blobSize)) + "'";
                    }
                    break;
                    case 0:
                    case SQLITE_NULL: {
                        rowData[name] = nullptr;
                    }
                    break;
                    default: {
                        LogError("Unknown sqlite3 type: {}\n", type);
                    }
                    break;
                }
            }
            rowsData.emplace_back(rowData);
        }
        return std::make_unique<Response>(
            json{
                {"columns", columnNames},
                {"data", rowsData}
            }
        );
    } catch (const SQLException &e) {
        return std::make_unique<Response>(
            json{
                {"error_code", e.code()},
                {"error_message", e.what()},
                {"query", j}
            }
        );
    }
}

std::unique_ptr<IResponse> RequestHandler::handle_list(const nlohmann::json &j) {
    auto databases = json::array();
    if (boost::filesystem::is_directory(Config::instance().databases_folder)) {
        for (boost::filesystem::directory_iterator itr{Config::instance().databases_folder};
             itr != boost::filesystem::directory_iterator{}; ++itr) {
            if (boost::filesystem::is_regular_file(*itr)) {
                databases.emplace_back(itr->path().filename().string());
            }
        }
    }
    return std::make_unique<Response>(json{{"list", databases}});
}

std::unique_ptr<IResponse> RequestHandler::handle_delete_db(const nlohmann::json &j) {
    if (j.find("db") == j.end()) {
        return std::make_unique<Response>(
            json{
                {"generic_error", NO_DATABASE_SPECIFIED},
                {"request", j}
            }
        );
    }

    const std::string database_name = j["db"];
    const auto database_path = Config::instance().databases_folder / database_name;
    const auto result = boost::filesystem::remove(database_path);
    if (result) {
        m_databases.erase(database_name);
    }
    return std::make_unique<Response>(json{{"result", result ? "ok" : "error"}});
}

//database connections
std::shared_ptr<SQLDatabase> RequestHandler::get_database_connection(const std::string &database_name) {
    const auto db_itr = m_databases.find(database_name);
    if (db_itr != m_databases.end()) {
        return db_itr->second;
    }

    const auto database_path = Config::instance().databases_folder / database_name;
    const auto database = std::make_shared<SQLDatabase>(database_path.string());
    m_databases.emplace(database_name, database);
    return database;
}
