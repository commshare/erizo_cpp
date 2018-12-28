#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>
#include <memory>

#include "connection.h"

struct Client
{
    std::string id;
    std::vector<std::shared_ptr<Connection>> subscribers;
    std::vector<std::shared_ptr<Connection>> publishers;

    std::shared_ptr<Connection> getConnection(const std::string &stream_id)
    {
        {
            auto itc = std::find_if(publishers.begin(), publishers.end(), [&](std::shared_ptr<Connection> connection) {
                if (!connection->getStreamId().compare(stream_id))
                    return true;
                return false;
            });
            if (itc != publishers.end())
                return *itc;
        }
        {
            auto itc = std::find_if(subscribers.begin(), subscribers.end(), [&](std::shared_ptr<Connection> connection) {
                if (!connection->getStreamId().compare(stream_id))
                    return true;
                return false;
            });
            if (itc != subscribers.end())
                return *itc;
        }
        return nullptr;
    }
};

#endif