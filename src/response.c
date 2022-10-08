#include "response.h"
#include "log.h"
#include <unistd.h>


struct resposne *resposne_create(void)
{
    struct response *resp= (struct resposne *)malloc(sizeof(struct response));
    if (pipe(resp->body) < 0) {
       log_sys_err("create response body failed: ");
    }
}

void response_destroy(struct response *resp)
{
    close(resp->body[0]);
    close(resp->body[1]);
}

int write_header(struct response *resp)
{
    return 0;
}

int response_write_string(struct response *resp, char *str)
{
    ssize_t writelen;

    writelen = write(resp->body[1], str, sizeof(str)-1);

    if (writelen != sizeof(str) - 1) {
        //close(resp->body[0]);
        return -1;
    }

    return 0;
}