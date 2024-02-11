curl -v \
     --data '{"valor": 1000, "tipo": "c", "descricao": "descricao"}' \
     localhost:9999/clientes/1/transacoes

curl -v --data '{"valor": 1.2, "tipo": "d", "descricao": "devolve"}' localhost:9999/clientes/1/transacoes

curl -v localhost:9999/clientes/1/extrato
curl -v --data '{"valor": 1, "tipo": "d", "descricao": "devolve"}' localhost:9999/clientes/1/transacoes

curl -v localhost:9999/exit