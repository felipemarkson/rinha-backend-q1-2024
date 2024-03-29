rm -rf docker.log
rm -rf pgbouncer.log
pkill pgbouncer
set -x

docker compose down --remove-orphans
docker compose -f docker-compose-db-only.yml down --remove-orphans
docker compose -f docker-compose-db-only.yml up &> docker.log &
echo "Waiting DB"
sleep 10 # wait pg
valgrind --leak-check=full --show-leak-kinds=all --log-file=valgrind.log ./maind &
PID_BIN=$!
echo "Waiting valgrind"

while true; do
    curl --fail http://localhost:9999/clientes/1/extrato &> /dev/null
    if [ $? -ne 0 ]; then
        sleep 2
    else
        break
    fi
done

# wait $PID_BIN # to run tests

for j in $(seq 5); do
    curl -s "http://localhost:9999/clientes/$j/extrato" &
    curl -s --data '{"valor": 1, "tipo": "c", "descricao": "1234567890"}' localhost:9999/clientes/$j/transacoes &
    curl -s --data '{"valor": 2, "tipo": "d", "descricao": "1234567890"}' localhost:9999/clientes/$j/transacoes &
    curl "http://localhost:9999/clientes/$j/extrato" &
done
echo "Waiting Exiting"
sleep 2
curl "http://localhost:9999/exit"
docker compose down --remove-orphans
