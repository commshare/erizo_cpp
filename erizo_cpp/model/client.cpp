#include "client.h"

Client::Client(const std::string &id, std::shared_ptr<erizo::ThreadPool> thread_pool, std::shared_ptr<erizo::IOThreadPool> io_thread_pool) : id_(id),
                                                                                                                                             thread_pool_(thread_pool),
                                                                                                                                             io_thread_pool_(io_thread_pool),
                                                                                                                                             connection_index_(0)

{
}

Client::~Client() {}

std::shared_ptr<Connection> Client::createConnection()
{
    connection_index_++;
    std::string connection_id = id_ + "_";
    connection_id += connection_index_;
    return std::make_shared<Connection>(connection_id, thread_pool_, io_thread_pool_);
}