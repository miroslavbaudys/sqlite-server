//
// Created by Miroslav Baudys on 05/03/2018.
//

#ifndef SQLITE_SERVER_RESPONSE_H
#define SQLITE_SERVER_RESPONSE_H

#include <vector>
#include <nlohmann/json.hpp>
#include "IResponse.h"

class Response final : public IResponse
{
    using HeaderSize = uint32_t;

public:
    explicit Response(const nlohmann::json& json);

	//MARK: IResponse
    inline const ResponseData& data() const override
    {
        return m_data;
    }

private:
    ResponseData m_data;
};


#endif
