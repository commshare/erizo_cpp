#include "bridge_connection.h"

#include <BridgeIO.h>

BridgeConnection::BridgeConnection() : bridge_media_stream_(nullptr),
                                       otm_processor_(nullptr),
                                       bridge_stream_id_(""),
                                       src_stream_id_(""),
                                       init_(false)
{
}

BridgeConnection::~BridgeConnection() {}

void BridgeConnection::init(const std::string &bridge_stream_id,
                            const std::string &src_stream_id,
                            const std::string &ip,
                            uint16_t port,
                            std::shared_ptr<erizo::IOThreadPool> io_thread_pool,
                            bool is_send,
                            uint32_t video_ssrc,
                            uint32_t audio_ssrc)
{
    if (init_)
        return;

    bridge_stream_id_ = bridge_stream_id;
    src_stream_id_ = src_stream_id;
    is_send_ = is_send;

    printf("add stream:%s\n", bridge_stream_id_.c_str());
    bridge_media_stream_ = std::make_shared<erizo::BridgeMediaStream>();
    std::shared_ptr<erizo::IOWorker> io_worker = io_thread_pool->getLessUsedIOWorker();
    bridge_media_stream_->init(ip, port, bridge_stream_id_, io_worker, !is_send_, video_ssrc, audio_ssrc);

    // bridge_media_stream_ = std::make_shared<erizo::BridgeMediaStream>(ip, port, bridge_stream_id_, video_ssrc, audio_ssrc, is_send_);
    // bridge_media_stream_->init();
    if (!is_send_)
    {
        otm_processor_ = std::make_shared<erizo::OneToManyProcessor>();
        bridge_media_stream_->setAudioSink(otm_processor_.get());
        bridge_media_stream_->setVideoSink(otm_processor_.get());
        bridge_media_stream_->setEventSink(otm_processor_.get());
        otm_processor_->setPublisher(bridge_media_stream_);
    }

    erizo::BridgeIO::getInstance()->addStream(bridge_stream_id_, bridge_media_stream_);
    init_ = true;
}

void BridgeConnection::close()
{
    if (!init_)
        return;
    erizo::BridgeIO::getInstance()->removeStream(bridge_stream_id_);
    if (!is_send_)
    {
        bridge_media_stream_->setAudioSink(nullptr);
        bridge_media_stream_->setVideoSink(nullptr);
        bridge_media_stream_->setEventSink(nullptr);
        otm_processor_->close();
        otm_processor_.reset();
        otm_processor_ = nullptr;
    }
    bridge_media_stream_->uninit();
    bridge_media_stream_.reset();
    bridge_media_stream_ = nullptr;
    init_ = false;

    printf("remove stream:%s\n", bridge_stream_id_.c_str());
}

std::shared_ptr<erizo::BridgeMediaStream> BridgeConnection::getBridgeMediaStream()
{
    return bridge_media_stream_;
}

void BridgeConnection::addSubscriber(const std::string &client_id, std::shared_ptr<erizo::MediaStream> media_stream)
{
    if (otm_processor_ != nullptr)
    {
        std::string subscriber_id = (client_id + "_") + bridge_stream_id_;
        otm_processor_->addSubscriber(media_stream, subscriber_id);
    }
}

void BridgeConnection::removeSubscriber(const std::string &client_id)
{
    if (otm_processor_ != nullptr)
    {
        std::string subscriber_id = (client_id + "_") + bridge_stream_id_;
        otm_processor_->removeSubscriber(subscriber_id);
    }
}