#include <bsd/string.h>
#include <regex.h>
#include <stdio.h>
#include <stdint.h>

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

#define OK_TRANSACAO                  \
    "HTTP/1.1 200 OK\r\n"             \
    "Connection: close\r\n"           \
    "Content-type: text/plain\r\n"    \
    "Content-Length: 12\r\n"          \
    "\r\n"                            \
    "transacoes\r\n"                  \

#define OK_EXTRATO                    \
    "HTTP/1.1 200 OK\r\n"             \
    "Connection: close\r\n"           \
    "Content-type: text/plain\r\n"    \
    "Content-Length: 9\r\n"           \
    "\r\n"                            \
    "extrato\r\n"                     \


static const char *res[] = {
    "\\/clientes\\/[0-9]+\\/transacoes",
    "\\/clientes\\/[0-9]+\\/extrato"
};

typedef enum method_t { GET, POST } Method;

static int map_method(const char *method, Method *out) {
    if (strnstr(method, "GET", 3) != NULL) {
        out = GET;
        return 0;
    } else if (strnstr(method, "POST", 4) != NULL) {
        *out = POST;
        return 0;
    }

    return -1;
}

static void transacoes(uint64_t id, char* buffer, int body_loc);

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
            return;
        }
        if (regexec(&regex, uri, 1, pmatch, 0) == 0) {
            regfree(&regex);
            if (matched_index > 0) {  // More than one match.
                SET_STATIC_RESPONSE(req->buffer, BADREQUEST);
                return;
            }
            matched_index = i;
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

    if (matched_index == 0 && method == POST) {
        transacoes(id, req->buffer, nparsed);
    } else if (matched_index == 1 && method == GET) {
        SET_STATIC_RESPONSE(req->buffer, OK_EXTRATO);
    } else {
        SET_STATIC_RESPONSE(req->buffer, BADREQUEST);
    }
}

typedef struct transacao_t{
    int64_t valor;
    char tipo;
    char descricao[10];
} Transacao;

typedef struct saldo_t{
    int64_t limite;
    int64_t saldo;
} Saldo;

static void transacoes(uint64_t id, char* buffer, int buffer_loc) {
    Transacao transacao = {0};
    cJSON *transacao_json = cJSON_Parse(buffer + buffer_loc);
    if (transacao_json == NULL){
        // cJSON do not report memory allocation fails. 
        // Thus, we will consider all errors as a client error.
        SET_STATIC_RESPONSE(buffer, BADREQUEST);
        return;   
    }

    const cJSON *valor = cJSON_GetObjectItemCaseSensitive(transacao_json, "valor");
    if (!cJSON_IsNumber(valor)){
        cJSON_Delete(transacao_json);
        SET_STATIC_RESPONSE(buffer, BADREQUEST);
        return; 
    }
    transacao.valor =  cJSON_GetNumberValue(valor);
    
    const cJSON *tipo = cJSON_GetObjectItemCaseSensitive(transacao_json, "tipo");
    if (!cJSON_IsString(tipo)           ||
        tipo->valuestring == NULL       ||
        strlen(tipo->valuestring) != 1  ||
        (tipo->valuestring[0] != 'c' && tipo->valuestring[0] != 'v'))
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

    SET_STATIC_RESPONSE(buffer, OK_TRANSACAO);

}