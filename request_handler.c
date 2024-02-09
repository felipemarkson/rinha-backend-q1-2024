#include <string.h>
#include <regex.h>
#include <stdio.h>
#include <stdint.h>

#ifdef DOCKER
#include <postgresql/libpq-fe.h>
#else
#include <libpq-fe.h>
#endif

#include "picohttpparser/picohttpparser.h"
#include "cJSON/cJSON.h"
#include "server.h"

#define SET_STATIC_RESPONSE(buffer, response) memcpy((buffer), response, SIZE_ARRAY(response))

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)

#define BADREQUEST                 \
    "HTTP/1.1 400 Bad Request\r\n" \
    "File: " __FILE__    "\r\n"    \
    "Line: " LINE_STRING "\r\n"    \
    "Connection: close\r\n"        \
    "Content-type: text/plain\r\n" \
    "Content-Length: 0\r\n"        \
    "\r\n"

#define INTERNALERROR                 \
    "HTTP/1.1 500 Internal Error\r\n" \
    "File: " __FILE__     "\r\n"      \
    "Line: " LINE_STRING  "\r\n"      \
    "Connection: close\r\n"           \
    "Content-type: text/plain\r\n"    \
    "Content-Length: 0\r\n"           \
    "\r\n"                            \

#define NOTFOUND                      \
    "HTTP/1.1 404 Not found\r\n"      \
    "Connection: close\r\n"           \
    "Content-type: text/plain\r\n"    \
    "Content-Length: 0\r\n"           \
    "\r\n"                            \

#define UNPROCESSABLE                       \
    "HTTP/1.1 422 Unprocessable Entity\r\n" \
    "Connection: close\r\n"                 \
    "Content-type: text/plain\r\n"          \
    "Content-Length: 0\r\n"                 \
    "\r\n"                                  \


#define OK_TRANSACAO                  \
    "HTTP/1.1 200 OK\r\n"             \
    "Connection: close\r\n"           \
    "Content-type: text/plain\r\n"    \
    "Content-Length: %ld\r\n"         \
    "\r\n"                            \
    "%s\n"                            \

#define OK_EXTRATO                    \
    "HTTP/1.1 200 OK\r\n"             \
    "Connection: close\r\n"           \
    "Content-type: text/plain\r\n"    \
    "Content-Length: %ld\r\n"         \
    "\r\n"                            \
    "%s\n"                            \


static const char *res[] = {
    "\\/clientes\\/[0-9]+\\/transacoes",
    "\\/clientes\\/[0-9]+\\/extrato"
};

#ifdef DOCKER
    #define DB_HOST "postgres"
#else
    #define DB_HOST "localhost"
#endif


static const char *conn_kws[]  = {"host",  "dbname",  "user",      "password", NULL};
static const char *conn_vals[] = {DB_HOST, "user_db", "user_user", "user_pwd", NULL};

typedef enum method_t { GET, POST } Method;

static int map_method(const char *method, Method *out) {
    if (strstr(method, "GET") == method) {
        out = GET;
        return 0;
    } else if (strstr(method, "POST") == method) {
        *out = POST;
        return 0;
    }

    return -1;
}

static void transacoes(uint64_t id, char* buffer, int body_loc);
static void extrato(uint64_t id, char* buffer);

void request_handler(ReqRes *req) {
    const char *method_str = NULL;
    const char *buff = req->buffer;
    const char *uri = NULL;
    int minor_version;
    struct phr_header headers[100];
    size_t num_headers = SIZE_ARRAY(headers);
    size_t method_len, uri_len;
    int nparsed = phr_parse_request(buff, MAX_CONN, &method_str, &method_len, &uri,
                                    &uri_len, &minor_version, headers, &num_headers, 0);
    if (nparsed < 0) {
        SET_STATIC_RESPONSE(req->buffer, BADREQUEST);
        return;
    }

    // Method validation
    Method method = GET;
    if (map_method(method_str, &method) < 0) {
        SET_STATIC_RESPONSE(req->buffer, BADREQUEST);
        return;
    }

    // URI validation
    int matched_index = -1;
    regmatch_t pmatch[1] = {0};
    for (size_t i = 0; i < SIZE_ARRAY(res); i++) {
        regex_t regex;
        if (regcomp(&regex, res[i], REG_EXTENDED | REG_NEWLINE | REG_ICASE)) {
            SET_STATIC_RESPONSE(req->buffer, INTERNALERROR);
            regfree(&regex);
            return;
        }
        if (regexec(&regex, uri, 1, pmatch, 0) == 0) {
            regfree(&regex);
            if (matched_index > 0) {  // More than one match.
                SET_STATIC_RESPONSE(req->buffer, BADREQUEST);
                return;
            }
            matched_index = i;
            continue;
        }
        regfree(&regex);
    }
    if (matched_index < 0) {  // Not found
        SET_STATIC_RESPONSE(req->buffer, BADREQUEST);
        return;
    }


    // Getting ID
    uint64_t id;
    if (sscanf(uri, "/clientes/%lu", &id) != 1) {
        // We pass the validation, we should be able to get the id.
        SET_STATIC_RESPONSE(req->buffer, BADREQUEST);
        return;
    }

    // We know that any client greater than 6 exists. So, we hardcoded.
    if (id >= 6){
        SET_STATIC_RESPONSE(req->buffer, NOTFOUND);
        return;
    }

    if (matched_index == 0 && method == POST) {
        transacoes(id, req->buffer, nparsed);
    } else if (matched_index == 1 && method == GET) {
        extrato(id, req->buffer);
    } else {
        SET_STATIC_RESPONSE(req->buffer, BADREQUEST);
    }
}

typedef struct transacao_t{
    int64_t valor;
    char tipo;
    char descricao[10];
} Transacao;

static void transacoes(uint64_t id, char* buffer, int body_loc) {
    Transacao transacao = {0};
    cJSON *transacao_json = cJSON_Parse(buffer + body_loc);
    if (transacao_json == NULL){
        // cJSON do not report memory allocation fails. 
        // Thus, we will consider all errors as a client error.
        SET_STATIC_RESPONSE(buffer, BADREQUEST);
        return;   
    }

    const cJSON *valor = cJSON_GetObjectItemCaseSensitive(transacao_json, "valor");
    if (!cJSON_IsNumber(valor)              ||
        transacao_json->valuedouble < 0.0   ||
        cJSON_GetNumberValue(valor) > (double)__INT64_MAX__)
    {
        cJSON_Delete(transacao_json);
        SET_STATIC_RESPONSE(buffer, BADREQUEST);
        return; 
    }
    transacao.valor = (int64_t)cJSON_GetNumberValue(valor);

    const cJSON *tipo = cJSON_GetObjectItemCaseSensitive(transacao_json, "tipo");
    if (!cJSON_IsString(tipo)           ||
        tipo->valuestring == NULL       ||
        strlen(tipo->valuestring) != 1  ||
        (tipo->valuestring[0] != 'c' && tipo->valuestring[0] != 'd'))
    {
        cJSON_Delete(transacao_json);
        SET_STATIC_RESPONSE(buffer, BADREQUEST);
        return; 
    }
    transacao.tipo = tipo->valuestring[0];

    const cJSON *descricao = cJSON_GetObjectItemCaseSensitive(transacao_json, "descricao");
    if (!cJSON_IsString(descricao)      ||
        descricao->valuestring == NULL  ||
        strlen(descricao->valuestring) > 10 )
    {
        cJSON_Delete(transacao_json);
        SET_STATIC_RESPONSE(buffer, BADREQUEST);
        return; 
    }
    memcpy(transacao.descricao, descricao->valuestring, strlen(descricao->valuestring));
    LOG("Transação (%lu) {valor: %ld, tipo: %c, descricao: \"%.10s\"}\n",
            id, transacao.valor, transacao.tipo, transacao.descricao);

    PGconn *conn = PQconnectdbParams(conn_kws, conn_vals, 0);
    if (PQstatus(conn) != CONNECTION_OK){
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        LOG("Connection failed: %s", PQerrorMessage(conn));
        PQfinish(conn);
        return;
    }

    // Here, we reserve enough memory for build the command. Doing by PQexecParams is 
    // terrible.
    //                        __UINT64_MAX__         __INT64_MAX__
    // SELECT push_credito(18446744073709551615::int, 9223372036854775807::int, $1::varchar(10))
    char command_buffer[100] = {0};
    if (transacao.tipo == 'c')
        snprintf(command_buffer, 100, "SELECT push_credito(%lu::int, %ld::int, $1::varchar(10))", id, transacao.valor);
    else
        snprintf(command_buffer, 100, "SELECT push_debito(%lu::int, %ld::int, $1::varchar(10))", id, transacao.valor);
    LOG("Calling: %s\n", command_buffer);

    const char *paramVals[] = {transacao.descricao};
    PGresult * res = PQexecParams(conn, command_buffer, 1, NULL, paramVals, NULL, NULL, 0);
    ExecStatusType result = PQresultStatus(res);
    if (result != PGRES_TUPLES_OK) {
        LOG("SELECT failed: %s\n", PQerrorMessage(conn));
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        PQclear(res);
        PQfinish(conn);
        return;
    }

    // If DB function retuns NULL, it is a fail!
    if (PQgetisnull(res, 0, 0)) {
        LOG("%s\n","INVALID TRANSACTION!!!");
        SET_STATIC_RESPONSE(buffer, UNPROCESSABLE);
        PQclear(res);
        PQfinish(conn);
        return;
    }

    // Validations
    if (PQntuples(res) != 1){
        LOG("%s %d\n", "Expectation fail! Number of rows: ", PQntuples(res));
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        PQclear(res);
        PQfinish(conn);
        return;
    }
    if(PQnfields(res) != 1){
        LOG("%s %d\n", "Expectation fail! Number of columns: ", PQnfields(res));
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        PQclear(res);
        PQfinish(conn);
        return;
    }
    char* db_ret =  PQgetvalue(res, 0, 0);
    LOG("%s\n", db_ret);
    snprintf(buffer, MAX_REQ_RESP_SIZE, OK_TRANSACAO, strlen(db_ret) + 1, db_ret);
    PQclear(res);
    PQfinish(conn);
}


static void extrato(uint64_t id, char* buffer) {
    PGconn *conn = PQconnectdbParams(conn_kws, conn_vals, 0);
    if (PQstatus(conn) != CONNECTION_OK){
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        LOG("Connection failed: %s", PQerrorMessage(conn));
        PQfinish(conn);
        return;
    }
    // Here, we reserve enough memory for build the command. Doing by PQexecParams is 
    // terrible.
    //                        __UINT64_MAX__
    // SELECT get_extrato(18446744073709551615::int)
    char command_buffer[50] = {0};
    snprintf(command_buffer, 50, "SELECT get_extrato(%lu::int)", id);

    PGresult * res = PQexec(conn, command_buffer);
    ExecStatusType result = PQresultStatus(res);
    if (result != PGRES_TUPLES_OK) {
        LOG("SELECT failed: %s\n", PQerrorMessage(conn));
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        PQclear(res);
        PQfinish(conn);
        return;
    }

    // If DB function retuns NULL, it is a fail! But here we should not FAIL.
    if (PQgetisnull(res, 0, 0)) {
        LOG("%s\n","INTERNAL ERROR!");
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        PQclear(res);
        PQfinish(conn);
        return;
    }

    // Validations
    if (PQntuples(res) != 1){
        LOG("%s %d\n", "Expectation fail! Number of rows: ", PQntuples(res));
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        PQclear(res);
        PQfinish(conn);
        return;
    }
    if(PQnfields(res) != 1){
        LOG("%s %d\n", "Expectation fail! Number of columns: ", PQnfields(res));
        SET_STATIC_RESPONSE(buffer, INTERNALERROR);
        PQclear(res);
        PQfinish(conn);
        return;
    }
    char* db_ret =  PQgetvalue(res, 0, 0);
    LOG("%s\n", db_ret);
    snprintf(buffer, MAX_REQ_RESP_SIZE, OK_EXTRATO, strlen(db_ret) + 1, db_ret);
    PQclear(res);
    PQfinish(conn);
}