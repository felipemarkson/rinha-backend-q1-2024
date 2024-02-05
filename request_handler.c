#include <bsd/string.h>
#include <regex.h>
#include <stdio.h>

#include "picohttpparser/picohttpparser.h"
#include "server.h"

static const char *res[] = {"\\/clientes\\/[0-9]+\\/transacoes",
                            "\\/clientes\\/[0-9]+\\/extrato"};

const char badrequest[] =
    "HTTP/1.1 400 Bad Request\r\n"
    "Connection: close\r\n"
    "Content-type: text/plain\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

const char internalerror[] =
    "HTTP/1.1 500 Internal Error\r\n"
    "Connection: close\r\n"
    "Content-type: text/plain\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

const char OK_transacoes[] =
    "HTTP/1.1 200 OK\r\n"
    "Connection: close\r\n"
    "Content-type: text/plain\r\n"
    "Content-Length: 12\r\n"
    "\r\n"
    "transacoes\r\n";

const char OK_extrato[] =
    "HTTP/1.1 200 OK\r\n"
    "Connection: close\r\n"
    "Content-type: text/plain\r\n"
    "Content-Length: 9\r\n"
    "\r\n"
    "extrato\r\n";

typedef enum method_t { GET, POST } Method;

int map_method(const char *method, Method *out) {
    if (strnstr(method, "GET", 3) != NULL) {
        out = GET;
        return 0;
    } else if (strnstr(method, "POST", 4) != NULL) {
        *out = POST;
        return 0;
    }

    return -1;
}

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
        memcpy(req->buffer, badrequest, SIZE_ARRAY(badrequest));
        return;
    }

    // Method validation
    Method method = GET;
    if (map_method(method_str, &method) < 0) {
        memcpy(req->buffer, badrequest, SIZE_ARRAY(badrequest));
        return;
    }

    // URI validation
    int matched_index = -1;
    regmatch_t pmatch[1] = {0};
    for (size_t i = 0; i < SIZE_ARRAY(res); i++) {
        regex_t regex;
        if (regcomp(&regex, res[i], REG_EXTENDED | REG_NEWLINE | REG_ICASE)) {
            memcpy(req->buffer, internalerror, SIZE_ARRAY(internalerror));
            return;
        }
        if (regexec(&regex, uri, 1, pmatch, 0) == 0) {
            regfree(&regex);
            if (matched_index > 0) {  // More than one match.
                memcpy(req->buffer, badrequest, SIZE_ARRAY(badrequest));
                return;
            }
            matched_index = i;
        }
        regfree(&regex);
    }
    if (matched_index < 0) {  // Not found
        memcpy(req->buffer, badrequest, SIZE_ARRAY(badrequest));
        return;
    }


    // Getting ID
    unsigned long int id;
    if (sscanf(uri, "/clientes/%lu", &id) != 1) {
        // We pass the validation, we should be able to get the id.
        memcpy(req->buffer, internalerror, SIZE_ARRAY(internalerror));
        return;
    }

    if (matched_index == 0) {
        memcpy(req->buffer, OK_transacoes, SIZE_ARRAY(OK_transacoes));
    } else {
        memcpy(req->buffer, OK_extrato, SIZE_ARRAY(OK_extrato));
    }
}