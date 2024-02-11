#include "db.h"
#include "request.h"
#include "response.h"
#include "server.h"

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
    int n_writed = db_get_extrato(dbconn, reqdata->id, 1400, db_buffer);
    db_disconnect(dbconn);
    if (n_writed < 0) {
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
    }
    snprintf(buffer, MAX_REQ_RESP_SIZE, OK_EXTRATO, n_writed + 1, db_buffer);
    return;
}

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

    if (reqdata.method == GET && reqdata.uri == URI_EXTRATO) {
        extrato(reqres->buffer, &reqdata);
        return;
    }
    if (reqdata.method == POST && reqdata.uri == URI_TRANSACAO) {
        transacao(reqres->buffer, &reqdata);
        return;
    }
    SET_STATIC_RESPONSE(reqres->buffer, INTERNALERROR);
}

int main(int argc, char const* argv[]) {
    (void)argc;
    (void)argv;
    server(controller);
    return 0;
}
