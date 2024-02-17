#!/bin/sh
# Copyright (c) 2024 Felipe Markson dos Santos Monteiro
# Distributed under MIT No Attribution License (MIT-0)
# See the LICENSE at the end of this file or copy in at
# https://spdx.org/licenses/MIT-0.html

set -e
NUMBER_REQUEST_PARALLEL=300

BODY_DEBITO='{"valor":1, "tipo":"d", "descricao": "1234567890"}'
BODY_CREDITO='{"valor":1, "tipo":"c", "descricao": "1234567890"}'

for i in 1 2 3 4 5; do
    URL="http://localhost:9999/clientes/$i/transacoes"
    echo "$(date '+%H:%M:%S') Despachando $NUMBER_REQUEST_PARALLEL transações no cliente $i"&

    seq 1 $NUMBER_REQUEST_PARALLEL | \
        xargs -I $ -n1 -P $NUMBER_REQUEST_PARALLEL \
            curl -s -d \'$BODY_DEBITO\' $URL \
            &> /dev/null &

    seq 1 $NUMBER_REQUEST_PARALLEL | \
        xargs -I $ -n1 -P $NUMBER_REQUEST_PARALLEL \
            curl -s -d \'$BODY_CREDITO\' $URL \
            &> /dev/null &
done

wait
for i in 1 2 3 4 5; do
    result=$(curl -s http://localhost:9999/clientes/$i/extrato | jq ".saldo.total")
    echo "$(date '+%H:%M:%S') Client $i: saldo total = $result"
done
echo "Finishing!"
