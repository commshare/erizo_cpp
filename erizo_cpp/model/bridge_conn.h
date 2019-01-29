#ifndef BRIDGE_CONNECTION_H
#define BRIDGE_CONNECTION_H

#include <memory>

#include <logger.h>

namespace erizo
{
class BridgeMediaStream;
class OneToManyProcessor;
class IOThreadPool;
class MediaStream;
}; // namespace erizo

class BridgeConn
{
public:
  BridgeConn();
  ~BridgeConn();

  void init(const std::string &bridge_stream_id,
            const std::string &src_stream_id,
            const std::string &ip,
            uint16_t port,
            std::shared_ptr<erizo::IOThreadPool> io_thread_pool,
            bool is_send,
            uint32_t video_ssrc = 0,
            uint32_t audio_ssrc = 0);
  void close();

  void addSubscriber(const std::string &client_id, std::shared_ptr<erizo::MediaStream> media_stream);
  void removeSubscriber(const std::string &client_id);
  std::shared_ptr<erizo::BridgeMediaStream> getBridgeMediaStream();

  const std::string &getSrcStreamId()
  {
    return src_stream_id_;
  }

  const std::string &getBridgeStreamId()
  {
    return bridge_stream_id_;
  }

private:
  std::shared_ptr<erizo::BridgeMediaStream> bridge_media_stream_;
  std::shared_ptr<erizo::OneToManyProcessor> otm_processor_;

  std::string bridge_stream_id_;
  std::string src_stream_id_;
  bool is_send_;
  bool init_;
};

#endif