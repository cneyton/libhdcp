#ifndef COMHWSOFTWARE_H
#define COMHWSOFTWARE_H

#include "BaseComHw.h"
#include <string>
#include <vector>
#include <stdint.h>
#include <queue>

class ComHwSoftware: public BaseComHW
{
public:
    ComHwSoftware();

    virtual int32_t open();

    virtual int32_t read(uint8_t * bytes, uint32_t length);
    virtual int32_t write(const uint8_t * bytes, uint32_t length);

    virtual void close();

    void setDataToRead(uint8_t * bytes, uint32_t length);
    void getLastDataWritten(uint8_t * bytes, uint32_t length);
private:

    std::queue<std::uint8_t> m_DataToRead;
    std::vector<std::vector<std::uint8_t>> m_DataWritten;
};


#endif // COMHWSOFTWARE_H
