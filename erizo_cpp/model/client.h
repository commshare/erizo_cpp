#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>
#include <memory>

#include "stream.h"
#include "subscriber.h"
#include "publisher.h"

class Client
{
  public:
    Client(const std::string &id) : id_(id) {}

    void addPublisher(std::shared_ptr<Publisher> publisher)
    {
        publishers_.push_back(publisher);
    }

    void addSubscriber(std::shared_ptr<Subscriber> subscriber)
    {
        subscribers_.push_back(subscriber);
    }

    std::shared_ptr<Stream> getStream(const std::string &stream_id)
    {
        auto publishers_it = std::find_if(publishers_.begin(), publishers_.end(), [&](std::shared_ptr<Publisher> publisher) {
            std::string id = publisher->getStreamId();
            if (!id.compare(stream_id))
                return true;
            return false;
        });
        if (publishers_it != publishers_.end())
            return *publishers_it;

        auto subscribers_it = std::find_if(subscribers_.begin(), subscribers_.end(), [&](std::shared_ptr<Subscriber> subscriber) {
            std::string id = subscriber->getStreamId();
            if (!id.compare(stream_id))
                return true;
            return false;
        });
        if (subscribers_it != subscribers_.end())
            return *subscribers_it;
        return nullptr;
    }

    std::shared_ptr<Publisher> getPublisher(const std::string &publisher_id)
    {
        auto publishers_it = std::find_if(publishers_.begin(), publishers_.end(), [&](std::shared_ptr<Publisher> publisher) {
            std::string id = publisher->getStreamId();
            if (!id.compare(publisher_id))
                return true;
            return false;
        });
        if (publishers_it != publishers_.end())
            return *publishers_it;

        return nullptr;
    }

    std::shared_ptr<Subscriber> getSubscriber(const std::string &publisher_id)
    {
        auto subscribers_it = std::find_if(subscribers_.begin(), subscribers_.end(), [&](std::shared_ptr<Subscriber> subscriber) {
            std::string id = subscriber->getSubscribeTo();
            if (!id.compare(publisher_id))
                return true;
            return false;
        });
        if (subscribers_it != subscribers_.end())
            return *subscribers_it;

        return nullptr;
    }

  private:
    std::string id_;
    std::vector<std::shared_ptr<Subscriber>> subscribers_;
    std::vector<std::shared_ptr<Publisher>> publishers_;
};

#endif