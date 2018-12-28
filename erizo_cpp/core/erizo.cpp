#include "erizo.h"
#include "common/config.h"

DEFINE_LOGGER(Erizo, "Erizo");

Erizo::Erizo() : init_(false),
                 id_(""),
                 amqp_uniquecast_(nullptr),
                 thread_pool_(nullptr),
                 io_thread_pool_(nullptr)
{
}

Erizo::~Erizo() {}

void Erizo::onEvent(const std::string &reply_to, const std::string &msg)
{
    if (!init_)
        return;
    amqp_uniquecast_->addCallback(reply_to, reply_to, msg);
}

int Erizo::init(const std::string &id)
{
    if (init_)
        return 0;

    id_ = id;
    io_thread_pool_ = std::make_shared<erizo::IOThreadPool>(Config::getInstance()->erizo_io_worker_num_);
    io_thread_pool_->start();

    thread_pool_ = std::make_shared<erizo::ThreadPool>(Config::getInstance()->erizo_worker_num_);
    thread_pool_->start();

    std::string uniquecast_binding_key_ = "Erizo_" + id_;
    amqp_uniquecast_ = std::make_shared<AMQPHelper>();
    if (amqp_uniquecast_->init(Config::getInstance()->uniquecast_exchange_, uniquecast_binding_key_, [&](const std::string &msg) {
            Json::Value root;
            Json::Reader reader;
            if (!reader.parse(msg, root))
            {
                ELOG_ERROR("Message parse failed");
                return;
            }

            if (!root.isMember("corrID") ||
                root["corrID"].type() != Json::intValue ||
                !root.isMember("replyTo") ||
                root["replyTo"].type() != Json::stringValue ||
                !root.isMember("data") ||
                root["data"].type() != Json::objectValue)
            {
                ELOG_ERROR("Message header format error");
                return;
            }

            int corrid = root["corrID"].asInt();
            std::string reply_to = root["replyTo"].asString();
            Json::Value data = root["data"];
            if (!data.isMember("method") ||
                data["method"].type() != Json::stringValue)
            {
                ELOG_ERROR("Message data format error");
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
                ELOG_ERROR("Unknow message %s failed", msg);
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
        ELOG_ERROR("AMQP uniquecast init failed");
        return 1;
    }

    init_ = true;
    return 0;
}

Json::Value Erizo::addSubscriber(const Json::Value &root)
{
    if (!root.isMember("args") ||
        root["args"].type() != Json::arrayValue)
    {
        return Json::nullValue;
    }
    if (root["args"].size() < 4)
    {
        return Json::nullValue;
    }
    Json::Value args = root["args"];
    if (args[0].type() != Json::stringValue ||
        args[1].type() != Json::stringValue ||
        args[2].type() != Json::stringValue ||
        args[3].type() != Json::stringValue)// ||
        //args[4].type() != Json::stringValue)
    {
        return Json::nullValue;
    }
    std::string client_id = args[0].asString();
    std::string stream_id = args[1].asString();
 //   std::string subscribe_to = args[2].asString();
    std::string stream_label = args[2].asString();
    std::string reply_to = args[3].asString();

    auto it = clients_.find(client_id);

    if (it == clients_.end())
        clients_[client_id] = std::make_shared<Client>(client_id);
    std::shared_ptr<Client> client = clients_[client_id];
    std::shared_ptr<Publisher> publisher = getPublisher(stream_id);
    if (publisher == nullptr)
    {
        return Json::nullValue;
    }
 
    std::shared_ptr<Subscriber> subscriber = std::make_shared<Subscriber>("1111111111", id_, client_id, stream_id, stream_label, reply_to, thread_pool_, io_thread_pool_);
    subscriber->setConnectionListener(this);
 //   subscriber->setSubscribeTo(subscribe_to);
    subscriber->init();
    publisher->addSubscriber(stream_id, subscriber->getMediaStream());
    client->addSubscriber(subscriber);

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
    std::string stream_label = args[2].asString();
    std::string reply_to = args[3].asString();

    auto it = clients_.find(client_id);
    if (it == clients_.end())
        clients_[client_id] = std::make_shared<Client>(client_id);

    std::shared_ptr<Client> client = clients_[client_id];
    std::shared_ptr<Publisher> publisher = std::make_shared<Publisher>("1111111111", id_, client_id, stream_id, stream_label, reply_to, thread_pool_, io_thread_pool_);
    publisher->setConnectionListener(this);
    publisher->init();
    client->addPublisher(publisher);

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
    id_ = "";
}

// bool Erizo::processPublisherSignaling(const Json::Value &root)
// {
//     if (!checkMsgFmt(root) || !checkArgs(root))
//         return true;

//     std::string reply_to = root["replyTo"].asString();

//     Json::Value args = root["args"];
//     std::string client_id = args[0].asString();
//     std::string stream_id = args[1].asString();
//     Json::Value options = args[2];

//     auto it = publishers_.find(stream_id);
//     if (it != publishers_.end())
//     {
//         std::shared_ptr<Connection> connection = it->second;
//         if (!options.isMember("type") || options["type"].type() != Json::stringValue)
//         {
//             ELOG_ERROR("processSignaling signaling type error");
//             return true;
//         }

//         std::string signaling_type = options["type"].asString();
//         if (!signaling_type.compare("offer"))
//         {
//             if (!options.isMember("sdp") || options["sdp"].type() != Json::stringValue)
//             {
//                 ELOG_ERROR("processSignaling sdp is null or type wrong");
//                 return true;
//             }

//             std::string sdp = options["sdp"].asString();
//             if (!connection->setRemoteSdp(sdp))
//             {
//                 ELOG_ERROR("processSignaling sdp parse failed");
//                 return true;
//             }
//         }
//         else if (!signaling_type.compare("candidate"))
//         {
//             if (!options.isMember("candidate") || options["candidate"].type() != Json::objectValue)
//             {
//                 ELOG_ERROR("processSignaling candidate is null or type wrong");
//                 return true;
//             }

//             Json::Value candidate = options["candidate"];
//             if (!candidate.isMember("sdpMLineIndex") ||
//                 candidate["sdpMLineIndex"].type() != Json::uintValue ||
//                 !candidate.isMember("sdpMid") ||
//                 candidate["sdpMid"].type() != Json::stringValue ||
//                 !candidate.isMember("candidate") ||
//                 candidate["candidate"].type() != Json::stringValue)
//             {
//                 int sdp_mine_index = candidate["sdpMLineIndex"].asInt();
//                 std::string mid = candidate["sdpMid"].asString();
//                 std::string candidate_str = candidate["candidate"].asString();
//                 if (!connection->addRemoteCandidate(mid, sdp_mine_index, candidate_str))
//                 {
//                     ELOG_ERROR("processSignaling candidate parse failed");
//                     return true;
//                 }
//             }
//         }
//         return true;
//     }
//     return false;
// }

// bool Erizo::processSubscirberSignaling(const Json::Value &root)
// {

//     if (!checkMsgFmt(root) || !checkArgs(root))
//         return true;

//     std::string reply_to = root["replyTo"].asString();
//     Json::Value args = root["args"];
//     std::string client_id = args[0].asString();
//     std::string stream_id = args[1].asString();
//     Json::Value options = args[2];

//     auto subscribers_it = subscribers_.find(client_id);
//     if (subscribers_it != subscribers_.end())
//     {
//         std::map<std::string, std::shared_ptr<Connection>> &subscribe_streams = subscribers_it->second;
//         auto subscriber_streams_it = subscribe_streams.find(stream_id);
//         if (subscriber_streams_it != subscribe_streams.end())
//         {
//             std::shared_ptr<Connection> connection = subscriber_streams_it->second;
//             if (!options.isMember("type") || options["type"].type() != Json::stringValue)
//             {
//                 ELOG_ERROR("processSignaling signaling type error");
//                 return true;
//             }

//             std::string signaling_type = options["type"].asString();
//             if (!signaling_type.compare("offer"))
//             {
//                 if (!options.isMember("sdp") || options["sdp"].type() != Json::stringValue)
//                 {
//                     ELOG_ERROR("processSignaling sdp is null or type wrong");
//                     return true;
//                 }

//                 std::string sdp = options["sdp"].asString();
//                 if (!connection->setRemoteSdp(sdp))
//                 {
//                     ELOG_ERROR("processSignaling sdp parse failed");
//                     return true;
//                 }
//             }
//             else if (!signaling_type.compare("candidate"))
//             {
//                 if (!options.isMember("candidate") || options["candidate"].type() != Json::objectValue)
//                 {
//                     ELOG_ERROR("processSignaling candidate is null or type wrong");
//                     return true;
//                 }

//                 Json::Value candidate = options["candidate"];
//                 if (!candidate.isMember("sdpMLineIndex") ||
//                     candidate["sdpMLineIndex"].type() != Json::uintValue ||
//                     !candidate.isMember("sdpMid") ||
//                     candidate["sdpMid"].type() != Json::stringValue ||
//                     !candidate.isMember("candidate") ||
//                     candidate["candidate"].type() != Json::stringValue)
//                 {
//                     int sdp_mine_index = candidate["sdpMLineIndex"].asInt();
//                     std::string mid = candidate["sdpMid"].asString();
//                     std::string candidate_str = candidate["candidate"].asString();
//                     if (!connection->addRemoteCandidate(mid, sdp_mine_index, candidate_str))
//                     {
//                         ELOG_ERROR("processSignaling candidate parse failed");
//                         return true;
//                     }
//                 }
//             }
//             return true;
//         }
//     }

//     return false;
// }

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

    auto it = clients_.find(client_id);
    if (it == clients_.end())
        return Json::nullValue;

    std::shared_ptr<Client> client = it->second;
    std::shared_ptr<Stream> stream = client->getStream(stream_id);
    if (stream == nullptr)
    {
        // stream = client->getSubscriber(stream_id);
        // if (stream == nullptr)
         printf("EEEEEEEEEEEEEEEEEEEEEEEXXXX\n");
            return Json::nullValue;
        
    }


    if (!msg.isMember("type") ||
        msg["type"].type() != Json::stringValue)
    {
        ELOG_ERROR("Signaling message without type,dump:%s", msg);
        return Json::nullValue;
    }

    std::string type = msg["type"].asString();

   
    if (!type.compare("offer"))
    {
        if (!msg.isMember("sdp") ||
            msg["sdp"].type() != Json::stringValue)
        {
            ELOG_ERROR("Signaling message offer without sdp");
            return Json::nullValue;
        }

        std::string sdp = msg["sdp"].asString();
     
        stream->setRemoteSdp(sdp);
    }
    else if (!type.compare("candidate"))
    {
        if (!msg.isMember("candidate") ||
            msg["candidate"].type() != Json::objectValue)
        {
            ELOG_ERROR("Signaling message candidate format error");
            return Json::nullValue;
        }

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
            stream->addRemoteCandidate(mid, sdp_mine_index, candidate_str);
        }
    }

    Json::Value data;
    data["ret"] = 0;
    return data;
}
