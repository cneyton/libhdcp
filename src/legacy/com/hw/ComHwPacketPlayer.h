#ifndef COMHWPACKETPLAYER_H
#define COMHWPACKETPLAYER_H

#include "BaseComHw.h"
#include <string>
#include <vector>
#include <stdint.h>
#include <queue>
#include <mutex>

#include <fstream>

class ComHwPacketPlayer: public BaseComHW
{
public:
    ComHwPacketPlayer(const std::string & filename);

    virtual int32_t open();

    virtual int32_t read(uint8_t * bytes, uint32_t length);
    virtual int32_t write(const uint8_t * bytes, uint32_t length);

    virtual void close();

    void Play();
    void Rewind();
    void Pause();
    void SetSpeed(int speed);
    void SetPosition(int pospercent);
    int GetPosition();

//    void setDataToRead(uint8_t * bytes, uint32_t length);
private:

    uint32_t m_uCurrentSpeed;
    bool m_bPlay;
    std::ifstream m_File;
    std::string m_sFilepath;
    std::streampos m_fSize;
    std::queue<std::uint8_t> m_DataToRead;
    std::vector<std::vector<std::uint8_t>> m_DataWritten;

    std::mutex mMutex;
};


#endif // COMHWPACKETPLAYER_H
