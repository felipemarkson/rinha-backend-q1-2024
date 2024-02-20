#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include "db.h"

#define DEFAULT_SERVER_PORT 9999
#define MAX_REQ_RESP_SIZE 4096

typedef enum event_t{
    EVENT_ACCEPTTING = 0,
    EVENT_READING,
    EVENT_WRITING,
    EVENT_CLOSING,
    EVENT_DB_RESPONDING,
} Event;

typedef void (*DBFunc)(void* reqres);

typedef struct req_res_t {
    struct io_uring *ring;
    Event last_event;
    char buffer[MAX_REQ_RESP_SIZE + 1];
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    DBFunc db_request_sender;
    DBFunc db_response_handler;
    void* db_data;
    dbconn_t dbconn;
    int to_exit;
} ReqRes;

typedef void (*Controller)(ReqRes*);

int server(Controller handler, int port);
#endif
