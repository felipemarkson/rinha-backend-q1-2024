#include <string.h>
#include <regex.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "request.h"
#include "log.h"

#include "../picohttpparser/picohttpparser.h"
#include "../cJSON/cJSON.h"
#include "server.h"

static const char *res[] = {
    "\\/clientes\\/[0-9]+\\/transacoes",
    "\\/clientes\\/[0-9]+\\/extrato",
    "\\/exit",
    "\\/ping"
};

static int map_method(const char *method, Method *out) {
    if (strstr(method, "GET") == method) {
        *out = GET;
        return 0;
    } else if (strstr(method, "POST") == method) {
        *out = POST;
        return 0;
    }

    return -1;
}

ParserError request_parser(const char*buff, RequestData reqdata[1]) {
    const char *method_str = NULL;
    const char *uri = NULL;
    int minor_version;
    struct phr_header headers[100];
    size_t num_headers = SIZE_ARRAY(headers);
    size_t method_len, uri_len;
    int nparsed = phr_parse_request(buff, MAX_REQ_RESP_SIZE, &method_str, &method_len, &uri,
                                    &uri_len, &minor_version, headers, &num_headers, 0);
    if (nparsed < 0)
        return PARSER_INVALID;

    // Method validation
    Method method = GET;
    if (map_method(method_str, &method) < 0) {
        return PARSER_INVALID;
    }

    // URI validation
    int matched_index = -1;
    regmatch_t pmatch[1] = {0};
    for (size_t i = 0; i < SIZE_ARRAY(res); i++) {
        regex_t regex;
        if (regcomp(&regex, res[i], REG_EXTENDED | REG_NEWLINE | REG_ICASE)) {
            regfree(&regex);
            return PARSER_MEMORY;
        }
        if (regexec(&regex, uri, 1, pmatch, 0) == 0) {
            regfree(&regex);
            if (matched_index > 0)  // More than one match.
                return PARSER_INVALID;
            matched_index = i;
            continue;
        }
        regfree(&regex);
    }
    if (matched_index < 0)  // Not found
        return PARSER_INVALID;
    if (matched_index == URI_EXIT || matched_index == URI_PING) {
        reqdata->body_loc = nparsed; 
        reqdata->method = method;
        reqdata->uri = matched_index;
        reqdata->id = UINT64_MAX;
        return PARSER_OK;
    }

    // Getting ID
    if (sscanf(uri, "/clientes/%lu", &reqdata->id) != 1) {
        // We pass the validation, we should be able to get the id.
        return PARSER_INVALID;
    }

    reqdata->body_loc = nparsed; 
    reqdata->method = method;
    reqdata->uri = matched_index;
    return PARSER_OK;
}

ParserError parse_transacoes(const char* buffer, const RequestData* reqdata, Transacao transacao[1]) {
    cJSON *transacao_json = cJSON_Parse(buffer + reqdata->body_loc);
    if (transacao_json == NULL){
        // cJSON do not report memory allocation fails. 
        // Thus, we will consider all errors as a client error.
        return PARSER_INVALID;   
    }

    const cJSON *valor = cJSON_GetObjectItemCaseSensitive(transacao_json, "valor");
    double intpart;
    if (!cJSON_IsNumber(valor)                             ||
        valor->valuedouble < 0.0                  ||
        modf(valor->valuedouble, &intpart) != 0.0 ||
        cJSON_GetNumberValue(valor) > (double)__INT64_MAX__)
    {
        cJSON_Delete(transacao_json);
        return PARSER_INVALID;
    }
    transacao->valor = (int64_t)cJSON_GetNumberValue(valor);

    const cJSON *tipo = cJSON_GetObjectItemCaseSensitive(transacao_json, "tipo");
    if (!cJSON_IsString(tipo)           ||
        tipo->valuestring == NULL       ||
        strlen(tipo->valuestring) != 1  ||
        (tipo->valuestring[0] != 'c' && tipo->valuestring[0] != 'd'))
    {
        cJSON_Delete(transacao_json);
        return PARSER_INVALID;
    }
    transacao->tipo = tipo->valuestring[0];

    const cJSON *descricao = cJSON_GetObjectItemCaseSensitive(transacao_json, "descricao");
    if (!cJSON_IsString(descricao)          ||
        descricao->valuestring == NULL      ||
        strlen(descricao->valuestring) > 10 ||
        strlen(descricao->valuestring) < 1 )
    {
        cJSON_Delete(transacao_json);
        return PARSER_INVALID;
    }
    memcpy(transacao->descricao, descricao->valuestring, strlen(descricao->valuestring));
    cJSON_Delete(transacao_json);
    transacao->id = reqdata->id;
    LOG("Transação (%lu) {valor: %ld, tipo: %c, descricao: \"%.10s\"}\n",
        transacao->id, transacao->valor, transacao->tipo, transacao->descricao);
    return PARSER_OK;
}
