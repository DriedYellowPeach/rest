#include <nghttp2/nghttp2.h>

#define MAXHDRS 20
#define MAKE_NV(NAME, VALUE)                                                   \
  {                                                                            \
    (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1,    \
        NGHTTP2_NV_FLAG_NONE                                                   \
  }


struct headers {
    nghttp2_nv nvs[MAXHDRS];
    int length;
};

struct headers *headers_create(void);

void headers_destroy(struct headers* hdrs); 

int add_header(struct headers *hdrs, char *name, char *value);