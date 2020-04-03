#ifndef PACKETFACTORY_H
#define PACKETFACTORY_H

#include <cstdint>

class PacketFactory
{
public:
    PacketFactory();
    int32_t loadPacket(uint32_t msgId);
};



//#include <map>
//#include <string>

//class PacketSerialization{
//    int fromBytes(const uint8_t *bytes, const uint16_t & length);
//    int toBytes(uint8_t *bytes, const uint16_t & length);

//    int fromKeyValues(const std::map<std::string,std::string> &keyval);
//    int toKeyValues(const std::map<std::string,std::string> &keyval);
//};

//class DeviceStatus:public PacketSerialization{
//public:
//    uint8_t m_uAcquisitionState; 			/*!< State of	acquisition */
//    uint8_t m_uPulserThermalProtection; 	/*!< Pulser thermal protection status */
//    uint8_t m_uMCUtemperature; 			/*!< (�C) MCU temperature */
//    uint8_t m_uBATtemperature; 			/*!< (�C) Battery temperature */

//    uint32_t m_uFrameIndex;				/*!< index of frame in acquisition */
//    uint8_t m_uAcquisitionIndex;			/*!< index of acquisition in serie */

//    uint16_t m_uBatteryCapacity;			/*!< Battery capacity value */

//    uint8_t m_uChargePercent; 				/*!< battery charge in % */

//    uint8_t m_uErrorCode;					/*!< Current error code */
//    uint8_t m_uEventCode;					/*!< Current error code */
//};

#endif // PACKETFACTORY_H
