#ifndef SERVER_H
#define SERVER_H

#include "log.h"
#include "ioevent.h"

struct server {
    struct io_engine *eg;
    char *port;
};

#include "session.h"

struct server *server_create(char *port);

void server_destroy(void *ptr);

int server_start(struct server *svr); 

void acceptcb(struct evconnlistener *listener, int fd,
                     struct sockaddr *addr, int addrlen, void *arg);

#endif