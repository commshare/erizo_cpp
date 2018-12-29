#ifndef CONNECTION_H
#define CONNECTION_H

#include <map>
#include <memory>
#include <string>

#include <logger.h>
#include <WebRtcConnection.h>
#include <MediaStream.h>
#include <OneToManyProcessor.h>
#include <thread/ThreadPool.h>
#include <thread/IOThreadPool.h>

#include "rabbitmq/amqp_helper.h"

class ConnectionListener
{
public:
  virtual void onEvent(const std::string &reply_to, const std::string &event) = 0;
};

class Connection : public erizo::WebRtcConnectionEventListener
{
  DECLARE_LOGGER();

public:
  Connection();
  ~Connection();

  void setConnectionListener(ConnectionListener *listener) { listener_ = listener; }

  void init(const std::string &agent_id,
            const std::string &erizo_id,
            const std::string &client_id,
            const std::string &stream_id,
            const std::string &label,
            bool is_publisher,
            const std::string &reply_to,
            std::shared_ptr<erizo::ThreadPool> thread_pool,
            std::shared_ptr<erizo::IOThreadPool> io_thread_pool);
  void close();

  void notifyEvent(erizo::WebRTCEvent newEvent, const std::string &message, const std::string &stream_id = "") override;
  int setRemoteSdp(const std::string &sdp);
  int addRemoteCandidate(const std::string &mid, int sdp_mine_index, const std::string &sdp);
  void addSubscriber(const std::string &client_id, std::shared_ptr<erizo::MediaStream> connection);
  std::shared_ptr<erizo::MediaStream> getMediaStream();
  
  const std::string &getStreamId()
  {
    return stream_id_;
  }

private:
  std::shared_ptr<erizo::WebRtcConnection> webrtc_connection_;
  std::shared_ptr<erizo::OneToManyProcessor> otm_processor_;
  std::shared_ptr<erizo::MediaStream> media_stream_;
  ConnectionListener *listener_;

  std::string agent_id_;
  std::string erizo_id_;
  std::string client_id_;
  std::string stream_id_;
  bool is_publisher_;
  std::string reply_to_;

  std::shared_ptr<erizo::ThreadPool> thread_pool_;
  std::shared_ptr<erizo::IOThreadPool> io_thread_pool_;

  bool init_;
};

#endif