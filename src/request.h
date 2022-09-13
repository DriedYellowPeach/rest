#ifndef REQUEST_H
#define REQUEST_H

#include "utils/buffer_rqueue.h"
#include "headers.h"

#define DEFAULT_BODY_SIZE 16
#define MAX_STRING_SIZE 1024

struct request {
    char *path;
    char *method;
    struct buffer *body;
    struct headers *hdrs;
    char bdy_string[MAX_STRING_SIZE];
};

struct request *request_create(void);

void request_destroy(struct request *req);

int read_header(struct request *req);

int read_body_as_string(struct request *req);

int data_chunk_recv_cb(struct request *req, const uint8_t *data, int size);

#endif