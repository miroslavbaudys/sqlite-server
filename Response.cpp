//
// Created by Miroslav Baudys on 05/03/2018.
//

#include <fmt/format.h>
#include "Response.h"

Response::Response(const nlohmann::json &json) {
    /*
     * 4 bytes data size
     * xx bytes data
     */
    const auto response = json.dump();
    const auto size = static_cast<HeaderSize>(response.size());
    m_data.resize(sizeof(size) + size);
    memcpy(&m_data[0], &size, sizeof(size));
    memcpy(&m_data[sizeof(size)], response.data(), response.size());
}

std::string_view Response::data_repr() const noexcept {
    constexpr auto headerSize = sizeof(HeaderSize);
    if (m_data.size() < headerSize) {
        return {};
    }
    return {reinterpret_cast<const char *>(&m_data[headerSize]), m_data.size() - headerSize};
}
