#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <map>
#include <memory>

#include <thread/IOThreadPool.h>
#include <thread/ThreadPool.h>

#include "connection.h"

class Client
{
public:
  Client(const std::string &id, std::shared_ptr<erizo::ThreadPool> thread_pool, std::shared_ptr<erizo::IOThreadPool> io_thread_pool);
  ~Client();

  std::shared_ptr<Connection> createConnection();

private:
  std::string id_;
  std::shared_ptr<erizo::ThreadPool> thread_pool_;
  std::shared_ptr<erizo::IOThreadPool> io_thread_pool_;
  uint32_t connection_index_;
};

#endif