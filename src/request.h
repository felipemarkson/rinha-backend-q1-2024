#ifndef REQUEST_H
#define REQUEST_H
#include "server.h"

typedef enum uri_t{
    URI_TRANSACAO = 0,
    URI_EXTRATO,
    URI_EXIT,
    URI_PING
} Uri;

typedef enum method_t { GET, POST } Method;

typedef enum parser_error_t{
    PARSER_OK = 0,
    PARSER_MEMORY = -1,
    PARSER_INVALID = -2,
} ParserError;

typedef struct request_data_t {
    Uri uri;
    Method method;
    int body_loc;
    uint64_t id;
} RequestData;

typedef struct transacao_t{
    uint64_t id;
    int64_t valor;
    char tipo;
    char descricao[10];
} Transacao;

ParserError request_parser(const char* buffer, RequestData reqdata[1]);

ParserError parse_transacoes(const char* buffer, const RequestData* reqdata, Transacao transacao[1]);
#endif
