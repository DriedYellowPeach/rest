#include "server.h"


int main(void)
{
    struct server *svr = server_create("8080");
    server_start(svr);
}