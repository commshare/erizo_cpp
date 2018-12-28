#ifndef PUBLISHER_H
#define PUBLISHER_H

#include "stream.h"

class Publisher : public Stream
{
  public:
    Publisher(const std::string &agent_id,
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
                                                                            io_thread_pool)
    {
    }

    void init() override
    {
        if (init_)
            return;
        conn_.init(agent_id_, erizo_id_, client_id_, stream_id_, stream_label_, true, reply_to_);
        init_ = true;
    }

    void addSubscriber(const std::string &stream_id, std::shared_ptr<erizo::MediaStream> connection)
    {
        conn_.addSubscriber(stream_id, connection);
    }
};

#endif