#include "erizo.h"
#include "common/config.h"

DEFINE_LOGGER(Erizo, "Erizo");

Erizo::Erizo() : init_(false),
                 id_(""),
                 amqp_broadcast_(nullptr),
                 amqp_uniquecast_(nullptr),
                 thread_pool_(nullptr),
                 io_thread_pool_(nullptr)
{
}

Erizo::~Erizo() {}

std::shared_ptr<Client> Erizo::getOrCreateClient(std::string client_id)
{
    auto it = clients_.find(client_id);
    if (it != clients_.end())
        return clients_[client_id];

    clients_[client_id] = std::make_shared<Client>(client_id, thread_pool_, io_thread_pool_);
    return clients_[client_id];
}

int Erizo::init(const std::string &id)
{
    if (init_)
    {
        ELOG_WARN("Erizo duplicate initialize");
        return 0;
    }

    id_ = id;

    io_thread_pool_ = std::make_shared<erizo::IOThreadPool>(Config::getInstance()->erizo_io_worker_num_);
    io_thread_pool_->start();

    thread_pool_ = std::make_shared<erizo::ThreadPool>(Config::getInstance()->erizo_worker_num_);
    thread_pool_->start();

    amqp_broadcast_ = std::make_shared<AMQPHelper>();
    int res = amqp_broadcast_->init("broadcastExchange", "ErizoJS", [&](const std::string &msg) {
    });
    if (res)
    {
        ELOG_ERROR("AMQP broadcast init failed");
        return 1;
    }

    std::string uniquecast_binding_key_ = "ErizoJS_" + id_;
    amqp_uniquecast_ = std::make_shared<AMQPHelper>();
    res = amqp_uniquecast_->init("rpcExchange", uniquecast_binding_key_, [&](const std::string &msg) {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(msg, root))
            return;
        if (root["method"].isNull() || root["method"].type() != Json::stringValue)
            return;

        std::string method = root["method"].asString();
        if (!method.compare("keepAlive"))
        {
            keepAlive(root);
        }
        else if (!method.compare("addPublisher"))
        {
            addPublisher(root);
        }
        else if (!method.compare("processSignaling"))
        {
            processSignaling(root);
        }
        else if (!method.compare("addSubscriber"))
        {
            addSubscriber(root);
        }
    });
    if (res)
    {
        ELOG_ERROR("AMQP uniquecast init failed");
        return 1;
    }

    init_ = true;

    return 0;
}

void Erizo::close()
{
    if (!init_)
    {
        ELOG_WARN("Erizo didn't initialize,can't close!!!");
        return;
    }

    for (auto it = subscribers_.begin(); it != subscribers_.end(); it++)
    {
        for (auto itc = it->second.begin(); itc != it->second.end(); itc++)
        {
            itc->second->close();
            itc->second.reset();
        }
    }

    for (auto it = publishers_.begin(); it != publishers_.end(); it++)
    {
        it->second->close();
        it->second.reset();
    }

    for (auto it = clients_.begin(); it != clients_.end(); it++)
    {
        it->second.reset();
    }

    amqp_broadcast_->close();
    amqp_uniquecast_->close();
    thread_pool_->close();
    io_thread_pool_->close();

    init_ = false;
    id_ = "";
    amqp_broadcast_.reset();
    amqp_broadcast_ = nullptr;
    amqp_uniquecast_.reset();
    amqp_uniquecast_ = nullptr;
    thread_pool_.reset();
    thread_pool_ = nullptr;
    io_thread_pool_.reset();
    io_thread_pool_ = nullptr;

    clients_.clear();
    publishers_.clear();
    subscribers_.clear();
}

bool Erizo::checkMsgFmt(const Json::Value &root)
{
    if (root["replyTo"].isNull() ||
        root["replyTo"].type() != Json::stringValue ||
        root["corrID"].isNull() ||
        root["corrID"].type() != Json::intValue)
    {
        ELOG_ERROR("message format error");
        return false;
    }
    return true;
}

bool Erizo::checkArgs(const Json::Value &root)
{
    Json::Value args = root["args"];
    if (args.isNull() || args.type() != Json::arrayValue)
    {
        ELOG_ERROR("Request without args");
        return false;
    }

    if (args.size() != 3 ||
        args[0].type() != Json::stringValue ||
        args[1].type() != Json::uintValue ||
        args[2].type() != Json::objectValue)
    {
        ELOG_ERROR("Args format error");
        return false;
    }
    return true;
}

void Erizo::keepAlive(const Json::Value &root)
{
    if (!checkMsgFmt(root))
        return;
    std::string reply_to = root["replyTo"].asString();
    int corrid = root["corrID"].asInt();

    Json::Value reply;
    reply["data"] = true;
    reply["corrID"] = corrid;
    reply["type"] = "callback";
    Json::FastWriter writer;
    std::string msg = writer.write(reply);

    amqp_uniquecast_->addCallback({"rpcExchange", reply_to, reply_to, msg});
}

void Erizo::addPublisher(const Json::Value &root)
{
    if (!checkMsgFmt(root) || !checkArgs(root))
        return;

    std::string reply_to = root["replyTo"].asString();
    int corrid = root["corrID"].asInt();

    Json::Value args = root["args"];
    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();
    Json::Value options = args[2];

    if (options["label"].isNull() || options["label"].type() != Json::stringValue)
    {
        ELOG_ERROR("addPubilsher stream label is null or type wrong");
        return;
    }
    std::string stream_label = options["label"].asString();

    std::shared_ptr<Client> client = getOrCreateClient(client_id);
    std::shared_ptr<Connection> connection = client->createConnection();
    connection->init(stream_id, stream_label, true, amqp_uniquecast_, reply_to, corrid);
    publishers_[stream_id] = connection;

    Json::Value reply;
    reply["data"]["type"] = "initializing";
    reply["corrID"] = corrid;
    reply["type"] = "callback";
    Json::FastWriter writer;
    std::string msg = writer.write(reply);

    amqp_uniquecast_->addCallback({"rpcExchange", reply_to, reply_to, msg});
}

bool Erizo::processPublisherSignaling(const Json::Value &root)
{
    if (!checkMsgFmt(root) || !checkArgs(root))
        return true;

    std::string reply_to = root["replyTo"].asString();

    Json::Value args = root["args"];
    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();
    Json::Value options = args[2];

    auto it = publishers_.find(stream_id);
    if (it != publishers_.end())
    {
        std::shared_ptr<Connection> connection = it->second;
        if (options["type"].isNull() || options["type"].type() != Json::stringValue)
        {
            ELOG_ERROR("processSignaling signaling type error");
            return true;
        }

        std::string signaling_type = options["type"].asString();
        if (!signaling_type.compare("offer"))
        {
            if (options["sdp"].isNull() || options["sdp"].type() != Json::stringValue)
            {
                ELOG_ERROR("processSignaling sdp is null or type wrong");
                return true;
            }

            std::string sdp = options["sdp"].asString();
            if (!connection->setRemoteSdp(sdp))
            {
                ELOG_ERROR("processSignaling sdp parse failed");
                return true;
            }
        }
        else if (!signaling_type.compare("candidate"))
        {
            if (options["candidate"].isNull() || options["candidate"].type() != Json::objectValue)
            {
                ELOG_ERROR("processSignaling candidate is null or type wrong");
                return true;
            }

            Json::Value candidate = options["candidate"];
            if (candidate["sdpMLineIndex"].isNull() ||
                candidate["sdpMLineIndex"].type() != Json::uintValue ||
                candidate["sdpMid"].isNull() ||
                candidate["sdpMid"].type() != Json::stringValue ||
                candidate["candidate"].isNull() ||
                candidate["candidate"].type() != Json::stringValue)
            {
                int sdp_mine_index = candidate["sdpMLineIndex"].asInt();
                std::string mid = candidate["sdpMid"].asString();
                std::string candidate_str = candidate["candidate"].asString();
                if (!connection->addRemoteCandidate(mid, sdp_mine_index, candidate_str))
                {
                    ELOG_ERROR("processSignaling candidate parse failed");
                    return true;
                }
            }
        }
        return true;
    }
    return false;
}

bool Erizo::processSubscirberSignaling(const Json::Value &root)
{

    if (!checkMsgFmt(root) || !checkArgs(root))
        return true;

    std::string reply_to = root["replyTo"].asString();
    Json::Value args = root["args"];
    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();
    Json::Value options = args[2];

    auto subscribers_it = subscribers_.find(client_id);
    if (subscribers_it != subscribers_.end())
    {
        std::map<std::string, std::shared_ptr<Connection>> &subscribe_streams = subscribers_it->second;
        auto subscriber_streams_it = subscribe_streams.find(stream_id);
        if (subscriber_streams_it != subscribe_streams.end())
        {
            std::shared_ptr<Connection> connection = subscriber_streams_it->second;
            if (options["type"].isNull() || options["type"].type() != Json::stringValue)
            {
                ELOG_ERROR("processSignaling signaling type error");
                return true;
            }

            std::string signaling_type = options["type"].asString();
            if (!signaling_type.compare("offer"))
            {
                if (options["sdp"].isNull() || options["sdp"].type() != Json::stringValue)
                {
                    ELOG_ERROR("processSignaling sdp is null or type wrong");
                    return true;
                }

                std::string sdp = options["sdp"].asString();
                if (!connection->setRemoteSdp(sdp))
                {
                    ELOG_ERROR("processSignaling sdp parse failed");
                    return true;
                }
            }
            else if (!signaling_type.compare("candidate"))
            {
                if (options["candidate"].isNull() || options["candidate"].type() != Json::objectValue)
                {
                    ELOG_ERROR("processSignaling candidate is null or type wrong");
                    return true;
                }

                Json::Value candidate = options["candidate"];
                if (candidate["sdpMLineIndex"].isNull() ||
                    candidate["sdpMLineIndex"].type() != Json::uintValue ||
                    candidate["sdpMid"].isNull() ||
                    candidate["sdpMid"].type() != Json::stringValue ||
                    candidate["candidate"].isNull() ||
                    candidate["candidate"].type() != Json::stringValue)
                {
                    int sdp_mine_index = candidate["sdpMLineIndex"].asInt();
                    std::string mid = candidate["sdpMid"].asString();
                    std::string candidate_str = candidate["candidate"].asString();
                    if (!connection->addRemoteCandidate(mid, sdp_mine_index, candidate_str))
                    {
                        ELOG_ERROR("processSignaling candidate parse failed");
                        return true;
                    }
                }
            }
            return true;
        }
    }

    return false;
}

void Erizo::processSignaling(const Json::Value &root)
{
    if (!processSubscirberSignaling(root))
    {
        processPublisherSignaling(root);
    }
}

void Erizo::addSubscriber(const Json::Value &root)
{
    if (!checkMsgFmt(root) || !checkArgs(root))
        return;
    std::string reply_to = root["replyTo"].asString();
    int corrid = root["corrID"].asInt();

    Json::Value args = root["args"];
    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();
    Json::Value options = args[2];

    auto publishers_it = publishers_.find(stream_id);
    if (publishers_it != publishers_.end())
    {
        std::shared_ptr<Connection> publisher_connection = publishers_it->second;
        if (!publisher_connection->isReadyToSubscribe())
        {
            ELOG_ERROR("addSubscriber stream is not ready or stream doesn't a publisher");
            return;
        }

        if (options["label"].isNull() || options["label"].type() != Json::stringValue)
        {
            ELOG_ERROR("addSubscriber stream label is null or type wrong");
            return;
        }

        auto subscribers_it = subscribers_.find(client_id);
        if (subscribers_it != subscribers_.end())
        {
            std::map<std::string, std::shared_ptr<Connection>> &subscribe_streams = subscribers_it->second;
            auto subscriber_streams_it = subscribe_streams.find(stream_id);
            if (subscriber_streams_it != subscribe_streams.end())
            {
                ELOG_WARN("Already subscribe this stream");
                return;
            }
        }

        std::string stream_label = options["label"].asString();

        std::shared_ptr<Client> client = getOrCreateClient(client_id);
        std::shared_ptr<Connection> subscribe_connection = client->createConnection();
        subscribe_connection->init(stream_id, stream_label, false, amqp_uniquecast_, reply_to, corrid);

        publisher_connection->addSubscriber(client_id, subscribe_connection->getMediaStream());

        std::map<std::string, std::shared_ptr<Connection>> &subscribe_streams = subscribers_[client_id];
        subscribe_streams[stream_id] = subscribe_connection;

        Json::Value reply;
        reply["data"]["type"] = "initializing";
        reply["corrID"] = corrid;
        reply["type"] = "callback";
        Json::FastWriter writer;
        std::string msg = writer.write(reply);

        amqp_uniquecast_->addCallback({"rpcExchange", reply_to, reply_to, msg});
    }
}