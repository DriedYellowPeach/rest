#include "server.h"


int main(void)
{
    struct server *svr = server_create("8180");
    server_start(svr);
}