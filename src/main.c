#include "db.h"
#include "request.h"
#include "response.h"
#include "server.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>

void end_transacao_async(void* _reqres){
    ReqRes* reqres = (ReqRes*)_reqres;
    char db_buffer[50] = {0};
    int n_writed = db_end_transacao(reqres->dbconn, 50, db_buffer);
    switch (n_writed) {
        case TRANSACAO_DB_ERROR: {
            SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
            return;
        }
        case TRANSACAO_INVALID: {
            SET_STATIC_RESPONSE(reqres->buffer, UNPROCESSABLE);
            return;
        }
        default:
            break;
    }
    if (n_writed < 0) {
        SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
        return;
    }
    snprintf(reqres->buffer, MAX_REQ_RESP_SIZE, OK_TRANSACAO, n_writed + 1, db_buffer);
    return;
}
void start_transacao_async(void* _reqres) {
    ReqRes* reqres = _reqres;
    Transacao transacao = {0};
    RequestData reqdata = {0};
    memcpy(&reqdata, reqres->db_data, sizeof(RequestData));
    free(reqres->db_data);
    switch (parse_transacoes(reqres->buffer, &reqdata, &transacao)) {
        case PARSER_INVALID: {
            SET_STATIC_RESPONSE(reqres->buffer, UNPROCESSABLE);
            return;
        }
        case PARSER_MEMORY: {
            SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
            return;
        }
        case PARSER_OK:
            break;
        default: {
            SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
            return;
        }
    }
    if (db_start_transacao(reqres->dbconn, &transacao) < 0){
        SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
        return;   
    }
    reqres->db_response_handler = end_transacao_async;
}

void end_extrato_async(void* _reqres){
    ReqRes* reqres = (ReqRes*)_reqres;
    char db_buffer[2000] = {0};
    int n_writed = db_end_extrato(reqres->dbconn, 2000-1, db_buffer);
    if (n_writed < 0) {
        SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
        return;
    }
    snprintf(reqres->buffer, MAX_REQ_RESP_SIZE, OK_EXTRATO, n_writed + 1, db_buffer);
    return;
}

void start_extrato_async(void* _reqres) {
    ReqRes* reqres = _reqres;
    RequestData reqdata = {0};
    memcpy(&reqdata, reqres->db_data, sizeof(RequestData));
    free(reqres->db_data);
    if (db_start_extrato(reqres->dbconn, reqdata.id) < 0){
        SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
        return;   
    }
    reqres->db_response_handler = end_extrato_async;
}

void controller(ReqRes* reqres) {
    RequestData* reqdata = calloc(1, sizeof(RequestData));
    if (reqdata == NULL){
        SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
        return;
    }
    switch ((request_parser(reqres->buffer, reqdata))) {
        case PARSER_MEMORY: {
            SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
            free(reqdata);
            return;
        }
        case PARSER_INVALID: {
            SET_STATIC_RESPONSE(reqres->buffer, UNPROCESSABLE);
            free(reqdata);
            return;
        }
        case PARSER_OK:
            break;
        default: {
            SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
            free(reqdata);
            return;
        }
    }
    if (reqdata->uri == URI_EXIT) {  // Circuit breaker
        SET_STATIC_RESPONSE(reqres->buffer, OK_EXIT);
        reqres->to_exit = 1;
        free(reqdata);
        return;
    }
    if (reqdata->uri == URI_PING){
        SET_STATIC_RESPONSE(reqres->buffer, OK_PING);
        free(reqdata);
        return;
    }

    if (reqdata->id >= 6) {
        SET_STATIC_RESPONSE(reqres->buffer, NOTFOUND);
        free(reqdata);
        return;
    }

    int good_request = (reqdata->method == GET && reqdata->uri == URI_EXTRATO) ||
                       (reqdata->method == POST && reqdata->uri == URI_TRANSACAO);
    if (!good_request) {
        SET_STATIC_RESPONSE(reqres->buffer, BADREQUEST);
        free(reqdata);
        return;
    }

    reqres->db_data = reqdata;
    if (reqdata->method == GET && reqdata->uri == URI_EXTRATO) {
        reqres->db_request_sender = start_extrato_async;
        reqres->db_data = reqdata;
        return;
    }
    if (reqdata->method == POST && reqdata->uri == URI_TRANSACAO) {
        reqres->db_request_sender = start_transacao_async;
        reqres->db_data = reqdata;
        return;
    }
    SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
}

int main(int argc, char const* argv[]) {

    if (argc != 2)
        FATAL3("%s","You need to provide only the port.");

    char* endptr = NULL;
    int port = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || port <= 0)
        FATAL3("Invalid port %s", argv[1]);

    server(controller, port);
    return 0;
}
