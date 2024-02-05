#include <errno.h>
#include <liburing.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define DEFAULT_SERVER_PORT 5555
#define QUEUE_DEPTH 4096
#define MAX_CONN 4096
#define MAX_REQ_RESP_SIZE 4096
#define FATAL_SYS()                                                    \
    do {                                                               \
        fprintf(stderr, "FATAL IN %s:%d -> %s \n", __FILE__, __LINE__, \
                strerror(errno));                                      \
        exit(1);                                                       \
    } while (0)

#define FATAL_IO(err_io)                                                  \
    do {                                                                  \
        fprintf(stderr, "FATAL IO IN %s:%d -> %s \n", __FILE__, __LINE__, \
                strerror(-(int)(err_io)));                                \
        exit(1);                                                          \
    } while (0)
#define SIZE_ARRAY(static_array) sizeof(static_array) / sizeof(*(static_array))
#define LOG(template, ...) printf("LOG %s:%d -> " template, __FILE__, __LINE__, __VA_ARGS__)

typedef enum event_t{
    EVENT_ACCEPTTING = 0,
    EVENT_READING,
    EVENT_WRITING,
    ENVENT_CLOSING,
} Event;

typedef struct req_res_t {
    Event last_event;
    char buffer[MAX_REQ_RESP_SIZE];
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
} ReqRes;

struct io_uring ring = {0};
int server_fd = 0;

void init_server_fd(int port) {
    int reuse_addr = 1;
    struct sockaddr_in server_addr = {.sin_family = AF_INET, .sin_port = htons(port)};
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) FATAL_SYS();
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0)
        FATAL_SYS();
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        FATAL_SYS();
    if (listen(server_fd, MAX_CONN) < 0) FATAL_SYS();
}

int push_accepting(void){
    ReqRes *req = calloc(1, sizeof(ReqRes));
    if(req == NULL)
        FATAL_SYS(); // Memory error, just dying...
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);

    req->last_event = EVENT_ACCEPTTING;
    io_uring_prep_accept(sqe, server_fd, (struct sockaddr *)&(req->client_addr),
                            &req->client_addr_len, 0);
    io_uring_sqe_set_data(sqe, req);
    int ret = 0;
    if((ret = io_uring_submit(&ring)) < 0){
        fprintf(stderr, "ERROR: Could not accept the request: %s\n", strerror(-ret));
        return -1;
    }
    return 0;
}

int push_reading(ReqRes *req, int client_fd){
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);

    req->client_fd = client_fd;
    req->last_event = EVENT_READING;

    io_uring_prep_read(sqe, req->client_fd, req->buffer, MAX_REQ_RESP_SIZE, 0);
    io_uring_sqe_set_data(sqe, req);

    int ret = 0;
    if((ret = io_uring_submit(&ring)) < 0){
        fprintf(stderr, "ERROR: Could not read the request: %s\n", strerror(-ret));
        return -1;
    }

    return 0;
}

int push_writing(ReqRes *req){
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    req->last_event = EVENT_WRITING;
    io_uring_prep_write(sqe, req->client_fd, req->buffer, strnlen(req->buffer, MAX_REQ_RESP_SIZE), 0);
    io_uring_sqe_set_data(sqe, req);
    int ret = 0;
    if((ret = io_uring_submit(&ring)) < 0){
        fprintf(stderr, "ERROR: Could not write the response: %s\n", strerror(-ret));
        return -1;
    }
    return 0;
}

int push_close_connection(ReqRes *req){
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    req->last_event = ENVENT_CLOSING;
    io_uring_prep_close(sqe, req->client_fd);
    io_uring_sqe_set_data(sqe, req);
    int ret = 0;
    if((ret = io_uring_submit(&ring)) < 0){
        fprintf(stderr, "ERROR: Could not close the connection: %s\n", strerror(-ret));
        return -1;
    }
    return 0;
}

void deinit_request(ReqRes **req){
    if(*req == NULL) return;
    free(*req);
    *req = NULL;
}

const char response[] =
    "HTTP/1.1 200 OK\r\n"
    "Connection: close\r\n"
    "Content-type: text/plain\r\n"
    "Content-Length:14\r\n"
    "\r\n"
    "Hello, World!\n";

void handle_request(ReqRes *req){
    printf("Request recived!:\n");
    printf("%.*s", MAX_REQ_RESP_SIZE, req->buffer);
    strncpy(req->buffer, response, SIZE_ARRAY(response));
}


void server_loop(void) {
    if (push_accepting() < 0)
        exit(1);

    printf("Listening in %d\n", DEFAULT_SERVER_PORT);
    while (1) {
        int ret;
        struct io_uring_cqe *cqe = NULL;
        if((ret = io_uring_wait_cqe(&ring, &cqe)) != 0)
            FATAL_IO(ret);
        int result = cqe->res;

        ReqRes *req = (ReqRes *)cqe->user_data;
        if (result < 0) {
            fprintf(stderr, "ERROR: Could not process the last event (%d): %s\n", req->last_event, strerror(-result));
            continue;
        }

        switch (req->last_event) {
            case EVENT_ACCEPTTING: {
                LOG("%s", "The server had accepted a connection!\n");
                (void)push_reading(req, result);
                (void)push_accepting(); // We add a new accepting for the next request.
                break;
            }
            case EVENT_READING: {
                LOG("%s", "The server had read data from the connection!\n");
                handle_request(req);
                push_writing(req);
                break;
            }
            case EVENT_WRITING: {
                LOG("%s", "The server had write data to the connection!\n");
                push_close_connection(req);
                break;
            }
            case ENVENT_CLOSING: {
                LOG("%s", "The server had close the connection!\n");
                deinit_request(&req);
                break;
            }
            default: {
                fprintf(stderr, "ERROR: UNREACHABLE!!!! ReqRes last envent: %d", req->last_event);
                break;
            }
        }
        io_uring_cqe_seen(&ring, cqe);
    }
}


void sigint_handler(int signo) {
    (void)signo;
    printf("Exiting!\n");
    io_uring_queue_exit(&ring);
    exit(0);
}

int main(void) {
    init_server_fd(DEFAULT_SERVER_PORT);
    if (signal(SIGINT, sigint_handler) == SIG_ERR) FATAL_SYS();
    int ret;
    if ((ret = -io_uring_queue_init(QUEUE_DEPTH, &ring, 0)) != 0) FATAL_IO(ret);
    server_loop();
    return 0;
}
