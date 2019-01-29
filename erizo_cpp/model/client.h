#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <memory>
#include <map>

class Connection;

struct Client
{
    std::string id;
    std::map<std::string, std::shared_ptr<Connection>> subscribers;
    std::map<std::string, std::shared_ptr<Connection>> publishers;
};

#endif