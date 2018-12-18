#ifndef ERIZO_H
#define ERIZO_H

#include <string>
#include <memory>

#include <json/json.h>

#include <logger.h>
#include <thread/IOThreadPool.h>
#include <thread/ThreadPool.h>

#include "rabbitmq/amqp_helper.h"
#include "model/client.h"

class Erizo
{
  DECLARE_LOGGER();

public:
  Erizo();
  ~Erizo();

  int init(const std::string &id);
  void close();

private:
  std::shared_ptr<Client> getOrCreateClient(std::string client_id);

  bool checkMsgFmt(const Json::Value &root);
  bool checkArgs(const Json::Value &root);

  void addPublisher(const Json::Value &root);
  void addSubscriber(const Json::Value &root);
  void keepAlive(const Json::Value &root);

  void processSignaling(const Json::Value &root);
  bool processPublisherSignaling(const Json::Value &root);
  bool processSubscirberSignaling(const Json::Value &root);

private:
  bool init_;
  std::string id_;
  std::shared_ptr<AMQPHelper> amqp_broadcast_;
  std::shared_ptr<AMQPHelper> amqp_uniquecast_;

  std::shared_ptr<erizo::ThreadPool> thread_pool_;
  std::shared_ptr<erizo::IOThreadPool> io_thread_pool_;

  std::map<std::string, std::shared_ptr<Client>> clients_;
  std::map<std::string, std::shared_ptr<Connection>> publishers_;
  std::map<std::string, std::map<std::string, std::shared_ptr<Connection>>> subscribers_;
};

#endif