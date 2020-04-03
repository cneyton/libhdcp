#include "request_factory.h"


#include <common/be8-packet.h>

uint8_t RequestFactory::m_uNextSeqId=0;

RequestFactory::RequestFactory()
{

}

const Request RequestFactory::buildAck(uint8_t seqId){
    Packet p(eACK,RequestFactory::m_uNextSeqId++,1,&seqId);
    Request req(p,0);

    return req;
}

const Request RequestFactory::buildNack(uint8_t seqId){
    Packet p(eNACK,RequestFactory::m_uNextSeqId++,1,&seqId);
    Request req(p,0);

    return req;
}

const Request RequestFactory::buildPoll(uint8_t msqId){
    Packet p(eCMDPoll,RequestFactory::m_uNextSeqId++,1,&msqId);
    Request req(p,msqId);

    return req;
}

const Request RequestFactory::buildWriteSequenceConfiguration(const BE8_SequenceConfiguration_t& seqConf){
    Packet p(eCMDWriteSequenceConfiguration,RequestFactory::m_uNextSeqId++,sizeof(seqConf),(uint8_t*)&seqConf);
    Request req(p,eACK);

    return req;
}

const Request RequestFactory::buildControlSequence(const BE8_ControlSequence_t&seqCtrl){
    Packet p(eCMDControlSequence,RequestFactory::m_uNextSeqId++,sizeof(seqCtrl),(uint8_t*)&seqCtrl);
    Request req(p,eACK);

    return req;
}

const Request RequestFactory::buildWriteDeviceConfiguration(const BE8_DeviceConfiguration_t &devConf){
    Packet p(eCMDWriteDeviceConfiguration,RequestFactory::m_uNextSeqId++,sizeof(devConf),(uint8_t*)&devConf);
    Request req(p,eACK);

    return req;
}


const Request RequestFactory::buildWriteFrontendConfiguration(const BE8_AFEConfiguration_t& frontendConf){
    Packet p(eCMDWriteFrontendConfiguration,RequestFactory::m_uNextSeqId++,sizeof(BE8_AFEConfiguration_t),(uint8_t*)&frontendConf);
    Request req(p,eACK);

    return req;
}

const Request RequestFactory::buildWriteFrontendTGC(const BE8_SequenceTGCLaw_t &conf){
    Packet p(eCMDWriteFrontendTGC,RequestFactory::m_uNextSeqId++,sizeof(BE8_SequenceTGCLaw_t),(uint8_t*)&conf);
    Request req(p,eACK);

    return req;
}

const Request RequestFactory::buildWriteFrontendVectorProfile(const BE8_FrontendVectorProfile_t& conf){
    Packet p(eCMDWriteFrontendProfiles,RequestFactory::m_uNextSeqId++,sizeof(BE8_FrontendVectorProfile_t),(uint8_t*)&conf);
    Request req(p,eACK);

    return req;
}

const Request RequestFactory::buildWriteFrontendCoefficients(const BE8_FrontendCoefficients_t &coef){
    Packet p(eCMDWriteFrontendCoefficients,RequestFactory::m_uNextSeqId++,sizeof(BE8_FrontendCoefficients_t),(uint8_t*)&coef);
    Request req(p,eACK);

    return req;
}




const Request RequestFactory::buildReadFrontendRegister(const BE8_RegisterValueRequest_t & readReq){
    Packet p(eCMDReadFrontendRegister,RequestFactory::m_uNextSeqId++,sizeof(readReq),(uint8_t*)&readReq);
    Request req(p,eBE8RegisterConfiguration);

    return req;
}

const Request RequestFactory::buildWriteFrontendRegister(const BE8_RegisterWriteRequest_t & writeReq){
    Packet p(eCMDWriteFrontendRegister,RequestFactory::m_uNextSeqId++,sizeof(writeReq),(uint8_t*)&writeReq);
    Request req(p,eACK);

    return req;
}

const Request RequestFactory::buildPing()
{
    Packet p(eCMDPing, RequestFactory::m_uNextSeqId++, 0, NULL);
    Request req(p, eBE8Pong);

    return req;
}

const Request RequestFactory::buildPong(){
    Packet p(eCMDPong,RequestFactory::m_uNextSeqId++,0,NULL);
    Request req(p,eBE8Ping);

    return req;
}

