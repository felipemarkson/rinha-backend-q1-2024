#include <netinet/in.h>

#define DEFAULT_SERVER_PORT 5555
#define QUEUE_DEPTH 4096
#define MAX_CONN 4096
#define MAX_REQ_RESP_SIZE 4096
#define SIZE_ARRAY(static_array) sizeof(static_array) / sizeof(*(static_array))
#define LOG(template, ...) printf("LOG %s:%d -> " template, __FILE__, __LINE__, __VA_ARGS__)

typedef enum event_t{
    EVENT_ACCEPTTING = 0,
    EVENT_READING,
    EVENT_WRITING,
    EVENT_CLOSING,
} Event;

typedef struct req_res_t {
    struct io_uring *ring;
    Event last_event;
    char buffer[MAX_REQ_RESP_SIZE + 1];
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
} ReqRes;

typedef void (*RequestHandler)(ReqRes*);

int server(RequestHandler handler);