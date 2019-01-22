#include <unistd.h>
#include <signal.h>

#include "common/utils.h"
#include "common/config.h"
#include "core/erizo.h"

Erizo ez;

void signal_handler(int signo)
{
    printf("[%d]:recevice signo:%d,erizo process quit\n", getpid(), signo);
    ez.close();
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Usage:%s [agentID] [erizoID] [bridgeIP] [bridgePort]\n", argv[0]);
        return 0;
    }

    if (Utils::initPath())
    {
        printf("Change process path failed\n");
        return 1;
    }

    if (Config::getInstance()->init("erizo_config.json"))
    {
        printf("Config initialize failed\n");
        return 1;
    }

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    ez.init(argv[1], argv[2], argv[3], atoi(argv[4]));
    sleep(100000);
}