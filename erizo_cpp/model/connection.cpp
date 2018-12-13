#include "connection.h"

#include <IceConnection.h>
#include <json/json.h>

#include "common/config.h"

DEFINE_LOGGER(Connection, "Connection");
static Config *config = Config::getInstance();

Connection::Connection(const std::string &id, std::shared_ptr<erizo::ThreadPool> thread_pool, std::shared_ptr<erizo::IOThreadPool> io_thread_pool) : id_(id),
                                                                                                                                                     thread_pool_(thread_pool),
                                                                                                                                                     io_thread_pool_(io_thread_pool),
                                                                                                                                                     init_(false),
                                                                                                                                                     ready_(false),
                                                                                                                                                     is_publisher_(false),
                                                                                                                                                     stream_id_(""),
                                                                                                                                                     reply_to_(""),
                                                                                                                                                     corrid_(0),
                                                                                                                                                     amqp_helper_(nullptr),
                                                                                                                                                     webrtc_connection_(nullptr),
                                                                                                                                                     otm_processor_(nullptr),
                                                                                                                                                     media_stream_(nullptr)

{
}

Connection::~Connection() {}

void Connection::initWebRtcConnection()
{

    std::shared_ptr<erizo::Worker> worker = thread_pool_->getLessUsedWorker();
    std::shared_ptr<erizo::IOWorker> io_worker = io_thread_pool_->getLessUsedIOWorker();

    erizo::IceConfig ice_config;
    ice_config.stun_server = config->stun_server_;
    ice_config.stun_port = config->stun_port_;
    ice_config.min_port = config->min_port_;
    ice_config.max_port = config->max_port_;
    ice_config.should_trickle = config->should_trickle_;
    ice_config.turn_server = config->turn_server_;
    ice_config.turn_port = config->turn_port_;
    ice_config.turn_username = config->turn_username_;
    ice_config.turn_pass = config->turn_password_;
    ice_config.network_interface = config->network_interface_;

    webrtc_connection_ = std::make_shared<erizo::WebRtcConnection>(worker, io_worker, id_, ice_config, config->getRtpMaps(), config->getExpMaps(), this);
}

void Connection::init(const std::string &stream_id,
                      const std::string &stream_label,
                      bool is_publisher,
                      std::shared_ptr<AMQPHelper> amqp_helper,
                      const std::string &reply_to, int corrid)
{
    if (init_)
    {
        ELOG_WARN("Connection duplicate initialize!!!");
        return;
    }

    stream_id_ = stream_id;
    is_publisher_ = is_publisher;
    amqp_helper_ = amqp_helper;
    reply_to_ = reply_to;
    corrid_ = corrid;

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
    id_ = "";
    thread_pool_.reset();
    thread_pool_ = nullptr;
    io_thread_pool_.reset();
    io_thread_pool_ = nullptr;
    init_ = false;
    ready_ = false;
    is_publisher_ = false;
    stream_id_ = "";
    reply_to_ = "";
    corrid_ = 0;
    amqp_helper_.reset();
    amqp_helper_ = nullptr;
    webrtc_connection_.reset();
    webrtc_connection_ = nullptr;
    otm_processor_.reset();
    otm_processor_ = nullptr;
    media_stream_.reset();
    media_stream_ = nullptr;
}

void Connection::notifyEvent(erizo::WebRTCEvent newEvent, const std::string &message, const std::string &stream_id)
{
    ELOG_INFO("Connection:event:%d", newEvent);
    switch (newEvent)
    {
    case erizo::CONN_INITIAL:
    {
        Json::Value reply;
        reply["data"]["type"] = "started";
        reply["corrID"] = corrid_;
        reply["type"] = "callback";
        Json::FastWriter writer;
        std::string msg = writer.write(reply);
        amqp_helper_->addCallback({"rpcExchange", reply_to_, reply_to_, msg});
    }
    break;
    case erizo::CONN_SDP_PROCESSED:
    {
        Json::Value reply;
        reply["data"]["type"] = "answer";
        reply["corrID"] = corrid_;
        reply["type"] = "callback";
        reply["data"]["sdp"] = message;
        Json::FastWriter writer;
        std::string msg = writer.write(reply);
        amqp_helper_->addCallback({"rpcExchange", reply_to_, reply_to_, msg});
    }
    break;
    case erizo::CONN_READY:
    {
        Json::Value reply;
        reply["data"]["type"] = "ready";
        reply["corrID"] = corrid_;
        reply["type"] = "callback";
        Json::FastWriter writer;
        std::string msg = writer.write(reply);
        ready_ = true;
        amqp_helper_->addCallback({"rpcExchange", reply_to_, reply_to_, msg});
    }
    break;
    default:
        break;
    }
};

bool Connection::setRemoteSdp(const std::string &sdp)
{
    if (!init_)
    {
        ELOG_WARN("Could't setRemoteSdp before connection initialize");
        return false;
    }
    return webrtc_connection_->setRemoteSdp(sdp, stream_id_);
}

bool Connection::addRemoteCandidate(const std::string &mid, int sdp_mine_index, const std::string &sdp)
{
    if (!init_)
    {
        ELOG_WARN("Could't addRemoteCandidate before connection initialize");
        return false;
    }
    return webrtc_connection_->addRemoteCandidate(mid, sdp_mine_index, sdp);
}

void Connection::addSubscriber(const std::string &client_id, std::shared_ptr<erizo::MediaStream> media_stream)
{
    if (!init_)
    {
        ELOG_WARN("Could't addSubscirber before connection initialize");
        return;
    }
    if (!is_publisher_)
    {
        ELOG_WARN("Only publisher can addSubscriber");
        return;
    }

    std::string subscribe_stream_id = client_id + stream_id_;
    otm_processor_->addSubscriber(media_stream, subscribe_stream_id);
}

bool Connection::isReadyToSubscribe()
{
    return ready_ && is_publisher_;
}

std::shared_ptr<erizo::MediaStream> Connection::getMediaStream()
{
    return media_stream_;
}