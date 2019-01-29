#ifndef CONNECTION_H
#define CONNECTION_H

#include <map>
#include <memory>
#include <string>

#include <logger.h>
#include <WebRtcConnection.h>

namespace erizo
{
class MediaStream;
class BridgeMediaStream;
class OneToManyProcessor;
class ThreadPool;
class IOThreadPool;
}; // namespace erizo

class ConnectionListener;
class AMQPHelper;

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

  int setRemoteSdp(const std::string &sdp);
  int addRemoteCandidate(const std::string &mid, int sdp_mine_index, const std::string &sdp);
  void addSubscriber(const std::string &client_id, std::shared_ptr<erizo::MediaStream> media_stream);
  void addSubscriber(const std::string &bridge_stream_id, std::shared_ptr<erizo::BridgeMediaStream> bridge_media_stream);
  void removeSubscriber(const std::string &client);
  std::shared_ptr<erizo::MediaStream> getMediaStream();

  const std::string &getStreamId()
  {
    return stream_id_;
  }

  void setRoomId(const std::string &room_id)
  {
    room_id_ = room_id;
  }

  void notifyEvent(erizo::WebRTCEvent newEvent, const std::string &message, const std::string &stream_id = "") override;

private:
  std::shared_ptr<erizo::WebRtcConnection> webrtc_connection_;
  std::shared_ptr<erizo::OneToManyProcessor> otm_processor_;
  std::shared_ptr<erizo::MediaStream> media_stream_;
  ConnectionListener *listener_;

  std::string agent_id_;
  std::string erizo_id_;
  std::string room_id_;
  std::string client_id_;
  std::string stream_id_;
  std::string label_;
  bool is_publisher_;
  std::string reply_to_;

  bool init_;
};

#endif