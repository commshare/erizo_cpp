#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>

#include <json/json.h>
#include <logger.h>


namespace erizo
{
class ExtMap;
class RtpMap;
} // namespace erizo

class Config
{
  DECLARE_LOGGER();

public:
  static Config *getInstance();
  virtual ~Config();
  int init(const std::string &config_file);

private:
  Config();
  int initConfig(const Json::Value &root);
  int initMedia(const Json::Value &root);

public:
  // RabbitMQ config
  std::string rabbitmq_username;
  std::string rabbitmq_passwd;
  std::string rabbitmq_hostname;
  unsigned short rabbitmq_port;
  std::string uniquecast_exchange;
  std::string boardcast_exchange;

  // Erizo threadpool config
  int erizo_worker_num;
  int erizo_io_worker_num;
  int bridge_io_worker_num;

  // Erizo libnice config
  // stun
  std::string stun_server;
  unsigned short stun_port;
  // turn
  std::string turn_server;
  unsigned short turn_port;
  std::string turn_username;
  std::string turn_passwd;
  std::string network_interface;
  // other
  unsigned int ice_components;
  bool should_trickle;
  int max_port;
  int min_port;

  //Erizo media type
  std::string audio_codec;
  std::string video_codec;

  std::vector<erizo::ExtMap> ext_maps;
  std::vector<erizo::RtpMap> rtp_maps;

private:
  static Config *instance_;
};

#endif