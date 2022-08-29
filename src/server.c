#include <server.h>


void acceptcb(struct evconnlistener *listener, int fd,
                     struct sockaddr *addr, int addrlen, void *arg) {
  struct server *svr = (struct server *)arg;
  (void)listener;
  log_info("accepted on fd: %d", fd);
  create_session(svr, fd, addr, addrlen);
  log_info("session created for fd: %d", fd);
} 