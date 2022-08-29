#include <event2/listener.h>
#include <event2/event.h>
#include <event2/bufferevent.h>

#include "log.h"

struct io_engine {
    struct event_base *evbase;
};

struct evconnlistener *
apply_listener(struct io_engine *eg, const char *listen_addr, evconnlistener_cb acceptcb, void *arg);

struct bufferevent *
apply_bufferevent(struct io_engine *eg, int fd, bufferevent_data_cb readcb, bufferevent_data_cb writecb, bufferevent_event_cb eventcb, void *ctx);