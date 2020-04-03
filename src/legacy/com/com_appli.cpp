#include <chrono>
#include <gsl/gsl>

#include "com_appli.h"
#include "request.h"
#include "base_com_hw.h"
#include <common/be8-packet.h>
#include "packet_handler_interface.h"
#include "request_factory.h"

constexpr std::chrono::milliseconds timeout_connecting = std::chrono::milliseconds(30000);


ComAppli::ComAppli(std::unique_ptr<BaseComHW> com_hw, PacketHandlerInterface * packet_handler,
                   common::Logger logger):
     transport_(this, std::move(com_hw), logger), packet_handler_(packet_handler), logger_(logger),
     statemachine_(logger, "com", states_, state::disconnected)
{
    statemachine_.display_trace();
    response_type_ = packet::type::unknown;
    start_thread(0);
}

ComAppli::~ComAppli()
{
    cancel();

    if (joinable())
        join();

    if (transport_.active())
        transport_.stop();
}

int ComAppli::handler_state_disconnected_()
{
    disconnect_ = false;
    // wait for connection attempt
    std::unique_lock<std::mutex> lk(mutex_run_);
    cv_run_.wait(lk);
    return 0;
}

int ComAppli::handler_state_connecting_()
{
    int ret;

    connection_requested_ = false;

    if (!transport_.active()) {
        ret = transport_.start();
        common_die_zero_flag(logger_, ret, 0, disconnect_, "failed to start transport layer");
    }

    /*
     * Perform handshake
     */
    Request ping = RequestFactory::buildPing();
    queue_packet_(ping.get_packet());

    ret = process_queued_packets_();
    common_die_zero_flag(logger_, ret, 0, disconnect_, "failed to process queued packets");

    ret = transport_.run();
    common_die_zero_flag(logger_, ret, 0, disconnect_, "failed to run transport layer");

    if (auto found = received_map_.find(ping.get_response_type()) != received_map_.end()) {
        connection_established_ = true;
        return 0;
    } else {
        return 1;
    }
}

int ComAppli::handler_state_connected_()
{
    int ret;
    ret = process_queued_packets_();
    common_die_zero_flag(logger_, ret, 0, disconnect_, "failed to process queued packets");

    ret = transport_.run();
    common_die_zero_flag(logger_, ret, 0, disconnect_, "failed to run transport layer");

    return 0;
}

int ComAppli::check_connection_requested_()
{
    return connection_requested_ ? 1:0;
}

int ComAppli::check_connected_()
{
    return connection_established_ ? 1:0;
}

int ComAppli::check_disconnected_()
{
    return disconnect_ ? 1:0;
}

int ComAppli::process_queued_packets_()
{
    if (packet_queue_.empty())
        return 0;

    int ret;

    Packet p = packet_queue_.pop();
    ret = transport_.write_packet(&p);
    common_die_zero(logger_, ret, -1, "failed to write packets by the transport layer");

    return 0;
}

void ComAppli::run()
{
    int ret;

    try {
        while (!is_canceled()) {
            ret = statemachine_.wakeup();
            common_die_zero_void(logger_, ret, "statemachine wakeup error");
        }
    } catch(std::exception& e) {
        //TODO: manage error -> restart statemachine
        log_error(logger_, "device run exception : {}", e.what());
        notify_thread_running(-1);
    }
}

int ComAppli::disconnect()
{
    disconnect_ = true;
    return 0;
}

int ComAppli::connect()
{
    int ret;
    {
        std::unique_lock<std::mutex> lk(mutex_run_);
        connection_requested_ = true;
    }
    cv_run_.notify_all();

    //TODO: better to manage that in connecting state
    ret = statemachine_.wait_for(state::connected, timeout_connecting);
    common_die_zero_flag(logger_, ret, -1, disconnect_, "connection attempt failed");

    return 0;
}

ComAppli::state ComAppli::get_state() const
{
    return statemachine_.get_state();
}

int ComAppli::SendRequestSync(const Request& request, int msTimeout, Packet& returnPacket)
{
    if (request.get_response_type() != packet::type::unknown)
        received_map_.erase(request.get_response_type());

    queue_packet_(request.get_packet());
    response_type_ = request.get_response_type();

    if (request.get_response_type() != packet::type::unknown && msTimeout > 0) {
        if ( wait_response(msTimeout, request.get_response_type(), returnPacket) < 0) {
            log_warn(logger_, "timeout");
            return -1;
        } else {
            log_trace(logger_, "received answer");
            return 0;
        }
    }
    return 0;
}

int ComAppli::process_packet(const Packet& packet)
{
    int ret;

    signal_packet(packet.get_type(), packet);

    switch (packet.get_type()) {
    case packet::type::wave:
    {
        gsl::span<const uint8_t> data = packet.get_data();
        log_trace(logger_, "received periodic data");
        // TODO: change with new protocol
        auto it = data.cbegin();
        uint8_t data_id = *it;
        if (data_id != PERIODIC_CONTENT_MASK_PRIDATA)
            break;
        it += sizeof(data_id);
        it += sizeof(BE8_Timestamp_t);

        uint16_t acq_idx;
        std::copy(it, it + sizeof(acq_idx), (uint8_t*)&acq_idx);
        it += sizeof(acq_idx);

        uint16_t size;
        std::copy(it, it + sizeof(size), (uint8_t*)&size);
        size &= 0x03FF;
        it += sizeof(size);

        uint32_t frame_idx;
        std::copy(it, it + sizeof(frame_idx), (uint8_t*)&frame_idx);
        it += sizeof(frame_idx);

        ret = packet_handler_->handle_wave(data_id, gsl::span<const uint8_t>(&*it, size));
        break;
    }
    case packet::type::text:
    {
        auto data = packet.get_data();
        std::string txt(data.begin(), data.end());
        log_debug(logger_, "text received : {}", txt);
        break;
    }
    case packet::type::ack:
    case packet::type::nack:
    case packet::type::ping:
        log_debug(logger_, "not implemented");
        break;
    case packet::type::pong:
        log_debug(logger_, "received pong");
        break;
    case packet::type::seq_config:
        log_trace(logger_, "not impplemented yet");
        break;
    case packet::type::dev_status:
    {
        log_debug(logger_, "received device status");
        if (packet_handler_) {
            BE8_Status_t  status;
            gsl::span<const uint8_t> data = packet.get_data();
            memcpy(&status, data.data(), data.size());
            ret = packet_handler_->HandleBE8DeviceStatus(status);
        }
        break;
    }
    default:
        common_die(logger_, -1, "unknow packet type ({})", packet.get_type());
    }

    return 0;
}


    //case eBE8PeriodicData:
    //{
        //if (packet_handler_) {
            //const uint8_t * data = (const uint8_t*)packet.getBytes();
            //BE8_PeriodicData_Mem_t  fixedData;

            //std::vector<BE8_Sensors_t> sensorsValues;
            //std::vector<BE8_Event_t> events;
            //pri_.clear();
            //pri_.m_RawByteBuffer.clear();

            //uint16_t offset = 0;
            //memcpy(&fixedData, data, sizeof(fixedData));
            //offset += sizeof(fixedData);

            //if (fixedData.contentMask & PERIODIC_CONTENT_MASK_SENSORDATA) {
                //uint8_t chunkCount;
                //memcpy(&chunkCount,&data[offset],1);
                ////TODO handle multiple chunks
                //offset +=1;
                //BE8_Sensors_t sensors;
                //memcpy(&sensors,&data[offset],2);
                //offset +=2;
                //// Read all sensor data
                //for (int i=0;i<sensors.sensorSampleCount;i++){
                    //memcpy(&sensors.sensorSamples[i],&data[offset],sizeof(BE8_SensorSample_t));
                    //offset +=sizeof(BE8_SensorSample_t);
                //}
                //sensorsValues.push_back(sensors);
            //}

            //if (fixedData.contentMask & PERIODIC_CONTENT_MASK_EVENTDATA){
                //log_trace(logger_, "received event data");
                //uint16_t datalen;
                //memcpy(&datalen,&data[offset],2);
                //offset +=2;

                //for (int i=0;i<datalen;i++){
                    //BE8_Event_t sensorval;
                    //memcpy(&sensorval,&data[offset],sizeof(sensorval));
                    //offset+=sizeof(sensorval);

                    //events.push_back(sensorval);
                //}
            //}

            //if (fixedData.contentMask & PERIODIC_CONTENT_MASK_PRIDATA) {
                //int bufferBytesCount;

                //memcpy(&data_chunk_, &data[offset], sizeof(data_chunk_) - sizeof(data_chunk_.priContent.PRIBuffer));

                //bufferBytesCount = data_chunk_.priContent.header.bufferBytesCount;

                ////pri_.m_RawByteBuffer.reserve(data_chunk_.priContent.header.bufferBytesCount + PRI_DATA_HEADER_SIZE + PRI_DATA_CRC_SIZE);
                //std::copy(&data[offset+8], &data[offset+8+data_chunk_.priContent.header.bufferBytesCount+PRI_DATA_HEADER_SIZE+PRI_DATA_CRC_SIZE], back_inserter(pri_.m_RawByteBuffer));

                //offset += sizeof(data_chunk_) - sizeof(data_chunk_.priContent.PRIBuffer);

                //if (bufferBytesCount > 507 * 2)
                    //log_error(logger_, "Size too large {}", bufferBytesCount);

                //memcpy(data_chunk_.priContent.PRIBuffer, &data[offset], bufferBytesCount + 4);
                //offset += bufferBytesCount + 4;

                //pri_.m_uAcquisitionIndex = data_chunk_.acquisitionIndex;
                //pri_.m_uFrameIndex = (data_chunk_.priContent.header.frameID>>16) | (data_chunk_.priContent.header.frameID<<16);
                //pri_.m_sAcquStart = data_chunk_.startAcquTime;
                //pri_.m_CRC=data_chunk_.priContent.PRIBuffer[bufferBytesCount/2];
                //pri_.m_CRC=pri_.m_CRC<<16;
                //pri_.m_CRC|=data_chunk_.priContent.PRIBuffer[bufferBytesCount/2+1];

                ////pri_.reserve(bufferBytesCount/2);
                //std::copy(&data_chunk_.priContent.PRIBuffer[0], &data_chunk_.priContent.PRIBuffer[bufferBytesCount/2], back_inserter(pri_));
            //}
            ////ret = packet_handler_->HandleBE8PeriodicData(fixedData, sensorsValues, pri_,events);
        //}
        //break;
    //}

    //case eBE8SequenceConfiguration:
    //{
        //log_debug(logger_, "received sequence configuration");
        //uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
        //Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
        //transport_.write_packet(response);
        //delete response;
        //if (packet_handler_){
            //BE8_SequenceConfiguration_t theseqconfiguration;
            //memcpy(&theseqconfiguration,packet.getBytes(),packet.getLength());
            //ret=packet_handler_->HandleBE8SequenceConfiguration(theseqconfiguration);
        //}
        //break;
    //}

    //case eBE8DeviceConfiguration:
    //{
        //log_debug(logger_, "received device configuration");
        //uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
        //Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
        //transport_.write_packet(response);
        //delete response;
        //if (packet_handler_){
            //BE8_DeviceConfiguration_t theDevConfiguration;
            //memcpy(&theDevConfiguration,packet.getBytes(),packet.getLength());
           //ret= packet_handler_->HandleBE8DeviceConfiguration(theDevConfiguration);
        //}
        //break;
    //}

    //case eBE8DeviceInformation:
    //{
        //log_debug(logger_, "received device info");
        //uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
        //Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
        //transport_.write_packet(response);
        //delete response;
        //if (packet_handler_){
            //BE8_DeviceInformation_t theDevInformation;
            //memcpy(&theDevInformation,packet.getBytes(),packet.getLength());
            //ret=packet_handler_->HandleBE8DeviceInformation(theDevInformation);
        //}
        //break;
    //}


    //case eBE8FrontendTGC :
    //{
        //log_debug(logger_, "received tgc");
        //uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
        //Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
        //transport_.write_packet(response);
        //delete response;
        //if (packet_handler_){
            //BE8_SequenceTGCLaw_t conf;
            //memcpy(&conf,packet.getBytes(),packet.getLength());
            //ret= packet_handler_->HandleBE8FrontendTGC(conf);
        //}
        //break;
    //}

    //case eBE8FrontendProfiles :
    //{
        //log_debug(logger_, "received afe profile");
        //uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
        //Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
        //transport_.write_packet(response);
        //delete response;
        //if (packet_handler_){
            //BE8_FrontendVectorProfile_t conf;
            //memcpy(&conf,packet.getBytes(),packet.getLength());
            //ret= packet_handler_->HandleBE8FrontendVectorProfiles(conf);
        //}
        //break;
    //}

    //case eBE8FrontendCoefficients:
    //{
        //log_debug(logger_, "received afe coef");
        //uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
        //Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
        //transport_.write_packet(response);
        //delete response;
        //if (packet_handler_){
            //BE8_FrontendCoefficients_t conf;
            //memcpy(&conf,packet.getBytes(),packet.getLength());
            //ret= packet_handler_->HandleBE8FrontendCoefficients(conf);
        //}
        //break;
    //}

    //case eBE8FrontendConfiguration:
    //{
        //log_debug(logger_, "received afe config");
        //uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
        //Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
        //transport_.write_packet(response);
        //delete response;
        //if (packet_handler_){
            //BE8_AFEConfiguration_t conf;
            //memcpy(&conf,packet.getBytes(),packet.getLength());
            //ret= packet_handler_->HandleBE8FrontendConfiguration(conf);
        //}
        //break;
    //}

    //case eBE8RegisterConfiguration:
    //{
        //log_debug(logger_, "received register config");
        //uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
        //Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
        //transport_.write_packet(response);
        //delete response;
        //if (packet_handler_){
            //BE8_RegisterConfiguration_t theRegInfo;
            //if (packet.getLength()>sizeof(theRegInfo)){
                //log_error(logger_, "msg size mismatch");
            //}
            //else{
                //memcpy(&theRegInfo,packet.getBytes(),sizeof(theRegInfo));
                //ret=packet_handler_->HandleBE8RegisterConfiguration(theRegInfo);
            //}
        //}
        //break;
    //}

    //case eBE8Ping:
    //{
        //log_debug(logger_, "received ping");
        //uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
        //Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
        //transport_.write_packet(response);
        //delete response;
        //if (packet_handler_)
            //ret=packet_handler_->HandleBE8Ping();
        //break;
    //}

    //case eBE8Pong:
    //{
        //log_debug(logger_, "received pong");
        //uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
        //Packet * response = new Packet(eACK, 0, SIZE_IDENT, bytes);
        //transport_.write_packet(response);
        //delete response;
        //if (packet_handler_)
            //ret=packet_handler_->HandleBE8Pong();
        //break;
    //}

    //default:
    //{
        //log_debug(logger_, "received unknown message");
        //uint8_t * bytes = new uint8_t[SIZE_IDENT] { packet.getIdentifier() };
        //Packet * response = new Packet(eNACK, 0, SIZE_IDENT, bytes);
        //transport_.write_packet(response);
        //delete response;
    //}
    //}

    //return ret;



int ComAppli::queue_packet_(const Packet& packet)
{
    packet_queue_.push(packet);
    return 0;
}

int ComAppli::wait_response(int ms, packet::type type, Packet& packetAnswer)
{
     std::unique_lock<std::mutex> lock(mutex_);
     if (cv_.wait_for(lock,std::chrono::milliseconds(ms),
                      [this, type, &packetAnswer]()
                      {
                          auto it = received_map_.find(type);
                          bool found = false;
                          if (it != received_map_.end()) {
                              found = true;
                              packetAnswer = it->second;
                          }
                          return found;
                      }
                      )) {
         return 0;
     } else {
         return -1; // Timeout
     }
}

void  ComAppli::signal_packet(packet::type type, const Packet& packet)
{
    std::unique_lock<std::mutex> lock(mutex_);

    received_map_[type] = packet;

    if (response_type_ == type) {
        cv_.notify_all();
        response_type_ = packet::type::unknown;
    }
}

