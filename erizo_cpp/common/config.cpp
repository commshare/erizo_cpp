#include "config.h"

#include <fstream>
#include <string.h>

DEFINE_LOGGER(Config, "Config");
Config *Config::instance_ = nullptr;
Config::~Config()
{
    if (instance_ != nullptr)
    {
        delete instance_;
        instance_ = nullptr;
    }
}

Config *Config::getInstance()
{
    if (instance_ == nullptr)
        instance_ = new Config;
    return instance_;
}

Config::Config()
{
    rabbitmq_username_ = "linmin";
    rabbitmq_passwd_ = "linmin";
    rabbitmq_hostname_ = "localhost";
    rabbitmq_port_ = 5672;
    uniquecast_exchange_ = "erizo_uniquecast_exchange";
    boardcast_exchange_ = "erizo_boardcast_exchange";

    erizo_worker_num_ = 5;
    erizo_io_worker_num_ = 5;
    bridge_io_worker_num_ = 5;

    stun_server_ = "stun:stun.l.google.com";
    stun_port_ = 19302;
    turn_server_ = "";
    turn_port_ = 0;
    turn_username_ = "";
    turn_password_ = "";
    network_interface_ = "";
    ice_components_ = 0;
    should_trickle_ = false;
    max_port_ = 0;
    min_port_ = 0;

    audio_codec_ = "opus";
    video_codec_ = "vp8";
}

int Config::initConfig(const Json::Value &root)
{
    Json::Value rabbitmq = root["rabbitmq"];
    if (!root.isMember("rabbitmq") ||
        rabbitmq.type() != Json::objectValue ||
        !rabbitmq.isMember("host") ||
        rabbitmq["host"].type() != Json::stringValue ||
        !rabbitmq.isMember("port") ||
        rabbitmq["port"].type() != Json::intValue ||
        !rabbitmq.isMember("username") ||
        rabbitmq["username"].type() != Json::stringValue ||
        !rabbitmq.isMember("password") ||
        rabbitmq["password"].type() != Json::stringValue ||
        !rabbitmq.isMember("boardcast_exchange") ||
        rabbitmq["boardcast_exchange"].type() != Json::stringValue ||
        !rabbitmq.isMember("uniquecast_exchange") ||
        rabbitmq["uniquecast_exchange"].type() != Json::stringValue)
    {
        ELOG_ERROR("Rabbitmq config check error");
        return 1;
    }

    Json::Value ice = root["ice"];
    if (!root.isMember("ice") ||
        ice.type() != Json::objectValue ||
        !ice.isMember("network_interface") ||
        ice["network_interface"].type() != Json::stringValue ||
        !ice.isMember("ice_components") ||
        ice["ice_components"].type() != Json::intValue ||
        !ice.isMember("should_trickle") ||
        ice["should_trickle"].type() != Json::booleanValue ||
        !ice.isMember("min_port") ||
        ice["min_port"].type() != Json::intValue ||
        !ice.isMember("max_port") ||
        ice["max_port"].type() != Json::intValue)
    {
        ELOG_ERROR("Ice config check error");
        return 1;
    }

    Json::Value stun = ice["stun"];
    if (!ice.isMember("stun") ||
        stun.type() != Json::objectValue ||
        !stun.isMember("host") ||
        stun["host"].type() != Json::stringValue ||
        !stun.isMember("port") ||
        stun["port"].type() != Json::intValue)
    {
        ELOG_ERROR("Ice stun check error");
        return 1;
    }

    Json::Value turn = ice["turn"];
    if (!ice.isMember("turn") ||
        turn.type() != Json::objectValue ||
        !turn.isMember("host") ||
        turn["host"].type() != Json::stringValue ||
        !turn.isMember("port") ||
        turn["port"].type() != Json::intValue ||
        !turn.isMember("username") ||
        turn["username"].type() != Json::stringValue ||
        !turn.isMember("password") ||
        turn["password"].type() != Json::stringValue)
    {
        ELOG_ERROR("Ice turn check error");
        return 1;
    }

    Json::Value media = root["media"];
    if (!root.isMember("media") ||
        media.type() != Json::objectValue ||
        !media.isMember("audio_codec") ||
        media["audio_codec"].type() != Json::stringValue ||
        !media.isMember("video_codec") ||
        media["video_codec"].type() != Json::stringValue)
    {
        ELOG_ERROR("Media check error");
        return 1;
    }

    rabbitmq_hostname_ = rabbitmq["host"].asString();
    rabbitmq_port_ = rabbitmq["port"].asInt();
    rabbitmq_username_ = rabbitmq["username"].asString();
    rabbitmq_passwd_ = rabbitmq["password"].asString();
    uniquecast_exchange_ = rabbitmq["uniquecast_exchange"].asString();
    boardcast_exchange_ = rabbitmq["boardcast_exchange"].asString();

    stun_server_ = stun["host"].asString();
    stun_port_ = stun["port"].asInt();
    turn_server_ = turn["host"].asString();
    turn_port_ = turn["port"].asInt();
    turn_username_ = turn["username"].asString();
    turn_password_ = turn["password"].asString();
    network_interface_ = ice["network_interface"].asString();
    ice_components_ = ice["ice_components"].asInt();
    should_trickle_ = ice["should_trickle"].asBool();
    min_port_ = ice["min_port"].asInt();
    max_port_ = ice["max_port"].asInt();
    audio_codec_ = media["audio_codec"].asString();
    video_codec_ = media["video_codec"].asString();

    return 0;
}

int Config::initMedia(const Json::Value &root)
{
    ext_maps_.clear();
    if (root.isMember("extMappings") && root["extMappings"].type() == Json::arrayValue)
    {
        uint32_t num = root["extMappings"].size();
        for (uint32_t i = 0; i < num; i++)
        {
            ext_maps_.push_back({i, root["extMappings"][i].asString()});
        }
    }

    rtp_maps_.clear();
    if (root.isMember("mediaType") && root["mediaType"].type() == Json::arrayValue)
    {
        uint32_t num = root["mediaType"].size();
        for (uint32_t i = 0; i < num; i++)
        {
            Json::Value value = root["mediaType"][i];
            erizo::RtpMap rtp_map;
            if (value.isMember("payloadType") && value["payloadType"].type() == Json::intValue)
            {
                rtp_map.payload_type = value["payloadType"].asInt();
            }

            if (value.isMember("clockRate") && value["clockRate"].type() == Json::intValue)
            {
                rtp_map.clock_rate = value["clockRate"].asInt();
            }
            if (value.isMember("channels") && value["channels"].type() == Json::intValue)
            {
                rtp_map.channels = value["channels"].asInt();
            }

            if (value.isMember("feedbackTypes") && value["feedbackTypes"].type() == Json::arrayValue)
            {
                uint32_t fb_type_num = value["feedbackTypes"].size();
                for (uint32_t j = 0; j < fb_type_num && (value["feedbackTypes"][j].type() == Json::stringValue); j++)
                    rtp_map.feedback_types.push_back(value["feedbackTypes"][j].asString());
            }

            if (value.isMember("formatParameters") && value["formatParameters"].type() == Json::objectValue)
            {
                Json::Value::Members members = value["formatParameters"].getMemberNames();
                Json::Value fmt_param = value["formatParameters"];
                for (auto it = members.begin(); it != members.end(); it++)
                {

                    std::string value = fmt_param[*it].asString();
                    rtp_map.format_parameters[*it] = value;
                }
            }

            if (value.isMember("encodingName") && value["encodingName"].type() == Json::stringValue)
            {

                rtp_map.encoding_name = value["encodingName"].asString();
                if (!strcasecmp(rtp_map.encoding_name.c_str(), audio_codec_.c_str()))
                {
                    rtp_maps_.push_back(rtp_map);
                }
                else if (!strcasecmp(rtp_map.encoding_name.c_str(), video_codec_.c_str()))
                {
                    rtp_maps_.push_back(rtp_map);
                }
            }
        }
    }

    return 0;
}

int Config::init(const std::string &config_file)
{
    std::ifstream ifs(config_file, std::ios::binary);
    if (!ifs.is_open())
    {
        ELOG_ERROR("Open %s failed", config_file);
        return 1;
    }

    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(ifs, root))
    {
        ELOG_ERROR("Parse %s failed", config_file);
        return 1;
    }

    if (initConfig(root))
    {
        ELOG_ERROR("initConfig failed");
        return 1;
    }

    if (initMedia(root))
    {
        ELOG_ERROR("initMedia failed");
        return 1;
    }

    return 0;
}
