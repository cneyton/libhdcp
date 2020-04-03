#include "ComHwSoftware.h"

ComHwSoftware::ComHwSoftware()
{

}

int32_t ComHwSoftware::open(){

    return 0;
}

int32_t ComHwSoftware::read(uint8_t * bytes, uint32_t length){
    int index=0;
    while (m_DataToRead.size()>0 && length!=index ){
        bytes[index++]=m_DataToRead.front();
        m_DataToRead.pop();
    }

    return index;
}

int32_t ComHwSoftware::write(const uint8_t * bytes, uint32_t length){

    return 0;
}

void ComHwSoftware::close(){


}


void ComHwSoftware::setDataToRead(uint8_t * bytes, uint32_t length){
    for (int i=0;i<length;i++){
        m_DataToRead.push(bytes[i]);
    }
}

void ComHwSoftware::getLastDataWritten(uint8_t * bytes, uint32_t length){

}




