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
	virtual ~IResponse() {}
    virtual const ResponseData& data() const = 0;
    virtual const std::string data_repr() const { return ""; };
};

#endif
