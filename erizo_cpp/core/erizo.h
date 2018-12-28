#ifndef ERIZO_H
#define ERIZO_H

#include <string>
#include <memory>

#include <json/json.h>
#include <logger.h>
#include <thread/IOThreadPool.h>
#include <thread/ThreadPool.h>

#include "model/client.h"
#include "rabbitmq/amqp_helper.h"

class Erizo : public ConnectionListener
{
  DECLARE_LOGGER();

public:
  Erizo();
  ~Erizo();

  int init(const std::string &agent_id, const std::string &erizo_id);
  void close();
  void onEvent(const std::string &reply_to, const std::string &msg) override;

private:
  Json::Value addPublisher(const Json::Value &root);
  Json::Value addSubscriber(const Json::Value &root);
  Json::Value processSignaling(const Json::Value &root);

  std::shared_ptr<Connection> findConn(const std::string &client_id, const std::string &stream_id);
  std::shared_ptr<Connection> findConn(const std::string &stream_id);
  std::shared_ptr<Client> getOrCreateClient(const std::string &client_id);

private:
  std::shared_ptr<AMQPHelper> amqp_uniquecast_;
  std::shared_ptr<erizo::ThreadPool> thread_pool_;
  std::shared_ptr<erizo::IOThreadPool> io_thread_pool_;
  std::map<std::string, std::shared_ptr<Client>> clients_;

  std::string agent_id_;
  std::string erizo_id_;
  bool init_;
};

#endif