#ifndef REQUEST_FACTORY_H
#define REQUEST_FACTORY_H

#include "request.h"
#include <common/be8-packet.h>


class RequestFactory
{
public:
    RequestFactory();

    static const Request buildAck(uint8_t seqId);
    static const Request buildNack(uint8_t seqId);
    static const Request buildPoll(uint8_t msqId);
    static const Request buildWriteSequenceConfiguration(const BE8_SequenceConfiguration_t& seqConf);
    static const Request buildControlSequence(const BE8_ControlSequence_t&seqCtrl);
    static const Request buildWriteDeviceConfiguration(const BE8_DeviceConfiguration_t &devConf);

    static const Request buildWriteFrontendConfiguration(const BE8_AFEConfiguration_t& frontendConf);
    static const Request buildWriteFrontendTGC(const BE8_SequenceTGCLaw_t &conf);
    static const Request buildWriteFrontendVectorProfile(const BE8_FrontendVectorProfile_t& Conf);
    static const Request buildWriteFrontendCoefficients(const BE8_FrontendCoefficients_t &coef);

    static const Request buildReadFrontendRegister(const BE8_RegisterValueRequest_t & readReq);
    static const Request buildWriteFrontendRegister(const BE8_RegisterWriteRequest_t & writeReq);
    static const Request buildPing();
    static const Request buildPong();

private:
    static uint8_t m_uNextSeqId;
};

#endif // REQUESTFACTORY_H
