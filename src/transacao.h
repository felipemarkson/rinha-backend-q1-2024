#ifndef TRANSACAO_H
#define TRANSACAO_H
#include <stdint.h>

typedef struct transacao_t{
    uint64_t id;
    int64_t valor;
    char tipo;
    char descricao[10];
} Transacao;

#endif