#include "headers.h"
#include "log.h"
#include <string.h>

struct headers *headers_create(void)
{
    struct headers *hdrs = (struct headers *)malloc(sizeof(struct headers));
    hdrs->length = 0;
    return hdrs;
}

void headers_destroy(struct headers* hdrs)
{
    free(hdrs);
}

int add_header(struct headers *hdrs, const char *name, const char *value)
{
    if (hdrs->length == MAXHDRS) {
        log_err("can't add more header");
        return -1;
    }
    hdrs->nvs[hdrs->length] = (nghttp2_nv){ (uint8_t *)name, (uint8_t *)value, strlen(name), strlen(value), NGHTTP2_FLAG_NONE };
    hdrs->length++;
    return 0;
}
