//
// Created by Miroslav Baudys on 05/03/2018.
//

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <fmt/printf.h>
#include "sqlite3/sqlite3.h"
#include "RequestHandler.h"

using json = nlohmann::json;

//TODO: config
const auto databases_folder = "C:\\Users\\Mira\\Documents\\";

enum GenericErrorCode
{
    INVALID_FORMAT = 0,
    NO_COMMAND_SPECIFIED,
    UNKNOWN_COMMAND,
    NO_DATABASE_SPECIFIED
};

RequestHandler::RequestHandler() : 
	m_map{
		{"QUERY", boost::bind(&RequestHandler::handle_query, this, _1)},
		{"LIST", boost::bind(&RequestHandler::handle_list, this, _1)},
		{"DELETE_DB", boost::bind(&RequestHandler::handle_delete_db, this, _1)}
	}
{

}

std::unique_ptr<IResponse> RequestHandler::handle_request(const std::string& req)
{
	fmt::print("Request: {}\n", req);

    json j;
    try {
        j = parse_request(req);
    }catch(nlohmann::detail::parse_error& e) {
		return std::make_unique<Response>(json{{"generic_error", INVALID_FORMAT}, {"message", e.what()}, {"request", req}});
    }

    if(j.find("cmd") == j.end())
    {
        return std::make_unique<Response>(json{{"generic_error", NO_COMMAND_SPECIFIED}, {"request", req}});
    }

    const auto handler = m_map.find(j["cmd"]);
    if(handler == m_map.end())
    {
        return std::make_unique<Response>(json{{"generic_error", UNKNOWN_COMMAND}, {"request", req}});
    }
    return handler->second(j);
}

const json RequestHandler::parse_request(const std::string& req)
{
    json j;
    try {
        j = json::parse(req);
    }catch(nlohmann::detail::parse_error&) {
        //fix JSON - there is error in SQLiteStudio {cmd:"LIST"}
        std::string result;
        boost::regex e("(['\"])?([a-zA-Z0-9]+)(['\"])?:");
        boost::regex_replace(std::back_inserter(result), req.begin(), req.end(), e, "\"$2\":");
        j = json::parse(result);
    }
    return j;
}

std::unique_ptr<IResponse> RequestHandler::handle_query(const nlohmann::json& j)
{
    if(j.find("db") == j.end())
    {
        return std::make_unique<Response>(json{{"generic_error", NO_DATABASE_SPECIFIED}, {"request", j}});
    }

    const std::string database = j["db"];
    const std::string query = j["query"];

    const auto databasePath = std::string(databases_folder) + database;
    sqlite3* db;
    int rc = sqlite3_open(databasePath.c_str(), &db);
    if(rc != SQLITE_OK)
    {
        return std::make_unique<Response>(json{{"error_code", sqlite3_errcode(db)}, {"error_message", sqlite3_errmsg(db)}, {"query", j}});
    }
    else
    {
        sqlite3_stmt* stmt;
        do {
            rc = sqlite3_prepare_v2(db, query.c_str(), (int)query.length(), &stmt, nullptr);
        }while(rc == SQLITE_BUSY);
        if(rc != SQLITE_OK)
        {
            return std::make_unique<Response>(json{{"error_code", sqlite3_errcode(db)}, {"error_message", sqlite3_errmsg(db)}, {"query", j}});
        }
        else
        {
            auto columnNames = json::array();
            auto rowsData = json::array();

            const auto columnCount = sqlite3_column_count(stmt);
            for(auto i = 0;i < columnCount;++i)
            {
                columnNames.push_back(sqlite3_column_name(stmt, i));
            }

            while(sqlite3_step(stmt) == SQLITE_ROW)
            {
                auto rowData = json::object();
                for(auto i = 0;i < columnCount;++i)
                {
                    const auto name = sqlite3_column_name(stmt, i);
                    const auto type = sqlite3_column_type(stmt, i);
                    const auto value = sqlite3_column_value(stmt, i);
                    switch(type)
                    {
                        case SQLITE_INTEGER:
                        {
                            rowData[name] = sqlite3_value_int64(value);
                        }break;
                        case SQLITE_FLOAT:
                        {
                            rowData[name] = sqlite3_value_double(value);
                        }break;
                        case SQLITE_TEXT:
                        {
                            rowData[name] = (const char*)sqlite3_value_text(value);
                        }break;
                        case SQLITE_BLOB:
                        {
                            const auto hexStr = [](const char* data, const size_t len) -> std::string {
                                constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
                                std::string s(len * 2, ' ');
                                for(size_t i = 0; i < len; ++i)
                                {
                                    s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
                                    s[2 * i + 1] = hexmap[data[i] & 0x0F];
                                }
                                return s;
                            };

                            const auto blobSize = sqlite3_value_bytes(value);
                            const auto blobValue = (const char*)sqlite3_value_blob(value);
                            rowData[name] = "X'" + hexStr(blobValue, (size_t)blobSize) + "'";
                        }break;
                        case 0:
                        case SQLITE_NULL:
                        {
                            rowData[name] = nullptr;
                        }break;
                    }
                }
                rowsData.push_back(rowData);
            }

            sqlite3_clear_bindings(stmt);
            sqlite3_finalize(stmt);
            sqlite3_close(db);

            return std::make_unique<Response>(json{{"columns", columnNames}, {"data", rowsData}});
        }
    }
}

std::unique_ptr<IResponse> RequestHandler::handle_list(const nlohmann::json& j)
{
    auto databases = json::array();
    if(boost::filesystem::is_directory(databases_folder))
    {
        boost::filesystem::directory_iterator it{databases_folder};
        for(boost::filesystem::directory_iterator itr{databases_folder};itr != boost::filesystem::directory_iterator{};++itr)
        {
            if(boost::filesystem::is_regular_file(*itr))
            {
                databases.push_back((*itr).path().filename().string());
            }
        }
    }
	return std::make_unique<Response>(json{{"list", databases}});
}

std::unique_ptr<IResponse> RequestHandler::handle_delete_db(const nlohmann::json& j)
{
    if(j.find("db") == j.end())
    {
        return std::make_unique<Response>(json{{"generic_error", NO_DATABASE_SPECIFIED}, {"request", j}});
    }

    const std::string database = j["db"];
    const auto databasePath = std::string(databases_folder) + database;
    const auto result = boost::filesystem::remove(databasePath);
	return std::make_unique<Response>(json{{"result", result ? "ok" : "error"}});
}