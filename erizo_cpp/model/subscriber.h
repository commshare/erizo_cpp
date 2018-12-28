#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include "stream.h"

class Subscriber : public Stream
{
  public:
    Subscriber(const std::string &agent_id,
               const std::string &erizo_id,
               const std::string &client_id,
               const std::string &stream_id,
               const std::string &stream_label,
               const std::string &reply_to,
               std::shared_ptr<erizo::ThreadPool> thread_pool,
               std::shared_ptr<erizo::IOThreadPool> io_thread_pool) : Stream(agent_id,
                                                                             erizo_id,
                                                                             client_id,
                                                                             stream_id,
                                                                             stream_label,
                                                                             reply_to,
                                                                             thread_pool,
                                                                             io_thread_pool),
                                                                      subscribe_to_("")
    {
    }

    void init() override
    {
        if (init_)
            return;
        conn_.init(agent_id_, erizo_id_, client_id_, stream_id_, stream_label_, false, reply_to_);
        init_ = true;
    }

    void setSubscribeTo(const std::string &subscribe_to)
    {
        subscribe_to_ = subscribe_to;
    }

    std::string getSubscribeTo()
    {
        return subscribe_to_;
    }

  private:
    std::string subscribe_to_;
};

#endif