#include "db.h"
#include <string.h>
#include <time.h>

#ifdef DOCKER
#include <postgresql/libpq-fe.h>
#else
#include <libpq-fe.h>
#endif

#define MIN(X,Y) ((X) < (Y) ? (X): (Y))


static const char *conn_kws[]  = {"host",      "port",  "dbname",  "user",      "password", NULL};
static const char *conn_vals[] = {"localhost", "6432",  "user_db", "user_user", "user_pwd", NULL};

dbconn_t db_connect(void){
    PGconn* db_conn = PQconnectdbParams(conn_kws, conn_vals, 0);
    if (db_conn == NULL){
        LOG("%s", "DB memory error");
        return NULL;
    }
    if (PQstatus(db_conn) != CONNECTION_OK){
        PQfinish(db_conn);
        LOG("%s: %d", "DB bad connection", PQstatus(db_conn));
        return NULL;
    }
    if(PQsetnonblocking(db_conn, 1) == -1){
        PQfinish(db_conn);
        LOG("%s: %d", "DB cannot async", PQstatus(db_conn));
        return NULL;
    }
    return (dbconn_t)db_conn;
}

void db_disconnect(dbconn_t db_conn){
    PQfinish(db_conn);
}

static int db_get_result_transacao(dbconn_t dbconn, PGresult *res, size_t buffer_size, char buffer[static buffer_size]){
   ExecStatusType result = PQresultStatus(res);
    if (result != PGRES_TUPLES_OK) {
        LOG("SELECT failed: %s\n", PQerrorMessage(dbconn));
        return TRANSACAO_DB_ERROR;
    }

    // If DB function retuns NULL, it is a fail!
    if (PQgetisnull(res, 0, 0)) {
        LOG("%s\n","INVALID TRANSACTION!!!");
        return TRANSACAO_INVALID;
    }

    // Validations
    if (PQntuples(res) != 1){
        LOG("%s %d\n", "Expectation fail! Number of rows: ", PQntuples(res));
        return TRANSACAO_DB_ERROR;
    }
    if(PQnfields(res) != 1){
        LOG("%s %d\n", "Expectation fail! Number of columns: ", PQnfields(res));
        return TRANSACAO_DB_ERROR;
    }
    char* db_ret =  PQgetvalue(res, 0, 0);
    LOG("%s\n", db_ret);
    int nwrite = (int)MIN(strlen(db_ret), buffer_size);
    memcpy(buffer, db_ret, nwrite);
    return nwrite;
}

int db_push_transacao(dbconn_t dbconn, const Transacao transacao[1], size_t buffer_size, char buffer[static buffer_size]){
    // Here, we reserve enough memory for build the command. Doing by PQexecParams is 
    // terrible.
    //                        __UINT64_MAX__         __INT64_MAX__
    // SELECT push_credito(18446744073709551615::int, 9223372036854775807::int, $1::varchar(10))
    char command_buffer[100] = {0};
    if (transacao->tipo == 'c')
        snprintf(command_buffer, 100, "SELECT push_credito(%lu::int, %ld::int, $1::varchar(10))", transacao->id, transacao->valor);
    else
        snprintf(command_buffer, 100, "SELECT push_debito(%lu::int, %ld::int, $1::varchar(10))", transacao->id, transacao->valor);
    LOG("Calling: %s\n", command_buffer);


    const char *paramVals[] = {transacao->descricao};
    PGresult *res = PQexecParams(dbconn, command_buffer, 1, NULL, paramVals, NULL, NULL, 0);
    int nwrite = db_get_result_transacao(dbconn, res, buffer_size, buffer);
    PQclear(res);
    return nwrite; 
}

static int db_get_result_extrato(dbconn_t dbconn, PGresult *res, size_t buffer_size, char buffer[static buffer_size]){
    ExecStatusType result = PQresultStatus(res);
    if (result != PGRES_TUPLES_OK) {
        LOG("SELECT failed: %s\n", PQerrorMessage(dbconn));
        return -1;
    }

    // If DB function retuns NULL, it is a fail! But here we should not FAIL.
    if (PQgetisnull(res, 0, 0)) {
        LOG("%s\n","INTERNAL ERROR!");
        return -1;
    }

    // Validations
    if (PQntuples(res) != 1){
        LOG("%s %d\n", "Expectation fail! Number of rows: ", PQntuples(res));
        return -1;
    }
    if(PQnfields(res) != 1){
        LOG("%s %d\n", "Expectation fail! Number of columns: ", PQnfields(res));
        return -1;
    }
    char* db_ret =  PQgetvalue(res, 0, 0);
    LOG("%s\n", db_ret);
    int nwrite = (int)MIN(strlen(db_ret), buffer_size);
    memcpy(buffer, db_ret, nwrite);
    return nwrite;
}

int db_get_extrato(dbconn_t dbconn, uint64_t id, size_t buffer_size, char buffer[static buffer_size]){
    // Here, we reserve enough memory for build the command. Doing by PQexecParams is 
    // terrible.
    //                        __UINT64_MAX__
    // SELECT get_extrato(18446744073709551615::int)
    char command_buffer[50] = {0};
    snprintf(command_buffer, 50, "SELECT get_extrato(%lu::int)", id);

    PGresult *res = PQexec(dbconn, command_buffer);
    int nwrite = db_get_result_extrato(dbconn, res, buffer_size, buffer);
    PQclear(res);
    return nwrite;
}

// ASYN FUNCTIONS
static int wait_db(dbconn_t dbconn){
    do{
        int ret = PQconsumeInput(dbconn);
        if (ret != 1){
            LOG("Consume result fail!: %s\n", PQerrorMessage(dbconn));
            return -1;
        }
        nanosleep(&(struct timespec){.tv_nsec = 5}, NULL); // some time to load the result;
    } while(PQisBusy(dbconn));
    return 0;
}

int db_start_extrato(dbconn_t dbconn, uint64_t id){
    // Here, we reserve enough memory for build the command. Doing by PQexecParams is 
    // terrible.
    //                        __UINT64_MAX__
    // SELECT get_extrato(18446744073709551615::int)
    char command_buffer[50] = {0};
    snprintf(command_buffer, 50, "SELECT get_extrato(%lu::int)", id);

    int ret = PQsendQuery(dbconn, command_buffer);
    if (ret < 0) {
        LOG("SELECT failed: %s\n", PQerrorMessage(dbconn));
        return -1;
    }
    return 0;
}



int db_end_extrato(dbconn_t dbconn, size_t buffer_size, char buffer[static buffer_size]){
    if (wait_db(dbconn) < 0)
        return -1;

    PGresult *res = PQgetResult(dbconn);
    int nwrite = db_get_result_extrato(dbconn, res, buffer_size, buffer);
    PQclear(res);
    return nwrite;
}


int db_start_transacao(dbconn_t dbconn, const Transacao transacao[1]) {
    // Here, we reserve enough memory for build the command. Doing by PQexecParams is 
    // terrible.
    //                        __UINT64_MAX__         __INT64_MAX__
    // SELECT push_credito(18446744073709551615::int, 9223372036854775807::int, $1::varchar(10))
    char command_buffer[100] = {0};
    if (transacao->tipo == 'c')
        snprintf(command_buffer, 100, "SELECT push_credito(%lu::int, %ld::int, $1::varchar(10))", transacao->id, transacao->valor);
    else
        snprintf(command_buffer, 100, "SELECT push_debito(%lu::int, %ld::int, $1::varchar(10))", transacao->id, transacao->valor);
    LOG("Calling: %s\n", command_buffer);
    const char *paramVals[] = {transacao->descricao};
    int ret = PQsendQueryParams(dbconn, command_buffer, 1, NULL, paramVals, NULL, NULL, 0);
    if (ret < 0) {
        LOG("SELECT failed: %s\n", PQerrorMessage(dbconn));
        return -1;
    }
    return 0;
}

int db_end_transacao(dbconn_t dbconn, size_t buffer_size, char buffer[static buffer_size]){
    if (wait_db(dbconn) < 0)
        return -1;
    PGresult *res = PQgetResult(dbconn);
    int nwrite = db_get_result_transacao(dbconn, res, buffer_size, buffer);
    PQclear(res);
    return nwrite;
}