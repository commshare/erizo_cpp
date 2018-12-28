#include "connection.h"

#include <IceConnection.h>
#include <json/json.h>

#include "common/utils.h"
#include "common/config.h"

DEFINE_LOGGER(Connection, "Connection");

Connection::Connection(std::shared_ptr<erizo::ThreadPool> thread_pool, std::shared_ptr<erizo::IOThreadPool> io_thread_pool) : thread_pool_(thread_pool),
                                                                                                                              io_thread_pool_(io_thread_pool),
                                                                                                                              init_(false),
                                                                                                                              is_publisher_(false),
                                                                                                                              stream_id_(""),
                                                                                                                              reply_to_(""),
                                                                                                                              webrtc_connection_(nullptr),
                                                                                                                              otm_processor_(nullptr),
                                                                                                                              media_stream_(nullptr),
                                                                                                                              listener_(nullptr)

{
}

Connection::~Connection() {}

void Connection::initWebRtcConnection()
{

    std::shared_ptr<erizo::Worker> worker = thread_pool_->getLessUsedWorker();
    std::shared_ptr<erizo::IOWorker> io_worker = io_thread_pool_->getLessUsedIOWorker();

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

    webrtc_connection_ = std::make_shared<erizo::WebRtcConnection>(worker, io_worker, Utils::getUUID(), ice_config, Config::getInstance()->getRtpMaps(), Config::getInstance()->getExpMaps(), this);
}

void Connection::init(const std::string &agent_id,
                      const std::string &erizo_id,
                      const std::string &client_id,
                      const std::string &stream_id,
                      const std::string &stream_label,
                      bool is_publisher,
                      const std::string &reply_to)
{
    if (init_)
    {
        ELOG_WARN("Connection duplicate initialize!!!");
        return;
    }

    agent_id_ = agent_id;
    erizo_id_ = erizo_id;
    client_id_ = client_id;
    stream_id_ = stream_id;
    is_publisher_ = is_publisher;
    reply_to_ = reply_to;

    initWebRtcConnection();
    std::shared_ptr<erizo::Worker> worker = thread_pool_->getLessUsedWorker();
    media_stream_ = std::make_shared<erizo::MediaStream>(worker, webrtc_connection_, stream_id, stream_label, is_publisher_);

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
    {
        ELOG_WARN("Connection didn't initialize,can't close!!!");
        return;
    }
    webrtc_connection_->close();
    if (is_publisher_)
    {
        otm_processor_->close();
    }
    media_stream_->close();
    thread_pool_.reset();
    thread_pool_ = nullptr;
    io_thread_pool_.reset();
    io_thread_pool_ = nullptr;
    init_ = false;
    is_publisher_ = false;
    agent_id_ = "";
    erizo_id_ = "";
    client_id_ = "";
    stream_id_ = "";
    reply_to_ = "";
    webrtc_connection_.reset();
    webrtc_connection_ = nullptr;
    otm_processor_.reset();
    otm_processor_ = nullptr;
    media_stream_.reset();
    media_stream_ = nullptr;
    listener_ = nullptr;
}

void Connection::notifyEvent(erizo::WebRTCEvent newEvent, const std::string &message, const std::string &stream_id)
{
    //      CONN_INITIAL = 101, CONN_STARTED = 102, CONN_GATHERED = 103, CONN_READY = 104, CONN_FINISHED = 105,
    //   CONN_CANDIDATE = 201, CONN_SDP = 202, CONN_SDP_PROCESSED = 203,
    //   CONN_FAILED = 500

    ELOG_INFO("-------->stream:%s    Connection:event:%d", stream_id_, newEvent);
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
            data["type"] = "publisher_answer";
        else
            data["type"] = "subscriber_answer";
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

void Connection::setRemoteSdp(const std::string &sdp)
{
    webrtc_connection_->setRemoteSdp(sdp, stream_id_);
}

void Connection::addRemoteCandidate(const std::string &mid, int sdp_mine_index, const std::string &sdp)
{
    webrtc_connection_->addRemoteCandidate(mid, sdp_mine_index, sdp);
}

void Connection::addSubscriber(const std::string &stream_id, std::shared_ptr<erizo::MediaStream> media_stream)
{
    otm_processor_->addSubscriber(media_stream, stream_id);
}

std::shared_ptr<erizo::MediaStream> Connection::getMediaStream()
{
    return media_stream_;
}