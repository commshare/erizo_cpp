#ifndef AMQP_HELPER_H
#define AMQP_HELPER_H

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <logger.h>

#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include <queue>
#include <functional>
#include <condition_variable>
#include <mutex>

class AMQPHelper
{
  DECLARE_LOGGER();

  struct AMQPData
  {
    std::string exchange;
    std::string queuename;
    std::string binding_key;
    std::string msg;
  };

public:
  AMQPHelper();
  ~AMQPHelper();

  int init(const std::string &binding_key, const std::function<void(const std::string &)> &func);
  void close();

  void sendMessage(const std::string &queuename,
                   const std::string &binding_key,
                   const std::string &send_msg);

private:
  int checkError(amqp_rpc_reply_t x);
  int send(const std::string &exchange,
           const std::string &queuename,
           const std::string &binding_key,
           const std::string &send_msg);

private:
  std::mutex send_queue_mux_;
  std::condition_variable send_cond_;
  std::queue<AMQPData> send_queue_;
  amqp_connection_state_t conn_;
  std::unique_ptr<std::thread> recv_thread_;
  std::unique_ptr<std::thread> send_thread_;
  std::atomic<bool> run_;
  bool init_;
};

#endif