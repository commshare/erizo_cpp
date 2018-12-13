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

class Connection : public erizo::WebRtcConnectionEventListener
{
  DECLARE_LOGGER();

public:
  Connection(const std::string &id, std::shared_ptr<erizo::ThreadPool> thread_pool, std::shared_ptr<erizo::IOThreadPool> io_thread_pool);
  ~Connection();

  void init(const std::string &stream_id,
            const std::string &stream_label,
            bool is_publisher,
            std::shared_ptr<AMQPHelper> amqp_helper,
            const std::string &reply_to,
            int corrid);
  void notifyEvent(erizo::WebRTCEvent newEvent, const std::string &message, const std::string &stream_id = "") override;

  bool setRemoteSdp(const std::string &sdp);
  bool addRemoteCandidate(const std::string &mid, int sdp_mine_index, const std::string &sdp);
  void addSubscriber(const std::string &client_id, std::shared_ptr<erizo::MediaStream> connection);
  bool isReadyToSubscribe();
  std::shared_ptr<erizo::MediaStream> getMediaStream();

  void close();
private:
  void initWebRtcConnection();

private:
  std::string id_;
  std::shared_ptr<erizo::ThreadPool> thread_pool_;
  std::shared_ptr<erizo::IOThreadPool> io_thread_pool_;
  bool init_;
  bool ready_;
  bool is_publisher_;
  std::string stream_id_;

  std::string reply_to_;
  int corrid_;
  std::shared_ptr<AMQPHelper> amqp_helper_;
  std::shared_ptr<erizo::WebRtcConnection> webrtc_connection_;
  std::shared_ptr<erizo::OneToManyProcessor> otm_processor_;
  std::shared_ptr<erizo::MediaStream> media_stream_;
};

#endif