//
// Created by Miroslav Baudys on 05/03/2018.
//

#ifndef SQLITE_SERVER_RESPONSE_H
#define SQLITE_SERVER_RESPONSE_H

#include <vector>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include "IResponse.h"

class Response final : public IResponse {
    using HeaderSize = uint32_t;

public:
    explicit Response(const nlohmann::json &json);

    //MARK: IResponse
    inline const ResponseData &data() const noexcept override {
        return m_data;
    }

    inline std::string data_repr() const noexcept override {
        return fmt::format("{}", std::string((const char *) &m_data[4], m_data.size() - 4));
    }

private:
    ResponseData m_data;
};


#endif
