
#include "common/config.h"
#include "core/erizo.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    Config *config = Config::getInstance();
    config->init("config.json");
    if (argc != 2)
        return 1;
    Erizo erizo;
    erizo.init(argv[1]);
    sleep(3600);
    erizo.close();
    return 0;
}