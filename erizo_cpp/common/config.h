#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include <map>

#include <json/json.h>
#include <logger.h>
#include <SdpInfo.h>

class Config
{
  DECLARE_LOGGER();

public:
  static Config *getInstance();
  virtual ~Config();
  int init(const std::string &config_file);
  const std::vector<erizo::ExtMap> &getExpMaps() { return ext_maps_; }
  const std::vector<erizo::RtpMap> &getRtpMaps() { return rtp_maps_; }

private:
  Config();

  int initConfig(const Json::Value &root);
  int initMedia(const Json::Value &root);

public:
  // RabbitMQ config
  std::string rabbitmq_username_;
  std::string rabbitmq_passwd_;
  std::string rabbitmq_hostname_;
  unsigned short rabbitmq_port_;
  std::string uniquecast_exchange_;
  std::string boardcast_exchange_;

  // Bridge config
  std::string bridge_ip_;
  unsigned short bridge_port_;

  // Erizo threadpool config
  int erizo_worker_num_;
  int erizo_io_worker_num_;

  // Erizo libnice config
  // stun
  std::string stun_server_;
  unsigned short stun_port_;
  // turn
  std::string turn_server_;
  unsigned short turn_port_;
  std::string turn_username_;
  std::string turn_password_;
  std::string network_interface_;
  // other
  unsigned int ice_components_;
  bool should_trickle_;
  int max_port_;
  int min_port_;

  //Erizo media type
  std::string audio_codec_;
  std::string video_codec_;

private:
  static Config *instance_;
  std::vector<erizo::ExtMap> ext_maps_;
  std::vector<erizo::RtpMap> rtp_maps_;
};

#endif