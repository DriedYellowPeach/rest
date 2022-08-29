#include "log.h"
#include "session.h"
#include "ioevent.h"

struct server {
    struct io_engine *eg;
};


void acceptcb(struct evconnlistener *listener, int fd,
                     struct sockaddr *addr, int addrlen, void *arg);