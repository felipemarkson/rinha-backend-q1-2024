#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#ifdef DOCKER
#include <postgresql/libpq-fe.h>
#else
#include <libpq-fe.h>
#endif

#define DEFAULT_SERVER_PORT 9999
// #define QUEUE_DEPTH 4294967296
// #define MAX_CONN 4294967296
#define MAX_REQ_RESP_SIZE 4096
#define SIZE_ARRAY(static_array) sizeof(static_array) / sizeof(*(static_array))
#ifndef LOG
#define LOG(template, ...) printf("LOG %s:%d -> " template, __FILE__, __LINE__, __VA_ARGS__)
#endif

typedef enum event_t{
    EVENT_ACCEPTTING = 0,
    EVENT_READING,
    EVENT_WRITING,
    EVENT_CLOSING,
    EVENT_DB_RESPONDING,
} Event;

typedef struct req_res_t {
    struct io_uring *ring;
    Event last_event;
    char buffer[MAX_REQ_RESP_SIZE + 1];
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    PGconn *db_conn;
    int to_exit;
} ReqRes;

typedef void (*Controller)(ReqRes*);

int server(Controller handler);
#endif
