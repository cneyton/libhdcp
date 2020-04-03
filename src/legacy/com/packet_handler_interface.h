#ifndef PACKETHANDLERINTERFACE_H
#define PACKETHANDLERINTERFACE_H

#include <cstdint>
#include <string>
#include <vector>
#include <gsl/gsl>
#include "data/BE8RawData.h"
#include <common/be8-packet.h>

class Packet;

/*
 * @brief The PacketHandlerInterface class
 *
 */
class PacketHandlerInterface
{
 public:
    virtual int HandleBE8PeriodicData(const BE8_PeriodicData_Mem_t &fixedData,
                                      const std::vector<BE8_Sensors_t> &sensorsValues,
                                      const BE8PriData&  priValues,
                                      const std::vector<BE8_Event_t>& events)
    {
        return 0;
    }

    virtual int HandleBE8Text(const std::string & text)
    {
        return 0;
    }

    virtual int HandleBE8SequenceConfiguration(const BE8_SequenceConfiguration_t & seqConf)
    {
        return 0;
    }

    virtual int HandleBE8DeviceConfiguration(const BE8_DeviceConfiguration_t & devConf)
    {
        return 0;
    }

    virtual int HandleBE8DeviceInformation(const BE8_DeviceInformation_t &devinfo)
    {
        return 0;
    }

    virtual int HandleBE8DeviceStatus(const BE8_Status_t & status)
    {
        return 0;
    }

    virtual int HandleBE8FrontendConfiguration(const BE8_AFEConfiguration_t & conf)
    {
        return 0;
    }

    virtual int HandleBE8FrontendTGC(const BE8_SequenceTGCLaw_t & conf)
    {
        return 0;
    }

    virtual int HandleBE8FrontendVectorProfiles(const BE8_FrontendVectorProfile_t & conf)
    {
        return 0;
    }

    virtual int HandleBE8FrontendCoefficients(const BE8_FrontendCoefficients_t & conf)
    {
        return 0;
    }


    virtual int HandleBE8RegisterConfiguration(const BE8_RegisterConfiguration_t &val)
    {
        return 0;
    }

    virtual int HandleBE8Ping()
    {
        return 0;
    }

    virtual int HandleBE8Pong()
    {
        return 0;
    }

    //TODO: to move to data_handler and use a type for wave id
    virtual int handle_wave(uint8_t wave_id, gsl::span<const uint8_t> span)
    {
        return 0;
    }
};

#endif // PACKETHANDLERINTERFACE_H
