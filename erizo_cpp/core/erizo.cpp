#include "erizo.h"

#include "common/utils.h"
#include "common/config.h"

#include "model/client.h"
#include "model/connection.h"
#include "model/bridge_conn.h"

#include "rabbitmq/amqp_helper.h"

#include <thread/IOThreadPool.h>
#include <thread/ThreadPool.h>

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

Erizo *Erizo::instance_ = nullptr;
Erizo *Erizo::getInstance()
{
    if (!instance_)
        instance_ = new Erizo();
    return instance_;
}

void Erizo::onEvent(const std::string &reply_to, const std::string &msg)
{
    if (!init_)
        return;
    amqp_uniquecast_->sendMessage(reply_to, reply_to, msg);
}

int Erizo::init(const std::string &agent_id, const std::string &erizo_id, const std::string &ip, uint16_t port)
{
    if (init_)
        return 0;

    agent_id_ = agent_id;
    erizo_id_ = erizo_id;

    io_thread_pool_ = std::make_shared<erizo::IOThreadPool>(Config::getInstance()->erizo_io_worker_num);
    io_thread_pool_->start();

    thread_pool_ = std::make_shared<erizo::ThreadPool>(Config::getInstance()->erizo_worker_num);
    thread_pool_->start();

    amqp_uniquecast_ = std::make_shared<AMQPHelper>();
    if (amqp_uniquecast_->init(erizo_id_, [this](const std::string &msg) {
            Json::Value root;
            Json::Reader reader(Json::Features::strictMode());
            if (!reader.parse(msg, root))
            {
                ELOG_ERROR("json parse root failed,dump %s", msg);
                return;
            }

            if (!root.isMember("data") ||
                root["data"].type() != Json::objectValue)
            {
                ELOG_ERROR("json parse data failed,dump %s", msg);
                return;
            }

            Json::Value data = root["data"];
            if (!data.isMember("method") ||
                data["method"].type() != Json::stringValue)
            {
                ELOG_ERROR("json parse method failed,dump %s", msg);
                return;
            }

            std::string method = data["method"].asString();
            if (!method.compare("addPublisher"))
            {
                addPublisher(data);
            }
            else if (!method.compare("addSubscriber"))
            {
                addSubscriber(data);
            }
            else if (!method.compare("processSignaling"))
            {
                processSignaling(data);
            }
            else if (!method.compare("addVirtualPublisher"))
            {
                addVirtualPublisher(data);
            }
            else if (!method.compare("addVirtualSubscriber"))
            {
                addVirtualSubscriber(data);
            }
            else if (!method.compare("removeSubscriber"))
            {
                removeSubscriber(data);
            }
            else if (!method.compare("removePublisher"))
            {
                removePublisher(data);
            }
            else if (!method.compare("removeVirtualPublisher"))
            {
                removeVirtualPublisher(data);
            }
            else if (!method.compare("removeVirtualSubscriber"))
            {
                removeVirtualSubscriber(data);
            }
        }))
    {
        ELOG_ERROR("amqp initialize failed");
        return 1;
    }
    init_ = true;
    return 0;
}

void Erizo::addSubscriber(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
    {
        ELOG_ERROR("json parse args failed,dump %s", Utils::dumpJson(root));
        return;
    }
    if (root["args"].size() < 5)
    {
        ELOG_ERROR("json parse args num failed,dump %s", Utils::dumpJson(root));
        return;
    }
    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue ||
        args[2].type() != Json::stringValue ||
        args[3].type() != Json::stringValue ||
        args[4].type() != Json::stringValue)
    {
        ELOG_ERROR("json parse args type failed,dump %s", Utils::dumpJson(root));
        return;
    }
    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();
    std::string stream_label = args[2].asString();
    std::string reply_to = args[3].asString();
    std::string isp = args[4].asString();

    std::shared_ptr<Client> client = getOrCreateClient(client_id);
    std::shared_ptr<Connection> pub_conn = getPublishConn(stream_id);
    if (pub_conn != nullptr)
    {
        std::shared_ptr<Connection> sub_conn = std::make_shared<Connection>();
        sub_conn->setConnectionListener(this);
        sub_conn->init(agent_id_, erizo_id_, client_id, stream_id, stream_label, false, reply_to, isp, thread_pool_, io_thread_pool_);

        pub_conn->addSubscriber(client_id, sub_conn->getMediaStream());
        client->subscribers[stream_id] = sub_conn;
    }
    else
    {
        std::shared_ptr<BridgeConn> bridge_conn = getBridgeConn(stream_id);
        if (bridge_conn != nullptr)
        {
            std::shared_ptr<Connection> sub_conn = std::make_shared<Connection>();
            sub_conn->setConnectionListener(this);
            sub_conn->init(agent_id_, erizo_id_, client_id, stream_id, stream_label, false, reply_to, isp, thread_pool_, io_thread_pool_);

            bridge_conn->addSubscriber(client_id, sub_conn->getMediaStream());
            client->subscribers[stream_id] = sub_conn;
        }
    }
}

void Erizo::removeSubscriber(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
    {
        ELOG_ERROR("json parse args failed,dump %s", Utils::dumpJson(root));
        return;
    }
    if (root["args"].size() < 2)
    {
        ELOG_ERROR("json parse args num failed,dump %s", Utils::dumpJson(root));
        return;
    }
    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue)
    {
        ELOG_ERROR("json parse args type failed,dump %s", Utils::dumpJson(root));
        return;
    }
    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();

    std::shared_ptr<Connection> pub_conn = getPublishConn(stream_id);
    if (pub_conn != nullptr)
        pub_conn->removeSubscriber(client_id);

    std::shared_ptr<BridgeConn> bridge_conn = getBridgeConn(stream_id);
    if (bridge_conn != nullptr)
        bridge_conn->removeSubscriber(client_id);

    std::shared_ptr<Client> client = getOrCreateClient(client_id);
    std::shared_ptr<Connection> sub_conn = getSubscribeConn(client, stream_id);
    if (sub_conn != nullptr)
    {
        client->subscribers.erase(stream_id);
        if (client->publishers.size() == 0 && client->subscribers.size() == 0)
            clients_.erase(client->id);
        sub_conn->close();
    }
}

void Erizo::addPublisher(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
    {
        ELOG_ERROR("json parse args failed,dump %s", Utils::dumpJson(root));
        return;
    }
    if (root["args"].size() < 6)
    {
        ELOG_ERROR("json parse args num failed,dump %s", Utils::dumpJson(root));
        return;
    }
    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue ||
        args[2].type() != Json::stringValue ||
        args[3].type() != Json::stringValue ||
        args[4].type() != Json::stringValue ||
        args[5].type() != Json::stringValue)
    {
        ELOG_ERROR("json parse args type failed,dump %s", Utils::dumpJson(root));
        return;
    }

    std::string room_id = args[0].asString();
    std::string client_id = args[1].asString();
    std::string stream_id = args[2].asString();
    std::string label = args[3].asString();
    std::string reply_to = args[4].asString();
    std::string isp = args[5].asString();

    std::shared_ptr<Client> client = getOrCreateClient(client_id);
    std::shared_ptr<Connection> conn = std::make_shared<Connection>();
    conn->setConnectionListener(this);
    conn->setRoomId(room_id);
    conn->init(agent_id_, erizo_id_, client_id, stream_id, label, true, reply_to, isp, thread_pool_, io_thread_pool_);
    client->publishers[stream_id] = conn;
}

void Erizo::addVirtualPublisher(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
    {
        ELOG_ERROR("json parse args failed,dump %s", Utils::dumpJson(root));
        return;
    }
    if (root["args"].size() < 6)
    {
        ELOG_ERROR("json parse args num failed,dump %s", Utils::dumpJson(root));
        return;
    }

    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue ||
        args[2].type() != Json::stringValue ||
        args[3].type() != Json::intValue)
    {
        ELOG_ERROR("json parse args type feiled,dump %s", Utils::dumpJson(root));
        return;
    }

    std::string bridge_stream_id = args[0].asString();
    std::string src_stream_id = args[1].asString();
    std::string ip = args[2].asString();
    uint16_t port = args[3].asInt();
    uint32_t video_ssrc = args[4].asUInt();
    uint32_t audio_ssrc = args[5].asUInt();

    std::shared_ptr<BridgeConn> bridge_conn = getBridgeConn(src_stream_id);
    if (bridge_conn == nullptr)
    {
        bridge_conn = std::make_shared<BridgeConn>();
        bridge_conn->init(bridge_stream_id, src_stream_id, ip, port, io_thread_pool_, false, video_ssrc, audio_ssrc);
        bridge_conns_[src_stream_id] = bridge_conn;
    }
}

void Erizo::removeVirtualPublisher(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
    {
        ELOG_ERROR("json parse args failed,dump %s", Utils::dumpJson(root));
        return;
    }
    if (root["args"].size() < 1)
    {
        ELOG_ERROR("json parse args num failed,dump %s", Utils::dumpJson(root));
        return;
    }

    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue)
    {
        ELOG_ERROR("json parse args type failed,dump %s", Utils::dumpJson(root));
        return;
    }

    std::string src_stream_id = args[0].asString();
    std::shared_ptr<BridgeConn> bridge_conn = getBridgeConn(src_stream_id);
    if (bridge_conn != nullptr)
    {
        std::vector<std::shared_ptr<Client>> sub_clients = getSubscribers(src_stream_id);
        for (std::shared_ptr<Client> sub_client : sub_clients)
        {
            std::shared_ptr<Connection> sub_conn = getSubscribeConn(sub_client, src_stream_id);
            if (sub_conn != nullptr)
            {
                sub_client->subscribers.erase(src_stream_id);
                sub_conn->close();
            }
        }

        bridge_conns_.erase(src_stream_id);
        bridge_conn->close();
    }
}

void Erizo::addVirtualSubscriber(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
    {
        ELOG_ERROR("json parse args failed,dump %s", Utils::dumpJson(root));
        return;
    }
    if (root["args"].size() < 4)
    {
        ELOG_ERROR("json parse args num failed,dump %s", Utils::dumpJson(root));
        return;
    }

    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue ||
        args[2].type() != Json::stringValue ||
        args[3].type() != Json::intValue)
    {
        ELOG_ERROR("json parse args type failed,dump %s", Utils::dumpJson(root));
        return;
    }

    std::string bridge_stream_id = args[0].asString();
    std::string src_stream_id = args[1].asString();
    std::string ip = args[2].asString();
    uint16_t port = args[3].asInt();

    std::shared_ptr<BridgeConn> bridge_conn = getBridgeConn(bridge_stream_id);
    if (bridge_conn == nullptr)
    {
        std::shared_ptr<Connection> pub_conn = getPublishConn(src_stream_id);
        if (pub_conn == nullptr)
            return;

        bridge_conn = std::make_shared<BridgeConn>();
        bridge_conn->init(bridge_stream_id, src_stream_id, ip, port, io_thread_pool_, true);

        pub_conn->addSubscriber(bridge_stream_id, bridge_conn->getBridgeMediaStream());
        bridge_conns_[bridge_stream_id] = bridge_conn;
    }
}

void Erizo::removeVirtualSubscriber(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
    {
        ELOG_ERROR("json parse args failed,dump %s", Utils::dumpJson(root));
        return;
    }
    if (root["args"].size() < 2)
    {
        ELOG_ERROR("json parse args num failed,dump %s", Utils::dumpJson(root));
        return;
    }

    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue)
    {
        ELOG_ERROR("json parse args type failed,dump %s", Utils::dumpJson(root));
        return;
    }

    std::string bridge_stream_id = args[0].asString();
    std::string src_stream_id = args[1].asString();

    std::shared_ptr<Connection> pub_conn = getPublishConn(src_stream_id);
    if (pub_conn != nullptr)
        pub_conn->removeSubscriber(bridge_stream_id);

    std::shared_ptr<BridgeConn> bridge_conn = getBridgeConn(bridge_stream_id);
    if (bridge_conn != nullptr)
    {
        bridge_conns_.erase(bridge_stream_id);
        bridge_conn->close();
    }
}

void Erizo::removePublisher(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
    {
        ELOG_ERROR("json parse args failed,dump %s", Utils::dumpJson(root));
        return;
    }
    if (root["args"].size() < 2)
    {
        ELOG_ERROR("json parse args num failed,dump %s", Utils::dumpJson(root));
        return;
    }

    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue)
    {
        ELOG_ERROR("json parse args type failed,dump %s", Utils::dumpJson(root));
        return;
    }
    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();

    std::shared_ptr<Client> pub_client = getOrCreateClient(client_id);
    std::shared_ptr<Connection> pub_conn = getPublishConn(pub_client, stream_id);
    if (pub_conn != nullptr)
    {
        std::vector<std::shared_ptr<Client>> sub_clients = getSubscribers(stream_id);
        for (std::shared_ptr<Client> sub_client : sub_clients)
        {
            std::shared_ptr<Connection> sub_conn = getSubscribeConn(sub_client, stream_id);
            if (sub_conn != nullptr)
            {
                sub_client->subscribers.erase(stream_id);
                sub_conn->close();
            }
        }

        std::vector<std::shared_ptr<BridgeConn>> bridge_conns = getBridgeConns(stream_id);
        for (std::shared_ptr<BridgeConn> bridge_conn : bridge_conns)
        {
            bridge_conns_.erase(bridge_conn->getBridgeStreamId());
            bridge_conn->close();
        }

        pub_client->publishers.erase(stream_id);
        if (pub_client->publishers.size() == 0 && pub_client->subscribers.size() == 0)
            clients_.erase(pub_client->id);
        pub_conn->close();
    }
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

    clients_.clear();
    bridge_conns_.clear();

    agent_id_ = "";
    erizo_id_ = "";

    init_ = false;
}

void Erizo::processSignaling(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
    {
        ELOG_ERROR("json parse args failed,dump %s", Utils::dumpJson(root));
        return;
    }

    if (root["args"].size() < 3)
    {
        ELOG_ERROR("json parse args num failed,dump %s", Utils::dumpJson(root));
        return;
    }

    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue ||
        args[2].type() != Json::objectValue)
    {

        ELOG_ERROR("json parse args type failed,dump %s", Utils::dumpJson(root));
        return;
    }

    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();
    Json::Value msg = args[2];

    std::shared_ptr<Client> client = getOrCreateClient(client_id);
    if (client == nullptr)
        return;

    std::shared_ptr<Connection> conn = getConn(client, stream_id);
    if (conn == nullptr)
        return;

    if (!msg.isMember("type") ||
        msg["type"].type() != Json::stringValue)
    {
        ELOG_ERROR("json parse type failed,dump %s", Utils::dumpJson(root));
        return;
    }

    std::string type = msg["type"].asString();
    if (!type.compare("offer"))
    {
        if (!msg.isMember("sdp") ||
            msg["sdp"].type() != Json::stringValue)
        {
            ELOG_ERROR("json parse sdp failed,dump %s", Utils::dumpJson(root));
            return;
        }

        std::string sdp = msg["sdp"].asString();
        if (conn->setRemoteSdp(sdp))
            return;
    }
    else if (!type.compare("candidate"))
    {
        if (!msg.isMember("candidate") ||
            msg["candidate"].type() != Json::objectValue)
        {
            ELOG_ERROR("json parse candidate failed,dump %s", Utils::dumpJson(root));
            return;
        }

        Json::Value candidate = msg["candidate"];
        if (!candidate.isMember("sdpMLineIndex") ||
            candidate["sdpMLineIndex"].type() != Json::intValue ||
            !candidate.isMember("sdpMid") ||
            candidate["sdpMid"].type() != Json::stringValue ||
            !candidate.isMember("candidate") ||
            candidate["candidate"].type() != Json::stringValue)
        {
            ELOG_ERROR("json parse [sdpMLineIndex/sdpMid/candidate] failed,dump %s", Utils::dumpJson(root));
            return;
        }

        int sdp_mine_index = candidate["sdpMLineIndex"].asInt();
        std::string mid = candidate["sdpMid"].asString();
        std::string candidate_str = candidate["candidate"].asString();
        if (conn->addRemoteCandidate(mid, sdp_mine_index, candidate_str))
            return;
    }
}

std::shared_ptr<Connection> Erizo::getPublishConn(const std::string &stream_id)
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

std::shared_ptr<Connection> Erizo::getPublishConn(std::shared_ptr<Client> client, const std::string &stream_id)
{
    auto it = client->publishers.find(stream_id);
    if (it != client->publishers.end())
        return it->second;

    return nullptr;
}

std::shared_ptr<Connection> Erizo::getSubscribeConn(std::shared_ptr<Client> client, const std::string &stream_id)
{
    auto it = client->subscribers.find(stream_id);
    if (it != client->subscribers.end())
        return it->second;

    return nullptr;
}

std::shared_ptr<Connection> Erizo::getConn(std::shared_ptr<Client> client, const std::string &stream_id)
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

std::shared_ptr<BridgeConn> Erizo::getBridgeConn(const std::string &stream_id)
{
    auto it = bridge_conns_.find(stream_id);
    if (it != bridge_conns_.end())
        return it->second;
    return nullptr;
}

std::vector<std::shared_ptr<BridgeConn>> Erizo::getBridgeConns(const std::string &src_stream_id)
{
    std::vector<std::shared_ptr<BridgeConn>> bridge_conns;
    for (auto it = bridge_conns_.begin(); it != bridge_conns_.end(); it++)
    {
        if (!it->second->getSrcStreamId().compare(src_stream_id))
            bridge_conns.push_back(it->second);
    }
    return bridge_conns;
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
