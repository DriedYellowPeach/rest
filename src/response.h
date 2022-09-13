#ifndef RESPONSE_H
#define RESPONSE_H

#include <utils/buffer_rqueue.h>

struct response {
    struct buffer *body;
    struct headers *hdrs;
};

struct resposne *resposne_create(void);

void response_destroy(struct response *resp);

// submit the response with provider;
int write_header(struct response *resp);

int write_string(struct response *resp, char *str); 

int close_response_body(struct response *resp);

#endif