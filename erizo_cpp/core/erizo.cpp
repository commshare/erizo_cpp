#include "erizo.h"
#include "common/config.h"

DEFINE_LOGGER(Erizo, "Erizo");

Erizo::Erizo() : amqp_uniquecast_(nullptr),
                 thread_pool_(nullptr),
                 io_thread_pool_(nullptr),
                 agent_id_(""),
                 erizo_id_(""),
                 init_(false)
{
}

Erizo::~Erizo() {}

void Erizo::onEvent(const std::string &reply_to, const std::string &msg)
{
    if (!init_)
        return;
    amqp_uniquecast_->addCallback(reply_to, reply_to, msg);
}

int Erizo::init(const std::string &agent_id, const std::string &erizo_id)
{
    if (init_)
        return 0;

    agent_id_ = agent_id;
    erizo_id_ = erizo_id;

    io_thread_pool_ = std::make_shared<erizo::IOThreadPool>(Config::getInstance()->erizo_io_worker_num_);
    io_thread_pool_->start();

    thread_pool_ = std::make_shared<erizo::ThreadPool>(Config::getInstance()->erizo_worker_num_);
    thread_pool_->start();

    std::string uniquecast_binding_key_ = "Erizo_" + erizo_id_;
    amqp_uniquecast_ = std::make_shared<AMQPHelper>();
    if (amqp_uniquecast_->init(Config::getInstance()->uniquecast_exchange_, uniquecast_binding_key_, [&](const std::string &msg) {
            Json::Value root;
            Json::Reader reader;
            if (!reader.parse(msg, root))
            {
                ELOG_ERROR("message parse failed");
                return;
            }

            if (!root.isMember("corrID") ||
                root["corrID"].type() != Json::intValue ||
                !root.isMember("replyTo") ||
                root["replyTo"].type() != Json::stringValue ||
                !root.isMember("data") ||
                root["data"].type() != Json::objectValue)
            {
                ELOG_ERROR("message header invaild");
                return;
            }

            int corrid = root["corrID"].asInt();
            std::string reply_to = root["replyTo"].asString();
            Json::Value data = root["data"];
            if (!data.isMember("method") ||
                data["method"].type() != Json::stringValue)
            {
                ELOG_ERROR("message data invaild");
                return;
            }

            Json::Value reply_data = Json::nullValue;
            std::string method = data["method"].asString();
            if (!method.compare("addPublisher"))
            {
                reply_data = addPublisher(data);
            }
            else if (!method.compare("addSubscriber"))
            {
                reply_data = addSubscriber(data);
            }
            else if (!method.compare("processSignaling"))
            {
                reply_data = processSignaling(data);
            }

            if (reply_data == Json::nullValue)
            {
                ELOG_ERROR("handle message failed,dump-->%s", msg);
                return;
            }

            Json::Value reply;
            reply["corrID"] = corrid;
            reply["data"] = reply_data;
            Json::FastWriter writer;
            std::string reply_msg = writer.write(reply);
            amqp_uniquecast_->addCallback(reply_to, reply_to, reply_msg);
        }))
    {
        ELOG_ERROR("amqp uniquecast init failed");
        return 1;
    }

    init_ = true;
    return 0;
}

Json::Value Erizo::addSubscriber(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
        return Json::nullValue;

    if (root["args"].size() < 4)
        return Json::nullValue;

    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue ||
        args[2].type() != Json::stringValue ||
        args[3].type() != Json::stringValue)
        return Json::nullValue;

    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();
    std::string stream_label = args[2].asString();
    std::string reply_to = args[3].asString();

    std::shared_ptr<Client> client = getOrCreateClient(client_id);
    std::shared_ptr<Connection> pub_conn = getPublisher(stream_id);
    if (pub_conn == nullptr)
        return Json::nullValue;

    std::shared_ptr<Connection> sub_conn = std::make_shared<Connection>();
    sub_conn->setConnectionListener(this);
    sub_conn->init(agent_id_, erizo_id_, client_id, stream_id, stream_label, false, reply_to, thread_pool_, io_thread_pool_);

    pub_conn->addSubscriber(client_id, sub_conn->getMediaStream());
    client->subscribers.push_back(sub_conn);

    Json::Value data;
    data["ret"] = 0;
    return data;
}

Json::Value Erizo::addPublisher(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
        return Json::nullValue;

    if (root["args"].size() < 4)
        return Json::nullValue;

    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue ||
        args[2].type() != Json::stringValue ||
        args[3].type() != Json::stringValue)
        return Json::nullValue;

    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();
    std::string label = args[2].asString();
    std::string reply_to = args[3].asString();

    std::shared_ptr<Client> client = getOrCreateClient(client_id);
    std::shared_ptr<Connection> conn = std::make_shared<Connection>();
    conn->setConnectionListener(this);
    conn->init(agent_id_, erizo_id_, client_id, stream_id, label, true, reply_to, thread_pool_, io_thread_pool_);
    client->publishers.push_back(conn);

    Json::Value data;
    data["ret"] = 0;
    return data;
}

void Erizo::close()
{
    if (!init_)
        return;

    amqp_uniquecast_->close();
    amqp_uniquecast_.reset();
    amqp_uniquecast_ = nullptr;
    thread_pool_->close();
    thread_pool_.reset();
    thread_pool_ = nullptr;
    io_thread_pool_->close();
    io_thread_pool_.reset();
    io_thread_pool_ = nullptr;

    init_ = false;
    agent_id_ = "";
    erizo_id_ = "";
}

Json::Value Erizo::processSignaling(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
        return Json::nullValue;

    if (root["args"].size() < 3)
        return Json::nullValue;

    Json::Value args = root["args"];
    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();
    Json::Value msg = args[2];

    std::shared_ptr<Client> client = getClient(client_id);
    if (client == nullptr)
        return Json::nullValue;

    std::shared_ptr<Connection> conn = client->getConnection(stream_id);
    if (conn == nullptr)
        return Json::nullValue;

    if (!msg.isMember("type") ||
        msg["type"].type() != Json::stringValue)
        return Json::nullValue;

    std::string type = msg["type"].asString();
    if (!type.compare("offer"))
    {
        if (!msg.isMember("sdp") ||
            msg["sdp"].type() != Json::stringValue)
            return Json::nullValue;

        std::string sdp = msg["sdp"].asString();
        conn->setRemoteSdp(sdp);
    }
    else if (!type.compare("candidate"))
    {
        if (!msg.isMember("candidate") ||
            msg["candidate"].type() != Json::objectValue)
            return Json::nullValue;

        Json::Value candidate = msg["candidate"];
        if (!candidate.isMember("sdpMLineIndex") ||
            candidate["sdpMLineIndex"].type() != Json::uintValue ||
            !candidate.isMember("sdpMid") ||
            candidate["sdpMid"].type() != Json::stringValue ||
            !candidate.isMember("candidate") ||
            candidate["candidate"].type() != Json::stringValue)
        {
            int sdp_mine_index = candidate["sdpMLineIndex"].asInt();
            std::string mid = candidate["sdpMid"].asString();
            std::string candidate_str = candidate["candidate"].asString();
            conn->addRemoteCandidate(mid, sdp_mine_index, candidate_str);
        }
    }

    Json::Value data;
    data["ret"] = 0;
    return data;
}

std::shared_ptr<Connection> Erizo::getPublisher(const std::string &stream_id)
{
    for (auto it = clients_.begin(); it != clients_.end(); it++)
    {
        std::vector<std::shared_ptr<Connection>> &publishers = it->second->publishers;
        auto itc = std::find_if(publishers.begin(), publishers.end(), [&](std::shared_ptr<Connection> connection) {
            if (!connection->getStreamId().compare(stream_id))
                return true;
            return false;
        });
        if (itc != publishers.end())
            return *itc;
    }
    return nullptr;
}

std::shared_ptr<Client> Erizo::getOrCreateClient(const std::string &client_id)
{
    auto it = clients_.find(client_id);
    if (it == clients_.end())
    {
        clients_[client_id] = std::make_shared<Client>();
        clients_[client_id]->id = client_id;
    }
    return clients_[client_id];
}

std::shared_ptr<Client> Erizo::getClient(const std::string &client_id)
{
    auto it = clients_.find(client_id);
    if (it != clients_.end())
        return it->second;
    return nullptr;
}