#ifndef STREAM_H
#define STREAM_H

#include <memory>
#include <thread/ThreadPool.h>
#include <thread/IOThreadPool.h>

#include "connection.h"

class Stream
{
  public:
    Stream(const std::string &agent_id,
           const std::string &erizo_id,
           const std::string &client_id,
           const std::string &stream_id,
           const std::string &stream_label,
           const std::string &reply_to,
           std::shared_ptr<erizo::ThreadPool> thread_pool,
           std::shared_ptr<erizo::IOThreadPool> io_thread_pool) : agent_id_(agent_id),
                                                                  erizo_id_(erizo_id),
                                                                  client_id_(client_id),
                                                                  stream_id_(stream_id),
                                                                  stream_label_(stream_label),
                                                                  reply_to_(reply_to),
                                                                  conn_(thread_pool, io_thread_pool),
                                                                  init_(false)
    {
    }

    virtual void init()
    {
        if (init_)
            return;
        conn_.init(agent_id_, erizo_id_, client_id_, stream_id_, stream_label_, false, reply_to_);
        init_ = true;
    }

    void setConnectionListener(ConnectionListener *listener)
    {
        conn_.setConnectionListener(listener);
    }

    void setRemoteSdp(const std::string &sdp)
    {
        conn_.setRemoteSdp(sdp);
    }

    void addRemoteCandidate(const std::string &mid, int sdp_mine_index, const std::string &sdp)
    {
        conn_.addRemoteCandidate(mid, sdp_mine_index, sdp);
    }

    std::shared_ptr<erizo::MediaStream> getMediaStream()
    {
        return conn_.getMediaStream();
    }

    

    std::string getStreamLabel() { return stream_label_; }
    std::string getStreamId() { return stream_id_; }

    void close()
    {
        if (!init_)
            return;
        conn_.close();
        init_ = false;
    }

  protected:
    std::string agent_id_;
    std::string erizo_id_;
    std::string client_id_;
    std::string stream_id_;
    std::string stream_label_;
    std::string reply_to_;
    Connection conn_;
    bool init_;
};

#endif