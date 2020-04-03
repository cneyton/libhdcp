#include "PacketFactory.h"
#include <common/be8-packet.h>

PacketFactory::PacketFactory()
{

}


int32_t PacketFactory::loadPacket(uint32_t msgId){

    switch(msgId){
    case eACK:
        break;
    case eNACK:
        break;
    case eBE8PeriodicData:
        break;
    case eBE8PeriodicDataCompressed:
        break;
    case eBE8Text:
        break;
    case eBE8SequenceConfiguration:
        break;
    case eBE8DeviceConfiguration:
        break;
    case eBE8DeviceInformation:
        break;
    case eBE8DeviceStatus:
        break;
    case eBE8FrontendTGC:
        break;
    case eBE8FrontendProfiles:
        break;
    case eBE8FrontendCoefficients:
        break;
    case eBE8FrontendConfiguration:
        break;
    case eBE8RegisterConfiguration:
        break;
    case eBE8Ping:
        break;
    case eBE8Pong:
        break;
    case eCMDPoll:
        break;
    case eCMDWriteSequenceConfiguration:
        break;
    case eCMDControlSequence:
        break;
    case eCMDWriteDeviceConfiguration:
        break;
    case eCMDReadFrontendRegister:
        break;
    case eCMDWriteFrontendRegister:
        break;
    case eCMDWriteFrontendTGC:
        break;
    case eCMDWriteFrontendProfiles:
        break;
    case eCMDWriteFrontendCoefficients:
        break;
    case eCMDWriteFrontendConfiguration:
        break;
    case eCMDPing:
        break;
    case eCMDPong:
        break;
    default:
        break;
    }
    return 0;
}


///**
// * @brief ComAppliBE8::onPacketReceived
// * @param packet
// * @return
// */
//int ComAppliBE8::onPacketReceived(const Packet & packet){
//    int ret=0;

//    if (m_pPacketHandler)
//        m_pPacketHandler->HandlePacket(packet);

//    signalPacket(packet.getType(),packet);
//    // Handle packets
//    // Not a response
//    switch (packet.getType()) {
//    case eACK:
//    case eNACK:
//        //TODO handle
//        break;
//    case eBE8PeriodicDataCompressed:
//        //TODO decompress
//    case eBE8PeriodicData:
//    {
//        uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
//        Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
//        m_Transport.writePacket(response);
//        delete response;

//        if (m_pPacketHandler){
//            spd::get("console")->trace("Packet: Periodic data frame");
//            const uint8_t * data=(uint8_t*)packet.getBytes();
//            BE8_PeriodicData_t  fixedData;

//            std::vector<BE8_Sensors_t> sensorsValues;
//            std::vector<BE8_Event_t> events;

//            uint16_t offset=0;
//            memcpy(&fixedData,data, sizeof(fixedData));
//            offset+=sizeof(fixedData);

//            if (fixedData.contentMask & PERIODIC_CONTENT_MASK_SENSORDATA){
//                uint8_t chunkCount;
//                memcpy(&chunkCount,&data[offset],1);
//                //TODO handle multiple chunks
//                offset +=1;
//                BE8_Sensors_t sensors;
//                memcpy(&sensors,&data[offset],2);
//                offset +=2;
//                // Read all sensor data
//                for (int i=0;i<sensors.sensorSampleCount;i++){
//                    memcpy(&sensors.sensorSamples[i],&data[offset],sizeof(BE8_SensorSample_t));
//                    offset +=sizeof(BE8_SensorSample_t);
//                }
//                sensorsValues.push_back(sensors);
//            }

//            if (fixedData.contentMask & PERIODIC_CONTENT_MASK_EVENTDATA){
//                spd::get("console")->trace("Packet:Event data present");
//                uint16_t datalen;
//                memcpy(&datalen,&data[offset],2);
//                offset +=2;

//                for (int i=0;i<datalen;i++){
//                    BE8_Event_t sensorval;
//                    memcpy(&sensorval,&data[offset],sizeof(sensorval));
//                    offset+=sizeof(sensorval);

//                    events.push_back(sensorval);
//                }
//            }

//            if (fixedData.contentMask & PERIODIC_CONTENT_MASK_PRIDATA){
//                uint16_t datalen;
//                uint32_t frameindex;
//                uint8_t acquisitionIndex;

//                memcpy(&frameindex,&data[offset],sizeof(frameindex));
//                offset +=sizeof(frameindex);

//                memcpy(&acquisitionIndex,&data[offset],sizeof(acquisitionIndex));
//                offset +=sizeof(acquisitionIndex);

//                memcpy(&datalen,&data[offset],sizeof(uint16_t));
//                offset +=2;

//                priValues.m_uAcquisitionIndex=acquisitionIndex;
//                priValues.m_uFrameIndex=frameindex;

//                spd::get("console")->trace("Packet:PRI frame index {}",frameindex);
//                // Read all sensor data
//                for (int i=0;i<datalen;i++){
//                    uint16_t sensorval;
//                    memcpy(&sensorval,&data[offset],sizeof(sensorval));
//                    offset+=sizeof(sensorval);
//                    if (offset> (packet.getLength())){
//                        spd::get("console")->error("Size too large {}",sizeof(sensorval));
//                        return 0;
//                    }
////                    if (i+1!=sensorval)
//                        spd::get("console")->trace("Packet:{}/{}",i+1,sensorval);
//                    priValues.push_back(sensorval);
//                }
//            }
//            ret=m_pPacketHandler->HandleBE8PeriodicData(fixedData,sensorsValues,priValues,events);
//        }
//        break;
//    }
//    case eBE8Text: {
//        std::string msg((char*)packet.getBytes(),packet.getLength());
//        spd::get("console")->info("Text data received : {}",msg);
//        uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
//        Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
//        m_Transport.writePacket(response);
//        delete response;
//        if (m_pPacketHandler){
//            std::string text((char*)packet.getBytes(),packet.getLength());
//           ret= m_pPacketHandler->HandleBE8Text(text);
//        }
//        break;
//    }
//    case eBE8SequenceConfiguration: {
//        spd::get("console")->info("Received sequence configuration");
//        uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
//        Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
//        m_Transport.writePacket(response);
//        delete response;
//        if (m_pPacketHandler){
//            BE8_SequenceConfiguration_t theseqconfiguration;
//            memcpy(&theseqconfiguration,packet.getBytes(),packet.getLength());
//            ret=m_pPacketHandler->HandleBE8SequenceConfiguration(theseqconfiguration);
//        }
//        break;
//    }
//    case eBE8DeviceConfiguration: {
//        spd::get("console")->info("Received device configuration");
//        uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
//        Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
//        m_Transport.writePacket(response);
//        delete response;
//        if (m_pPacketHandler){
//            BE8_DeviceConfiguration_t theDevConfiguration;
//            memcpy(&theDevConfiguration,packet.getBytes(),packet.getLength());
//           ret= m_pPacketHandler->HandleBE8DeviceConfiguration(theDevConfiguration);
//        }
//        break;
//    }
//    case eBE8DeviceInformation: {
//        spd::get("console")->info("Received device information");
//        uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
//        Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
//        m_Transport.writePacket(response);
//        delete response;
//        if (m_pPacketHandler){
//            BE8_DeviceInformation_t theDevInformation;
//            memcpy(&theDevInformation,packet.getBytes(),packet.getLength());
//            ret=m_pPacketHandler->HandleBE8DeviceInformation(theDevInformation);
//        }
//        break;
//    }
//    case eBE8DeviceStatus: {
//        spd::get("console")->info("Received device status");
//        uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
//        Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
//        m_Transport.writePacket(response);
//        delete response;
//        if (m_pPacketHandler){
//            BE8_Status_t  thestatus;
//            memcpy(&thestatus,packet.getBytes(),packet.getLength());
//            ret=m_pPacketHandler->HandleBE8DeviceStatus(thestatus);
//        }
//        break;
//    }
//    case eBE8RegisterConfiguration: {
//        spd::get("console")->info("Received register configuration");
//        uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
//        Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
//        m_Transport.writePacket(response);
//        delete response;
//        if (m_pPacketHandler){
//            BE8_RegisterValueRequest_t theRegInfo;
//            std::vector<uint32_t> values;
//            memcpy(&theRegInfo,packet.getBytes(),sizeof(theRegInfo));
//            for (int i=0;i<theRegInfo.length;i++){
//                uint32_t val;
//                memcpy(&val,&(packet.getBytes()[sizeof(theRegInfo)+i*4]),4);
//                values.push_back(val);
//            }
//            // TODO add read value buffer
//            ret=m_pPacketHandler->HandleBE8RegisterConfiguration(theRegInfo,values);
//        }
//        break;
//    }

//    case eBE8Ping: {
//        spd::get("console")->info("Received ping");
//        uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
//        Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
//        m_Transport.writePacket(response);
//        delete response;
//        if (m_pPacketHandler)
//            ret=m_pPacketHandler->HandleBE8Ping();
//        break;
//    }
//    case eBE8Pong: {
//        spd::get("console")->info("Received pong");
//        uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
//        Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
//        m_Transport.writePacket(response);
//        delete response;
//        if (m_pPacketHandler)
//            ret=m_pPacketHandler->HandleBE8Pong();
//        break;
//    }
//    default:{
//        spd::get("console")->info("Received unknown message");
//        uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
//        Packet * response = new Packet(eNACK, 0, SIZE_IDENT, bytes);
//        m_Transport.writePacket(response);
//        delete response;
//    }
//    }

//    return ret;
//}
