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
};

#endif