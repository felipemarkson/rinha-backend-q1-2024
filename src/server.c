#include <errno.h>
#include <liburing.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "server.h"
#include "log.h"
#include "response.h"
#define MAXCONN SOMAXCONN

static struct io_uring ring = {0};
static int server_fd = 0;
#define REQRES_POOL_SIZE 4096
static ReqRes REQRES_POOL[REQRES_POOL_SIZE] = {0};
#define DBCONNS_POOL_SIZE 75
static char DBCONNS_USED[DBCONNS_POOL_SIZE] = {0};
static dbconn_t DBCONNS_POOL[DBCONNS_POOL_SIZE] = {0};

typedef struct queue_item_t {
    void* next;
    ReqRes* item;
} QueueItem;
QueueItem* first = NULL;
QueueItem* last = NULL;

void queu_push(ReqRes* reqres){
    if (first == NULL && last != NULL)
        FATAL3("UNREACHABLE! first: {%p}, last: {%p}", (void*)first, (void*)last);
    QueueItem* qitem = calloc(1, sizeof(QueueItem));
    if (qitem == NULL)
        FATAL3("%s" ,"MEMERROR");
    qitem->item = reqres;
    if (last != NULL)
        last->next = qitem;
    last = qitem;
    if(first == NULL)
        first = qitem;
}
ReqRes* queu_pop(void){
    if (first == NULL && last != NULL)
        FATAL3("UNREACHABLE! first: {%p}, last: {%p}", (void*)first, (void*)last);
    if(first == NULL)
        return NULL;
    ReqRes* ret = first->item;
    QueueItem* new_first = first->next;
    free(first);
    first = new_first;
    if (first == NULL)
        last = NULL;
    else
        last = first->next;
    return ret;
}

void free_queu(){
    while (queu_pop()) continue;
}



void init_dbconns_arena(void){
    for (int i = 0; i < DBCONNS_POOL_SIZE; i++){
        DBCONNS_POOL[i] = db_connect();
        if(DBCONNS_POOL[i] == NULL)
            FATAL3("%s","Invalid DB connection!");
    }
}
void deinit_dbconns_arena(void){
    for (int i = 0; i < DBCONNS_POOL_SIZE; i++){
        db_disconnect(DBCONNS_POOL[i]);
    }
}
dbconn_t get_dbconn(void) {
    for (int i = 0; i < DBCONNS_POOL_SIZE; i++){
        if (DBCONNS_USED[i] == 0){
            DBCONNS_USED[i] = (char)1;
            return DBCONNS_POOL[i];
        }
    }
    return NULL;
}
void release_dbconn(dbconn_t dbconn){
    for (int i = 0; i < DBCONNS_POOL_SIZE; i++){
        if (dbconn == DBCONNS_POOL[i]){
            DBCONNS_USED[i] = (char)0;
            return;
        }
    }
    FATAL3("%s","UNREACHABLE!"); 
}

ReqRes* init_reqres(void){
    int i;
    char found = 0;
    for (i = 0; i < REQRES_POOL_SIZE; i++){
        if (REQRES_POOL[i].ring == NULL){
            found = 1;
            break;
        }
    }
    if (found == 0){ // NOT FOUND
        LOGERR("%s","There is no ReqRes available!");
        return NULL;
    }
    REQRES_POOL[i].ring = &ring;
    return REQRES_POOL + i;
}
static void deinit_reqres(ReqRes *req){
    memset(req, 0, sizeof(ReqRes));
}

static void init_server_fd(int port) {
    int reuse_addr = 1;
    struct sockaddr_in server_addr = {.sin_family = AF_INET, .sin_port = htons(port)};
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) FATAL();
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0)
        FATAL();
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        FATAL();
    if (listen(server_fd, MAXCONN) < 0) FATAL();
}

static int push_accepting(ReqRes **out){
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    ReqRes *req = init_reqres();
    if(req == NULL)
        FATAL3("%s", "There is no ReqRes available");

    req->last_event = EVENT_ACCEPTTING;
    io_uring_prep_accept(sqe, server_fd, (struct sockaddr *)&(req->client_addr),
                            &req->client_addr_len, 0);
    io_uring_sqe_set_data(sqe, req);
    int ret = 0;
    if((ret = io_uring_submit(&ring)) < 0){
        LOGERR("ERROR: Could not accept the request: %s\n", strerror(-ret));
        return -1;
    }
    if (out != NULL)
        *out = req;
    return 0;
}

static int push_reading(ReqRes *req, int client_fd){
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);

    req->client_fd = client_fd;
    req->last_event = EVENT_READING;

    io_uring_prep_read(sqe, req->client_fd, req->buffer, MAX_REQ_RESP_SIZE, 0);
    io_uring_sqe_set_data(sqe, req);

    int ret = 0;
    if((ret = io_uring_submit(&ring)) < 0){
        LOGERR("ERROR: Could not read the request: %s\n", strerror(-ret));
        return -1;
    }

    return 0;
}

static int push_writing(ReqRes *req){
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    req->last_event = EVENT_WRITING;
    io_uring_prep_write(sqe, req->client_fd, req->buffer, strnlen(req->buffer, MAX_REQ_RESP_SIZE), 0);
    io_uring_sqe_set_data(sqe, req);
    int ret = 0;
    if((ret = io_uring_submit(&ring)) < 0){
        LOGERR("ERROR: Could not write the response: %s\n", strerror(-ret));
        return -1;
    }
    return 0;
}

static int push_close_connection(ReqRes *req){
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    req->last_event = EVENT_CLOSING;
    io_uring_prep_close(sqe, req->client_fd);
    io_uring_sqe_set_data(sqe, req);
    int ret = 0;
    if((ret = io_uring_submit(&ring)) < 0){
        LOGERR("ERROR: Could not close the connection: %s\n", strerror(-ret));
        return -1;
    }
    return 0;
}

static int push_db_responding(ReqRes *req){
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    req->last_event = EVENT_DB_RESPONDING;
    int db_fd = db_socket(req->dbconn);
    if(db_fd < 0)
        FATAL3("%s", "ERROR: Invalid DB file descriptor!");

    io_uring_prep_read(sqe, db_fd, NULL, 0, 0); // We do not read. it is done by the PG.
    io_uring_sqe_set_data(sqe, req);
    int ret = 0;
    if((ret = io_uring_submit(&ring)) < 0)
        FATAL3("ERROR: Could not read the db response: %s\n", strerror(-ret));
    return 0;
}

void queu_dispatch(){
    dbconn_t dbconn = get_dbconn();
    while (dbconn != NULL){
        ReqRes* reqres = queu_pop();
        if (reqres == NULL)
            break;
        reqres->dbconn = dbconn;
        reqres->db_request_sender(reqres);
        if (reqres->db_response_handler == NULL) {
            release_dbconn(dbconn);
            push_writing(reqres);
            continue;
        }
        push_db_responding(reqres);
        dbconn = get_dbconn();
    }
    if (dbconn != NULL)
        release_dbconn(dbconn);
}

void server_loop(Controller handler, int port) {
    if (push_accepting(NULL) < 0)
        exit(1);

    printf("Listening in %d\n", port);
    while (1) {
        queu_dispatch();
        int ret;
        struct io_uring_cqe *cqe = NULL;
        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret != 0){
            if (ret != -4) // gdb interruped
                FATAL2(ret);
            else
                continue;
        }
        int result = cqe->res;
        ReqRes *req = (ReqRes *)cqe->user_data;
        if (result < 0) {
            LOGERR("ERROR: Could not process the last event (%d): %s\n", req->last_event, strerror(-result));
            goto ioseen;
        }

        switch (req->last_event) {
            case EVENT_ACCEPTTING: {
                LOG("%s", "The server had accepted a connection!\n");
                (void)push_reading(req, result);
                (void)push_accepting(NULL); // We add a new accepting for the next request.
                break;
            }
            case EVENT_READING: {
                LOG("%s", "The server had read data from the connection!\n");
                handler(req);
                if (req->db_request_sender == NULL)
                {
                    LOG("%s", "no DB requested...\n");
                    push_writing(req);
                }
                else
                    queu_push(req);
                break;
            }
            case EVENT_DB_RESPONDING: {
                req->db_response_handler(req);
                release_dbconn(req->dbconn);
                push_writing(req);
                break;
            }
            case EVENT_WRITING: {
                LOG("%s", "The server had write data to the connection!\n");
                push_close_connection(req);
                break;
            }
            case EVENT_CLOSING: {
                LOG("%s", "The server had close the connection!\n");
                deinit_reqres(req);
                break;
            }
            default: {
                FATAL3("ERROR: UNREACHABLE!!!! ReqRes last envent: %d", req->last_event);
                break;
            }
        }
ioseen:
        io_uring_cqe_seen(&ring, cqe);
        if (req->to_exit){
            write(req->client_fd, req->buffer, strlen(req->buffer));
            for (size_t i = 0; i < REQRES_POOL_SIZE; i++)
                close(REQRES_POOL[i].client_fd);
            return;
        }
    }
}


static void sigint_handler(int signo) {
    (void)signo;
    printf("Exiting!\n");
    io_uring_queue_exit(&ring);
    deinit_dbconns_arena();
    free_queu();
    exit(0);
}

int server(Controller handler, int port) {
    init_dbconns_arena();
    init_server_fd(port);
    if (signal(SIGINT, sigint_handler) == SIG_ERR) FATAL();
    int ret;
    if ((ret = io_uring_queue_init(4096, &ring, 0)) != 0) FATAL2(ret);
    server_loop(handler, port);
    sigint_handler(0);
    return 0;
}
