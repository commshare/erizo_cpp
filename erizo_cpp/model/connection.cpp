#include "connection.h"

#include <IceConnection.h>
#include <json/json.h>

#include "common/utils.h"
#include "common/config.h"

DEFINE_LOGGER(Connection, "Connection");

Connection::Connection() : webrtc_connection_(nullptr),
                           otm_processor_(nullptr),
                           media_stream_(nullptr),
                           listener_(nullptr),
                           agent_id_(""),
                           erizo_id_(""),
                           client_id_(""),
                           stream_id_(""),
                           is_publisher_(false),
                           reply_to_(""),
                           thread_pool_(nullptr),
                           io_thread_pool_(nullptr),
                           init_(false)

{
}

Connection::~Connection() {}

void Connection::init(const std::string &agent_id,
                      const std::string &erizo_id,
                      const std::string &client_id,
                      const std::string &stream_id,
                      const std::string &label,
                      bool is_publisher,
                      const std::string &reply_to,
                      std::shared_ptr<erizo::ThreadPool> thread_pool, std::shared_ptr<erizo::IOThreadPool> io_thread_pool)
{
    if (init_)
        return;

    agent_id_ = agent_id;
    erizo_id_ = erizo_id;
    client_id_ = client_id;
    stream_id_ = stream_id;
    is_publisher_ = is_publisher;
    reply_to_ = reply_to;

    thread_pool_ = thread_pool;
    io_thread_pool_ = io_thread_pool;

    std::shared_ptr<erizo::Worker> wc_worker = thread_pool_->getLessUsedWorker();
    std::shared_ptr<erizo::IOWorker> wc_io_worker = io_thread_pool_->getLessUsedIOWorker();

    erizo::IceConfig ice_config;
    ice_config.stun_server = Config::getInstance()->stun_server_;
    ice_config.stun_port = Config::getInstance()->stun_port_;
    ice_config.min_port = Config::getInstance()->min_port_;
    ice_config.max_port = Config::getInstance()->max_port_;
    ice_config.should_trickle = Config::getInstance()->should_trickle_;
    ice_config.turn_server = Config::getInstance()->turn_server_;
    ice_config.turn_port = Config::getInstance()->turn_port_;
    ice_config.turn_username = Config::getInstance()->turn_username_;
    ice_config.turn_pass = Config::getInstance()->turn_password_;
    ice_config.network_interface = Config::getInstance()->network_interface_;

    webrtc_connection_ = std::make_shared<erizo::WebRtcConnection>(wc_worker, wc_io_worker, Utils::getUUID(), ice_config, Config::getInstance()->getRtpMaps(), Config::getInstance()->getExpMaps(), this);

    std::shared_ptr<erizo::Worker> ms_worker = thread_pool_->getLessUsedWorker();
    media_stream_ = std::make_shared<erizo::MediaStream>(ms_worker, webrtc_connection_, stream_id, label, is_publisher_);

    if (is_publisher_)
    {
        otm_processor_ = std::make_shared<erizo::OneToManyProcessor>();
        media_stream_->setAudioSink(otm_processor_.get());
        media_stream_->setVideoSink(otm_processor_.get());
        media_stream_->setEventSink(otm_processor_.get());
        otm_processor_->setPublisher(media_stream_);
    }

    webrtc_connection_->addMediaStream(media_stream_);
    webrtc_connection_->init();
    init_ = true;
}

void Connection::close()
{
    if (!init_)
        return;

    webrtc_connection_->close();
    webrtc_connection_.reset();
    webrtc_connection_ = nullptr;

    if (is_publisher_)
    {
        otm_processor_->close();
        otm_processor_.reset();
        otm_processor_ = nullptr;
    }

    media_stream_->close();
    media_stream_.reset();
    media_stream_ = nullptr;

    listener_ = nullptr;

    agent_id_ = "";
    erizo_id_ = "";
    client_id_ = "";
    stream_id_ = "";
    is_publisher_ = false;
    reply_to_ = "";

    thread_pool_.reset();
    thread_pool_ = nullptr;

    io_thread_pool_.reset();
    io_thread_pool_ = nullptr;

    init_ = false;
}

void Connection::notifyEvent(erizo::WebRTCEvent newEvent, const std::string &message, const std::string &stream_id)
{
    Json::Value data = Json::nullValue;
    switch (newEvent)
    {
    case erizo::CONN_INITIAL:
        data["type"] = "started";
        data["agentId"] = agent_id_;
        data["erizoId"] = erizo_id_;
        data["streamId"] = stream_id_;
        data["clientId"] = client_id_;
        break;
    case erizo::CONN_SDP_PROCESSED:
        if (is_publisher_)
        {
            data["type"] = "publisher_answer";
        }
        else
        {
            data["type"] = "subscriber_answer";
        }
        data["agentId"] = agent_id_;
        data["erizoId"] = erizo_id_;
        data["streamId"] = stream_id_;
        data["clientId"] = client_id_;
        data["sdp"] = message;
        break;
    case erizo::CONN_READY:
        data["type"] = "ready";
        data["agentId"] = agent_id_;
        data["erizoId"] = erizo_id_;
        data["streamId"] = stream_id_;
        data["clientId"] = client_id_;
        break;
    default:
        break;
    }

    if (data.type() != Json::nullValue && listener_ != nullptr)
    {
        Json::FastWriter writer;
        Json::Value reply;
        reply["data"] = data;
        std::string msg = writer.write(reply);
        listener_->onEvent(reply_to_, msg);
    }
};

int Connection::setRemoteSdp(const std::string &sdp)
{
    if (!webrtc_connection_->setRemoteSdp(sdp, stream_id_))
        return 1;
    return 0;
}

int Connection::addRemoteCandidate(const std::string &mid, int sdp_mine_index, const std::string &sdp)
{
    if (!webrtc_connection_->addRemoteCandidate(mid, sdp_mine_index, sdp))
        return 1;
    return 0;
}

void Connection::addSubscriber(const std::string &client_id, std::shared_ptr<erizo::MediaStream> media_stream)
{
    std::string subscriber_id = (client_id + "_") + stream_id_;
    otm_processor_->addSubscriber(media_stream, subscriber_id);
}

std::shared_ptr<erizo::MediaStream> Connection::getMediaStream()
{
    return media_stream_;
}