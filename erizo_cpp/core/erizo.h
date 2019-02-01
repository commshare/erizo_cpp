#ifndef ERIZO_H
#define ERIZO_H

#include <string>
#include <memory>

#include <json/json.h>
#include <logger.h>

namespace erizo
{
class IOThreadPool;
class ThreadPool;
}; // namespace erizo

class Connection;
class BridgeConn;
class Client;
class AMQPHelper;

class ConnectionListener
{
public:
  virtual void onEvent(const std::string &reply_to, const std::string &event) = 0;
};

class Erizo : public ConnectionListener
{
  DECLARE_LOGGER();

public:
  ~Erizo();
  static Erizo *getInstance();

  int init(const std::string &agent_id, const std::string &erizo_id, const std::string &ip, uint16_t port);
  void close();
  void onEvent(const std::string &reply_to, const std::string &msg) override;

private:
  Erizo();

  void addPublisher(const Json::Value &root);
  void removePublisher(const Json::Value &root);

  void addVirtualPublisher(const Json::Value &root);
  void removeVirtualPublisher(const Json::Value &root);

  void addSubscriber(const Json::Value &root);
  void removeSubscriber(const Json::Value &root);

  void addVirtualSubscriber(const Json::Value &root);
  void removeVirtualSubscriber(const Json::Value &root);

  void processSignaling(const Json::Value &root);

  std::shared_ptr<Connection> getPublishConn(const std::string &stream_id);
  std::vector<std::shared_ptr<Client>> getSubscribers(const std::string &subscribe_to);
  std::shared_ptr<Connection> getPublishConn(std::shared_ptr<Client> client, const std::string &stream_id);
  std::shared_ptr<Connection> getConn(std::shared_ptr<Client> client, const std::string &stream_id);
  std::shared_ptr<Connection> getSubscribeConn(std::shared_ptr<Client> client, const std::string &stream_id);
  std::shared_ptr<BridgeConn> getBridgeConn(const std::string &bridge_stream_id);
  std::vector<std::shared_ptr<BridgeConn>> getBridgeConns(const std::string &src_stream_id);
  std::shared_ptr<Client> getOrCreateClient(const std::string &client_id);

private:
  std::shared_ptr<AMQPHelper> amqp_uniquecast_;
  std::shared_ptr<erizo::ThreadPool> thread_pool_;
  std::shared_ptr<erizo::IOThreadPool> io_thread_pool_;
  std::map<std::string, std::shared_ptr<Client>> clients_;
  std::map<std::string, std::shared_ptr<BridgeConn>> bridge_conns_;

  std::string agent_id_;
  std::string erizo_id_;
  bool init_;

  static Erizo *instance_;
};

#endif