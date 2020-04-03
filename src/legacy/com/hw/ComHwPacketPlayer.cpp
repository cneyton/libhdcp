#include "ComHwPacketPlayer.h"
#include <iostream>
#include "spdlog/spdlog.h"
#include <chrono>
#include <thread>
#include <unistd.h>


ComHwPacketPlayer::ComHwPacketPlayer(const std::string & filename): m_sFilepath(filename)
{

    m_uCurrentSpeed=10000;
    m_fSize=0;
    m_bPlay=false;
    m_bHWIsActive=false;
}

int32_t ComHwPacketPlayer::open(){
    m_File.open(m_sFilepath, std::ifstream::binary);
    spdlog::get("console")->debug("Reading file {}",m_sFilepath.c_str());
    m_fSize = m_File.tellg();
    m_File.seekg( 0, std::ios::end );
    m_fSize = m_File.tellg() - m_fSize;
    m_File.seekg( 0, std::ios::beg );
    return 0;
}

int32_t ComHwPacketPlayer::read(uint8_t * bytes, uint32_t length){
    std::unique_lock<std::mutex> lock(mMutex);
    if (!m_bPlay)
        return 0;

    if (m_File.is_open()&& !m_File.eof()) {
        if (m_uCurrentSpeed!=0)
            std::this_thread::sleep_for(std::chrono::microseconds(m_uCurrentSpeed));

        m_File.read((char*)bytes,length);
        int32_t cnt=m_File.gcount();
        bytesReceived(cnt);
        return cnt;
    }

    return 0;
}


int32_t ComHwPacketPlayer::write(const uint8_t * bytes, uint32_t length){

    bytesSent(length);
    return length;
}

void ComHwPacketPlayer::close(){
    std::unique_lock<std::mutex> lock(mMutex);
    m_File.close();
}


void ComHwPacketPlayer::Play(){
    std::unique_lock<std::mutex> lock(mMutex);
    m_bPlay=true;
}

void ComHwPacketPlayer::Rewind(){
    Pause();
    SetPosition(0);
}

void ComHwPacketPlayer::Pause(){
    std::unique_lock<std::mutex> lock(mMutex);
    m_bPlay=false;
}

void ComHwPacketPlayer::SetSpeed(int speed){
    std::unique_lock<std::mutex> lock(mMutex);
    m_uCurrentSpeed=1000000-speed;
}

void ComHwPacketPlayer::SetPosition(int pospercent){
    std::unique_lock<std::mutex> lock(mMutex);
    m_File.clear();
    m_File.seekg(m_fSize*pospercent/100, std::ios::beg );
}

int ComHwPacketPlayer::GetPosition(){
    std::unique_lock<std::mutex> lock(mMutex);
    long long offset=m_fSize;
    if (offset==0)
        return 0;

    long long curoff=m_File.tellg();
    if (curoff<0){
        if (m_File.eof())
            return 100;
        else
            return 0;
    }
    return 100.0*(double)(curoff)/(double)(offset);
}


//void ComHwPacketPlayer::setDataToRead(uint8_t * bytes, uint32_t length){
//    for (int i=0;i<length;i++){
//        m_DataToRead.push(bytes[i]);
//    }
//}



