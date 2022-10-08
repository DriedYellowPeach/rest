#include "request.h"
#include "log.h"
#include <unistd.h>

struct request *request_create(void)
{
    struct request *req = (struct request *)malloc(sizeof(struct request));
    req->path = NULL;
    req->method = NULL;
    // TODO: the mode setting 
    //req->body = buffer_create(DEFAULT_BODY_SIZE, RQUEUE_MODE_BLOCKING);
    if (pipe(req->body) < 0) {
       log_sys_err("create request body failed: ");
    }
}

void request_destroy(struct request *req)
{
    free(req->path);
    free(req->method);
    // buffer_destroy(req->body);
    headers_destroy(req->hdrs);
    close(req->body[0]);
    close(req->body[1]);
    free(req);
}

static void print_header(nghttp2_nv nv)
{
    int namelen = nv.namelen;
    char *name = nv.name;
    int valuelen = nv.valuelen;
    char *value = nv.value;

    size_t i;
    char *name_string = malloc((1 + namelen) * sizeof(uint8_t));
    for (i = 0; i < namelen; i++)
    {
        name_string[i] = name[i];
    }
    name_string[namelen] = '\0';

    char *value_string = malloc((1 + valuelen) * sizeof(uint8_t));
    for (i = 0; i < valuelen; i++)
    {
        value_string[i] = value[i];
    }
    value_string[valuelen] = '\0';

    log_info("HEADERS--> %10s: %s", name_string, value_string);
    free(name_string);
    free(value_string);
}

int read_header(struct request *req)
{
    int i;
    int length = req->hdrs->length;
    for (i = 0; i < length; i++) {
        print_header(req->hdrs->nvs[i]);
    }

    return 0;
}

// TODO: should support any length string
int read_body_as_string(struct request *req)
{
    char buf[1024];
    int nbytes;

    nbytes = buffer_read(req->body, buf, sizeof(buf)/sizeof(buf[0]));
    if (nbytes == sizeof(buf)/sizeof(buf[0])) {
        log_err("body too long");
        return -1;
    }

    if (buf[nbytes - 1] != '\0') {
        buf[sizeof(buf)/sizeof(buf[0]) - 1] = 0;
    }
    log_info("BODY--> %s", buf);
    return 0;
}

int data_chunk_recv_cb(struct request *req, const uint8_t *data, int size)
{
    int ret;

    ret = buffer_write(req->body, data, size);
    if (ret == -1) {
        log_err("write to request body error");
        return -1;
    }

    if (ret == -1) {
        log_err("request body buffered too large");
        return -1;
    }

    return 0;
}