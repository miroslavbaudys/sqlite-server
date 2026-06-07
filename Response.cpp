//
// Created by Miroslav Baudys on 05/03/2018.
//

#include "Response.h"

Response::Response(const nlohmann::json &json) {
    /*
     * 4 bytes data size (little-endian)
     * xx bytes data
     */
    const auto response = json.dump();
    const auto size = static_cast<HeaderSize>(response.size());

    m_data.reserve(sizeof(size) + size);

    // length prefix in fixed little-endian byte order
    for (size_t i = 0; i < sizeof(size); ++i) {
        m_data.push_back(static_cast<uint8_t>(size >> (8 * i) & 0xFF));
    }

    m_data.insert(m_data.end(), response.begin(), response.end());
}

std::string_view Response::data_repr() const noexcept {
    constexpr auto headerSize = sizeof(HeaderSize);
    if (m_data.size() < headerSize) {
        return {};
    }
    return {reinterpret_cast<const char *>(&m_data[headerSize]), m_data.size() - headerSize};
}
