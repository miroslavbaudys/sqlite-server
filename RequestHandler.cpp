//
// Created by Miroslav Baudys on 05/03/2018.
//

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <fmt/printf.h>
#include "Config.h"
#include "RequestHandler.h"

using json = nlohmann::json;

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
    }catch(const nlohmann::detail::parse_error&) {
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
	if (j.find("db") == j.end())
	{
		return std::make_unique<Response>(json{{"generic_error", NO_DATABASE_SPECIFIED}, {"request", j}});
	}

	const std::string database_name = j["db"];
	const std::string query = j["query"];

	try
	{
		const auto database = get_database_connection(database_name);
		const auto statement = database->prepare(query);

		auto columnNames = json::array();
		auto rowsData = json::array();

		const auto columnCount = statement->column_count();
		for (auto i = 0; i < columnCount; ++i)
		{
			columnNames.push_back(statement->column_name(i));
		}

		while (statement->next_row())
		{
			auto rowData = json::object();
			for (auto i = 0; i < columnCount; ++i)
			{
				const auto name = statement->column_name(i);
				const auto type = statement->column_type(i);
				const auto value = statement->column_value(i);
				switch (type)
				{
				case SQLITE_INTEGER:
				{
					rowData[name] = statement->value_int64(value);
				}break;
				case SQLITE_FLOAT:
				{
					rowData[name] = statement->value_double(value);
				}break;
				case SQLITE_TEXT:
				{
					rowData[name] = (const char*)statement->value_text(value);
				}break;
				case SQLITE_BLOB:
				{
					const auto hexStr = [](const char* data, const size_t len) -> std::string {
						constexpr char hexmap[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
						std::string s(len * 2, ' ');
						for (auto i = 0; i < len; ++i)
						{
							s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
							s[2 * i + 1] = hexmap[data[i] & 0x0F];
						}
						return s;
					};

					const auto blobSize = statement->value_bytes(value);
					const auto blobValue = statement->value_blob(value);
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
		return std::make_unique<Response>(json{{"columns", columnNames}, {"data", rowsData}});
	}
	catch (const SQLException& e)
	{
		return std::make_unique<Response>(json{{"error_code", e.code()}, {"error_message", e.what()}, {"query", j}});
	}
}

std::unique_ptr<IResponse> RequestHandler::handle_list(const nlohmann::json& j)
{
    auto databases = json::array();
    if(boost::filesystem::is_directory(Config::instance().databases_folder))
    {
        for(boost::filesystem::directory_iterator itr{Config::instance().databases_folder};itr != boost::filesystem::directory_iterator{};++itr)
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

    const std::string database_name = j["db"];
    const auto database_path = Config::instance().databases_folder / database_name;
    const auto result = boost::filesystem::remove(database_path);
	return std::make_unique<Response>(json{{"result", result ? "ok" : "error"}});
}

//database conections
std::shared_ptr<SQLDatabase> RequestHandler::get_database_connection(const std::string& database_name)
{
	const auto db_itr = m_databases.find(database_name);
	if (db_itr != m_databases.end())
	{
		return db_itr->second;
	}

	const auto database_path = Config::instance().databases_folder / database_name;
	const auto database = std::make_shared<SQLDatabase>(database_path.string());
	m_databases.emplace(database_name, database);
	return database;
}