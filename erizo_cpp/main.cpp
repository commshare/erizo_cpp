
#include "common/config.h"
#include "core/erizo.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    
    if (argc != 2)
        return 1;
    Erizo erizo;
    Config::getInstance()->init("config.json");
    erizo.init(argv[1]);
    sleep(3600);
    erizo.close();
    return 0;
}