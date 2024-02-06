CREATE TABLE clientes (
    id SERIAL PRIMARY KEY,
    limite INTEGER NOT NULL
);

CREATE TABLE transacoes (
    id SERIAL PRIMARY KEY,
    cliente_id INTEGER NOT NULL REFERENCES clientes(id),
    valor INTEGER NOT NULL,
    tipo CHAR(1) NOT NULL,
    descricao VARCHAR(10) NOT NULL,
    realizada_em TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE saldos (
    id SERIAL PRIMARY KEY,
    cliente_id INTEGER NOT NULL REFERENCES clientes(id),
    valor INTEGER NOT NULL
);

DO $$
BEGIN
    INSERT INTO clientes (limite)
    VALUES
        (1000 * 100),
        (800 * 100),
        (10000 * 100),
        (100000 * 100),
        (5000 * 100);

    INSERT INTO saldos (cliente_id, valor)
        SELECT id, 0 FROM clientes;
END;
$$;


CREATE OR REPLACE PROCEDURE push_credito(
    cliente_id_in INTEGER,
    valor_in INTEGER,
    descricao_in VARCHAR(10)
)
LANGUAGE plpgsql
AS $$
BEGIN
    SELECT id FROM clientes WHERE id = cliente_id_in;
    IF NOT FOUND THEN
        RAISE 'INVALID CLIENT ID';
    END IF;

    UPDATE saldos SET valor = valor + valor_in WHERE cliente_id = cliente_id_in;
    INSERT INTO transacoes(cliente_id, valor, tipo, descricao) VALUES (cliente_id_in, valor_in, 'c' ,descricao_in);

    COMMIT;
END
$$;

CREATE OR REPLACE PROCEDURE push_debito(
    cliente_id_in INTEGER,
    valor_in INTEGER,
    descricao_in VARCHAR(10)
)
LANGUAGE plpgsql
AS $$
BEGIN
    UPDATE saldos SET valor = (valor - valor_in)
        WHERE cliente_id = cliente_id_in
            AND (valor - valor_in) > - (
                SELECT limite FROM clientes WHERE id = cliente_id_in
            );
    IF NOT FOUND THEN
        RAISE 'INVALID DEBIT';
    END IF;

    INSERT INTO transacoes(cliente_id, valor, tipo, descricao)
        VALUES (cliente_id_in, valor_in, 'd' ,descricao_in);

    COMMIT;
END
$$;
