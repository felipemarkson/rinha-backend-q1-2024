set -ex

sudo docker compose down --remove-orphans
sudo docker compose -f docker-compose-db-only.yml up -d
pgbouncer pgbouncer.ini -d
valgrind --leak-check=full --show-leak-kinds=all --log-file=valgrind.log ./maind &
PID_BIN=$!
echo "Waiting DB"
sleep 10 # wait pg
for j in $(seq 5); do
    curl -s "http://localhost:9999/clientes/$j/extrato" &
    curl -s --data '{"valor": 1, "tipo": "c", "descricao": "1234567890"}' localhost:9999/clientes/$j/transacoes &
    curl -s --data '{"valor": 2, "tipo": "d", "descricao": "1234567890"}' localhost:9999/clientes/$j/transacoes &
    curl "http://localhost:9999/clientes/$j/extrato"
done
echo "Waiting Exiting"
sleep 2
curl "http://localhost:9999/exit"
pkill pgbouncer
sudo docker compose down --remove-orphans
