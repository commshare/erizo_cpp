#include <unistd.h>
#include <signal.h>

#include <dtls/DtlsSocket.h>
#include <BridgeIO.h>

#include "common/utils.h"
#include "common/config.h"
#include "core/erizo.h"

static log4cxx::LoggerPtr logger;
static bool run = true;

void signal_handler(int signo)
{
    run = false;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);

    pid_t pid = getpid();
    char buf[1024];
    sprintf(buf, "[%d]", pid);
    logger = log4cxx::Logger::getLogger(buf);

    if (argc < 5)
    {
        ELOG_WARN("Usage:%s [agentID] [erizoID] [bridgeIP] [bridgePort]", argv[0]);
        return 0;
    }

    if (Utils::initPath())
    {
        ELOG_ERROR("working path initialize failed");
        return 1;
    }

    if (Config::getInstance()->init("config.json"))
    {
        ELOG_ERROR("load configure file failed");
        return 1;
    }

    dtls::DtlsSocketContext::globalInit();

    if (erizo::BridgeIO::getInstance()->init(argv[3], atoi(argv[4]), Config::getInstance()->bridge_io_worker_num))
    {
        ELOG_ERROR("bridge-io initialize failed");
        return 1;
    }

    if (Erizo::getInstance()->init(argv[1], argv[2], argv[3], atoi(argv[4])))
    {
        ELOG_ERROR("erizo initialize failed");
        return 1;
    }

    while (run)
        sleep(1);

    Erizo::getInstance()->close();
    erizo::BridgeIO::getInstance()->close();
    return 0;
}