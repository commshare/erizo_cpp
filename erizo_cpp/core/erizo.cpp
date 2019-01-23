#include "erizo.h"
#include "common/config.h"

#include <dtls/DtlsSocket.h>
#include <BridgeIO.h>

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

int Erizo::init(const std::string &agent_id, const std::string &erizo_id, const std::string &ip, uint16_t port)
{
    if (init_)
        return 0;

    agent_id_ = agent_id;
    erizo_id_ = erizo_id;

    io_thread_pool_ = std::make_shared<erizo::IOThreadPool>(Config::getInstance()->erizo_io_worker_num_);
    io_thread_pool_->start();

    thread_pool_ = std::make_shared<erizo::ThreadPool>(Config::getInstance()->erizo_worker_num_);
    thread_pool_->start();

    dtls::DtlsSocketContext::globalInit();

    if (erizo::BridgeIO::getInstance()->init(ip, port, Config::getInstance()->bridge_io_worker_num_))
    {
        ELOG_ERROR("bridge-io init failed");
        return 1;
    }

    std::string uniquecast_binding_key_ = erizo_id_;
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
                processSignaling(data);
                return;
            }
            else if (!method.compare("removeSubscriber"))
            {
                reply_data = removeSubscriber(data);
            }
            else if (!method.compare("removePublisher"))
            {
                reply_data = removePublisher(data);
            }
            else if (!method.compare("addVirtualPublisher"))
            {
                reply_data = addVirtualPublisher(data);
            }
            else if (!method.compare("addVirtualSubscriber"))
            {
                reply_data = addVirtualSubscriber(data);
            }

            if (corrid == -1)
                return;

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
    std::shared_ptr<Connection> pub_conn = getPublisherConn(stream_id);
    if (pub_conn != nullptr)
    {
        std::shared_ptr<Connection> sub_conn = std::make_shared<Connection>();
        sub_conn->setConnectionListener(this);
        sub_conn->init(agent_id_, erizo_id_, client_id, stream_id, stream_label, false, reply_to, thread_pool_, io_thread_pool_);

        pub_conn->addSubscriber(client_id, sub_conn->getMediaStream());
        client->subscribers[stream_id] = sub_conn;
    }
    else
    {
        std::shared_ptr<BridgeConnection> bridge_conn = getBridgeConn(stream_id);
        if (bridge_conn != nullptr)
        {
            std::shared_ptr<Connection> sub_conn = std::make_shared<Connection>();
            sub_conn->setConnectionListener(this);
            sub_conn->init(agent_id_, erizo_id_, client_id, stream_id, stream_label, false, reply_to, thread_pool_, io_thread_pool_);

            bridge_conn->addSubscriber(client_id, sub_conn->getMediaStream());
            client->subscribers[stream_id] = sub_conn;
        }
        else
        {
            return Json::nullValue;
        }
    }

    Json::Value data;
    data["ret"] = 0;
    return data;
}

Json::Value Erizo::removeSubscriber(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
        return Json::nullValue;
    if (root["args"].size() < 2)
        return Json::nullValue;

    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue)
        return Json::nullValue;

    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();

    std::shared_ptr<Connection> pub_conn = getPublisherConn(stream_id);
    if (pub_conn != nullptr)
        pub_conn->removeSubscriber(client_id);

    std::shared_ptr<Client> client = getOrCreateClient(client_id);
    std::shared_ptr<Connection> sub_conn = getSubscriberConn(client, stream_id);
    if (sub_conn != nullptr)
    {
        client->subscribers.erase(stream_id);
        sub_conn->close();
    }

    Json::Value data;
    data["ret"] = 0;
    return data;
}

Json::Value Erizo::addPublisher(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
        return Json::nullValue;

    if (root["args"].size() < 5)
        return Json::nullValue;

    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue ||
        args[2].type() != Json::stringValue ||
        args[3].type() != Json::stringValue ||
        args[4].type() != Json::stringValue)
        return Json::nullValue;

    std::string room_id = args[0].asString();
    std::string client_id = args[1].asString();
    std::string stream_id = args[2].asString();
    std::string label = args[3].asString();
    std::string reply_to = args[4].asString();

    std::shared_ptr<Client> client = getOrCreateClient(client_id);
    std::shared_ptr<Connection> conn = std::make_shared<Connection>();
    conn->setConnectionListener(this);
    conn->setRoomId(room_id);
    conn->init(agent_id_, erizo_id_, client_id, stream_id, label, true, reply_to, thread_pool_, io_thread_pool_);
    client->publishers[stream_id] = conn;

    Json::Value data;
    data["ret"] = 0;
    return data;
}

Json::Value Erizo::addVirtualPublisher(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
        return Json::nullValue;
    if (root["args"].size() < 6)
        return Json::nullValue;

    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue ||
        args[2].type() != Json::stringValue ||
        args[3].type() != Json::intValue) //||
                                          //   args[4].type() != Json::intValue ||
                                          //  args[5].type() != Json::intValue)
        return Json::nullValue;

    std::string bridge_stream_id = args[0].asString();
    std::string src_stream_id = args[1].asString();
    std::string ip = args[2].asString();
    uint16_t port = args[3].asInt();
    uint32_t video_ssrc = args[4].asUInt();
    uint32_t audio_ssrc = args[5].asUInt();

    std::shared_ptr<BridgeConnection> bridge_conn = getBridgeConn(src_stream_id);
    if (bridge_conn == nullptr)
    {
        bridge_conn = std::make_shared<BridgeConnection>();
        bridge_conn->init(bridge_stream_id, src_stream_id, ip, port, thread_pool_, false, video_ssrc, audio_ssrc);
        bridge_conns_[src_stream_id] = bridge_conn;
    }

    Json::Value data;
    data["ret"] = 0;
    return data;
}

Json::Value Erizo::addVirtualSubscriber(const Json::Value &root)
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
        args[3].type() != Json::intValue)
        return Json::nullValue;

    std::string bridge_stream_id = args[0].asString();
    std::string src_stream_id = args[1].asString();
    std::string ip = args[2].asString();
    uint16_t port = args[3].asInt();

    std::shared_ptr<BridgeConnection> bridge_conn = getBridgeConn(bridge_stream_id);
    if (bridge_conn == nullptr)
    {
        std::shared_ptr<Connection> pub_conn = getPublisherConn(src_stream_id);
        if (pub_conn == nullptr)
            return Json::nullValue;

        bridge_conn = std::make_shared<BridgeConnection>();
        bridge_conn->init(bridge_stream_id, src_stream_id, ip, port, thread_pool_, true);

        pub_conn->addSubscriber(bridge_stream_id, bridge_conn->getBridgeMediaStream());
        bridge_conns_[bridge_stream_id] = bridge_conn;
    }

    Json::Value data;
    data["ret"] = 0;
    return data;
}

Json::Value Erizo::removePublisher(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
        return Json::nullValue;
    if (root["args"].size() < 2)
        return Json::nullValue;

    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue)
        return Json::nullValue;
    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();

    std::shared_ptr<Client> pub_client = getOrCreateClient(client_id);
    std::shared_ptr<Connection> pub_conn = getPublisherConn(pub_client, stream_id);
    if (pub_conn != nullptr)
    {
        std::vector<std::shared_ptr<Client>> sub_clients = getSubscribers(stream_id);
        for (std::shared_ptr<Client> sub_client : sub_clients)
        {
            std::shared_ptr<Connection> sub_conn = getSubscriberConn(sub_client, stream_id);
            if (sub_conn != nullptr)
            {
                sub_client->subscribers.erase(stream_id);
                sub_conn->close();
            }
        }

        pub_client->publishers.erase(stream_id);
        pub_conn->close();
    }

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

    std::shared_ptr<Client> client = getOrCreateClient(client_id);
    if (client == nullptr)
    {

        printf("111111111111\n");
        return Json::nullValue;
    }

    std::shared_ptr<Connection> conn = getConnection(client, stream_id);
    if (conn == nullptr)
    {

        printf("22222222222222222\n");
        return Json::nullValue;
    }

    if (!msg.isMember("type") ||
        msg["type"].type() != Json::stringValue)
    {

        printf("3333333333333333333\n");
        return Json::nullValue;
    }

    std::string type = msg["type"].asString();
    if (!type.compare("offer"))
    {
        if (!msg.isMember("sdp") ||
            msg["sdp"].type() != Json::stringValue)
        {

            printf("4444444444444444\n");
            return Json::nullValue;
        }

        std::string sdp = msg["sdp"].asString();
        if (conn->setRemoteSdp(sdp))
        {

            printf("55555555555555\n");
            return Json::nullValue;
        }
    }
    else if (!type.compare("candidate"))
    {
        if (!msg.isMember("candidate") ||
            msg["candidate"].type() != Json::objectValue)
        {

            printf("666666666666666666\n");
            return Json::nullValue;
        }

        Json::Value candidate = msg["candidate"];
        if (!candidate.isMember("sdpMLineIndex") ||
            candidate["sdpMLineIndex"].type() != Json::intValue ||
            !candidate.isMember("sdpMid") ||
            candidate["sdpMid"].type() != Json::stringValue ||
            !candidate.isMember("candidate") ||
            candidate["candidate"].type() != Json::stringValue)
        {

            printf("7777777777777\n");
            return Json::nullValue;
        }

        int sdp_mine_index = candidate["sdpMLineIndex"].asInt();
        std::string mid = candidate["sdpMid"].asString();
        std::string candidate_str = candidate["candidate"].asString();
        if (conn->addRemoteCandidate(mid, sdp_mine_index, candidate_str))
            return Json::nullValue;
    }

    Json::Value data;
    data["ret"] = 0;
    return data;
}

std::shared_ptr<Connection> Erizo::getPublisherConn(const std::string &stream_id)
{
    for (auto it = clients_.begin(); it != clients_.end(); it++)
    {
        auto itc = it->second->publishers.find(stream_id);
        if (itc != it->second->publishers.end())
            return itc->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<Client>> Erizo::getSubscribers(const std::string &subscribe_to)
{
    std::vector<std::shared_ptr<Client>> subscribers;
    for (auto it = clients_.begin(); it != clients_.end(); it++)
    {
        auto itc = it->second->subscribers.find(subscribe_to);
        if (itc != it->second->subscribers.end())
            subscribers.push_back(it->second);
    }
    return subscribers;
}

std::shared_ptr<Connection> Erizo::getPublisherConn(std::shared_ptr<Client> client, const std::string &stream_id)
{
    auto it = client->publishers.find(stream_id);
    if (it != client->publishers.end())
        return it->second;

    return nullptr;
}

std::shared_ptr<Connection> Erizo::getSubscriberConn(std::shared_ptr<Client> client, const std::string &stream_id)
{
    auto it = client->subscribers.find(stream_id);
    if (it != client->subscribers.end())
        return it->second;

    return nullptr;
}

std::shared_ptr<Connection> Erizo::getConnection(std::shared_ptr<Client> client, const std::string &stream_id)
{
    {
        auto it = client->publishers.find(stream_id);
        if (it != client->publishers.end())
            return it->second;
    }
    {
        auto it = client->subscribers.find(stream_id);
        if (it != client->subscribers.end())
            return it->second;
    }

    return nullptr;
}

std::shared_ptr<BridgeConnection> Erizo::getBridgeConn(const std::string &stream_id)
{
    auto it = bridge_conns_.find(stream_id);
    if (it != bridge_conns_.end())
        return it->second;
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
