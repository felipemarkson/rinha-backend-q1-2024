#ifndef DB_H
#define DB_H
#include "transacao.h"
#include <stddef.h>

typedef void* dbconn_t;

dbconn_t db_connect(void);
void db_disconnect(dbconn_t dbconn);
int db_socket(dbconn_t dbconn);

typedef enum transacao_error_t{
    TRANSACAO_INVALID = -1,
    TRANSACAO_DB_ERROR = -2,
}TransacaoError;

// Returns TransacaoError on error, number of bytes writed in buffer on success.
int db_push_transacao(dbconn_t dbconn, const Transacao transacao[1], size_t buffer_size, char buffer[static buffer_size]);

// Returns -1 on error, number of bytes writed in buffer on success.
int db_get_extrato(dbconn_t dbconn, uint64_t id, size_t buffer_size, char buffer[static buffer_size]);

// ASYNC FUNCTIONS BELLOW
int db_start_extrato(dbconn_t dbconn, uint64_t id);
int db_end_extrato(dbconn_t dbconn, size_t buffer_size, char buffer[static buffer_size]);

int db_start_transacao(dbconn_t dbconn, const Transacao transacao[1]);
int db_end_transacao(dbconn_t dbconn, size_t buffer_size, char buffer[static buffer_size]);

#endif
