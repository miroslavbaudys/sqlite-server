//
// Created by Miroslav Baudys on 05/03/2018.
//

#ifndef SQLITE_SERVER_IRESPONSE_H
#define SQLITE_SERVER_IRESPONSE_H

class IResponse
{
protected:
    using ResponseData = std::vector<uint8_t>;

public:
	virtual ~IResponse() = default;
    virtual const ResponseData& data() const noexcept = 0;
    virtual const std::string data_repr() const noexcept { return ""; };
};

#endif
