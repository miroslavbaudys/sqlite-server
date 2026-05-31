//
// Created by Miroslav Baudys on 05/03/2018.
//

#ifndef SQLITE_SERVER_REQUESTHANDLER_H
#define SQLITE_SERVER_REQUESTHANDLER_H

#include <string>
#include <nlohmann/json.hpp>
#include "sqlite3_wrapper/SQLDatabase.h"
#include "Response.h"
#include "IRequestHandler.h"

class RequestHandler final : public IRequestHandler {
public:
    RequestHandler() = default;

    //MARK: IRequestHandler
    std::unique_ptr<IResponse> handle_request(const std::string &req) override;

private:
    nlohmann::json parse_request(const std::string &req) const;

    //handlers
    std::unique_ptr<IResponse> handle_query(const nlohmann::json &j);

    std::unique_ptr<IResponse> handle_list(const nlohmann::json &j);

    std::unique_ptr<IResponse> handle_delete_db(const nlohmann::json &j);

    //database connections
    std::shared_ptr<SQLDatabase> get_database_connection(const std::string &database_name);

private:
    std::map<std::string, std::shared_ptr<SQLDatabase>> m_databases;
};


#endif //SQLITE_SERVER_REQUESTHANDLER_H
