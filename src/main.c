#include "db.h"
#include "request.h"
#include "response.h"
#include "server.h"

#ifndef DB_ASYNC
void transacao(char* buffer, RequestData* reqdata) {
    Transacao transacao = {0};
    switch (parse_transacoes(buffer, reqdata, &transacao)) {
        case PARSER_INVALID: {
            SET_STATIC_RESPONSE(buffer, UNPROCESSABLE);
            return;
        }
        case PARSER_MEMORY: {
            SET_STATIC_RESPONSE(buffer, INTERNALERROR);
            return;
        }
        case PARSER_OK:
            break;
        default: {
            SET_STATIC_RESPONSE(buffer, INTERNALERROR);
            return;
        }
    }
    dbconn_t dbconn = db_connect();
    if (dbconn == NULL) {
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        return;
    }

    // {"limite": 100000, "saldo": -9098}
    char db_buffer[50] = {0};
    int n_writed = db_push_transacao(dbconn, &transacao, 50, db_buffer);
    db_disconnect(dbconn);
    switch (n_writed) {
        case TRANSACAO_DB_ERROR: {
            SET_STATIC_RESPONSE(buffer, INTERNALERROR);
            return;
        }
        case TRANSACAO_INVALID: {
            SET_STATIC_RESPONSE(buffer, UNPROCESSABLE);
            return;
        }
        default:
            break;
    }
    if (n_writed < 0) {
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        return;
    }
    snprintf(buffer, MAX_REQ_RESP_SIZE, OK_TRANSACAO, n_writed + 1, db_buffer);
    return;
}

void extrato(char* buffer, RequestData* reqdata) {
    dbconn_t dbconn = db_connect();
    if (dbconn == NULL) {
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        return;
    }
    char db_buffer[2000] = {0};
    int n_writed = db_get_extrato(dbconn, reqdata->id, 2000-1, db_buffer);
    db_disconnect(dbconn);
    if (n_writed < 0) {
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        return;
    }
    snprintf(buffer, MAX_REQ_RESP_SIZE, OK_EXTRATO, n_writed + 1, db_buffer);
    return;
}
#else

void end_transacao_async(void* _reqres){
    ReqRes* reqres = (ReqRes*)_reqres;
    char db_buffer[50] = {0};
    int n_writed = db_end_transacao(reqres->db_conn, 50, db_buffer);
    db_disconnect(reqres->db_conn);
    reqres->db_conn = NULL;
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
void start_transacao_async(ReqRes* reqres, const RequestData* reqdata) {
    Transacao transacao = {0};
    switch (parse_transacoes(reqres->buffer, reqdata, &transacao)) {
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
    dbconn_t dbconn = db_connect();
    if (dbconn == NULL) {
        SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
        return;
    }

    if (db_start_transacao(dbconn, &transacao) < 0){
        SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
        return;   
    }

    reqres->db_conn = dbconn;
    reqres->db_handler = end_transacao_async;
}

void end_extrato_async(void* _reqres){
    ReqRes* reqres = (ReqRes*)_reqres;
    char db_buffer[2000] = {0};
    int n_writed = db_end_extrato(reqres->db_conn, 2000-1, db_buffer);
    db_disconnect(reqres->db_conn);
    reqres->db_conn = NULL;
    if (n_writed < 0) {
        SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
        return;
    }
    snprintf(reqres->buffer, MAX_REQ_RESP_SIZE, OK_EXTRATO, n_writed + 1, db_buffer);
    return;
}

void start_extrato_async(ReqRes* reqres, const RequestData* reqdata) {
    dbconn_t dbconn = db_connect();
    if (dbconn == NULL) {
        SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
        return;
    }
    if (db_start_extrato(dbconn, reqdata->id) < 0){
        SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
        return;   
    }
    reqres->db_conn = dbconn;
    reqres->db_handler = end_extrato_async;
}
#endif

void controller(ReqRes* reqres) {
    RequestData reqdata = {0};
    switch ((request_parser(reqres->buffer, &reqdata))) {
        case PARSER_MEMORY: {
            SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
            return;
        }
        case PARSER_INVALID: {
            SET_STATIC_RESPONSE(reqres->buffer, UNPROCESSABLE);
            return;
        }
        case PARSER_OK:
            break;
        default: {
            SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
            return;
        }
    }
    if (reqdata.uri == URI_EXIT) {  // Circuit breaker
        SET_STATIC_RESPONSE(reqres->buffer, OK_EXIT);
        reqres->to_exit = 1;
        return;
    }
    if (reqdata.id >= 6) {
        SET_STATIC_RESPONSE(reqres->buffer, NOTFOUND);
        return;
    }

    int good_request = (reqdata.method == GET && reqdata.uri == URI_EXTRATO) ||
                       (reqdata.method == POST && reqdata.uri == URI_TRANSACAO);
    if (!good_request) {
        SET_STATIC_RESPONSE(reqres->buffer, BADREQUEST);
        return;
    }

#ifndef DB_ASYNC
    if (reqdata.method == GET && reqdata.uri == URI_EXTRATO) {
        extrato(reqres, &reqdata);
        return;
    }
    if (reqdata.method == POST && reqdata.uri == URI_TRANSACAO) {
        transacao(reqres->buffer, &reqdata);
        return;
    }
#else
    if (reqdata.method == GET && reqdata.uri == URI_EXTRATO) {
        start_extrato_async(reqres, &reqdata);
        return;
    }
    if (reqdata.method == POST && reqdata.uri == URI_TRANSACAO) {
        start_transacao_async(reqres, &reqdata);
        return;
    }
#endif
    SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
}

int main(int argc, char const* argv[]) {
    (void)argc;
    (void)argv;
    server(controller);
    return 0;
}