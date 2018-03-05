//
// Created by Miroslav Baudys on 05/03/2018.
//

#include <fmt/printf.h>
#include "Response.h"

Response::Response(const nlohmann::json& json)
{
    /*
     * 4 bytes data size
     * xx bytes data
     */
    const auto response = json.dump();
    const auto size = (HeaderSize)response.size();
    m_data.resize(sizeof(size) + size);
    memcpy(&m_data[0], &size, sizeof(size));
    memcpy(&m_data[sizeof(size)], response.data(), response.size());

	fmt::print("Response: {}\n", response);
}
