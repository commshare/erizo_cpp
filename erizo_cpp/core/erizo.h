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

  int init(const std::string &id);
  void close();
  void onEvent(const std::string &reply_to, const std::string &msg) override;

private:
  Json::Value addPublisher(const Json::Value &root);
  Json::Value addSubscriber(const Json::Value &root);
  Json::Value processSignaling(const Json::Value &root);

  // bool processPublisherSignaling(const Json::Value &root);
  // bool processSubscirberSignaling(const Json::Value &root);

  std::shared_ptr<Publisher> getPublisher(const std::string &publisher_id)
  {
    std::shared_ptr<Publisher> publisher;
    for (auto it = clients_.begin(); it != clients_.end(); it++)
    {
      publisher = it->second->getPublisher(publisher_id);
      if (publisher != nullptr)
        return publisher;
    }
    return nullptr;
  }

private:
  bool init_;
  std::string id_;

  std::shared_ptr<AMQPHelper> amqp_uniquecast_;
  std::shared_ptr<erizo::ThreadPool> thread_pool_;
  std::shared_ptr<erizo::IOThreadPool> io_thread_pool_;
  std::map<std::string, std::shared_ptr<Client>> clients_;
};

#endif