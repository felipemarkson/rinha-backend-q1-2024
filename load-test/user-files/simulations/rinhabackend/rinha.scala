import scala.concurrent.duration._

import scala.util.Random

import util.Try

import io.gatling.commons.validation._
import io.gatling.core.session.Session
import io.gatling.core.Predef._
import io.gatling.http.Predef._


class RinhaBackendCrebitosSimulation
  extends Simulation {

  def randomClienteId() = Random.between(1, 5 + 1)
  def randomValorTransacao() = Random.between(1, 10000 + 1)
  def randomDescricao() = Random.alphanumeric.take(10).mkString
  def randomTipoTransacao() = Seq("c", "d", "d")(Random.between(0, 2 + 1)) // not used
  def toInt(s: String): Option[Int] = {
    try {
      Some(s.toInt)
    } catch {
      case e: Exception => None
    }
  }

  val validarConsistenciaSaldoLimite = (valor: Option[String], session: Session) => {
    /*
      Essa função é frágil porque depende que haja uma entrada
      chamada 'limite' com valor conversível para int na session
      e também que seja encadeada com com jmesPath("saldo") para
      que 'valor' seja o primeiro argumento da função validadora
      de 'validate(.., ..)'.
      
      =============================================================
      
      Nota para quem não tem experiência em testes de performance:
        O teste de lógica de saldo/limite extrapola o que é comumente 
        feito em testes de performance apenas por causa da natureza
        da Rinha de Backend. Evite fazer esse tipo de coisa em 
        testes de performance, pois não é uma prática recomendada
        normalmente.
    */ 

    val saldo = valor.flatMap(s => Try(s.toInt).toOption)
    val limite = toInt(session("limite").as[String])

    (saldo, limite) match {
      case (Some(s), Some(l)) if s.toInt < l.toInt * -1 => Failure("Limite ultrapassado!")
      case (Some(s), Some(l)) if s.toInt >= l.toInt * -1 => Success(Option("ok"))
      case _ => Failure("WTF?!")
    }
  }

  val httpProtocol = http
    .baseUrl("http://localhost:9999")
    .userAgentHeader("Agente do Caos - 2024/Q1")
    .shareConnections

  val debitos = scenario("débitos")
    .exec {s =>
      val descricao = randomDescricao()
      val cliente_id = randomClienteId()
      val valor = randomValorTransacao()
      val payload = s"""{"valor": ${valor}, "tipo": "d", "descricao": "${descricao}"}"""
      val session = s.setAll(Map("descricao" -> descricao, "cliente_id" -> cliente_id, "payload" -> payload))
      session
    }
    .exec(
      http("débitos")
      .post(s => s"/clientes/${s("cliente_id").as[String]}/transacoes")
          .header("content-type", "application/json")
          .body(StringBody(s => s("payload").as[String]))
          .check(
            status.in(200, 422),
            status.saveAs("httpStatus"))
          .checkIf(s => s("httpStatus").as[String] == "200") { jmesPath("limite").saveAs("limite") }
          .checkIf(s => s("httpStatus").as[String] == "200") {
            jmesPath("saldo").validate("ConsistenciaSaldoLimite - Transação", validarConsistenciaSaldoLimite)
          }
    )

  val creditos = scenario("créditos")
    .exec {s =>
      val descricao = randomDescricao()
      val cliente_id = randomClienteId()
      val valor = randomValorTransacao()
      val payload = s"""{"valor": ${valor}, "tipo": "c", "descricao": "${descricao}"}"""
      val session = s.setAll(Map("descricao" -> descricao, "cliente_id" -> cliente_id, "payload" -> payload))
      session
    }
    .exec(
      http("créditos")
      .post(s => s"/clientes/${s("cliente_id").as[String]}/transacoes")
          .header("content-type", "application/json")
          .body(StringBody(s => s("payload").as[String]))
          .check(
            status.in(200),
            jmesPath("limite").saveAs("limite"),
            jmesPath("saldo").validate("ConsistenciaSaldoLimite - Transação", validarConsistenciaSaldoLimite)
          )
    )

  val extratos = scenario("extratos")
    .exec(
      http("extratos")
      .get(s => s"/clientes/${randomClienteId()}/extrato")
      .check(
        jmesPath("saldo.limite").saveAs("limite"),
        jmesPath("saldo.total").validate("ConsistenciaSaldoLimite - Extrato", validarConsistenciaSaldoLimite)
    )
  )

  val validacaDebitosConcorrentesNumRequests = 25
  val validacaDebitosConcorrentesTransacaoes = scenario("validação concorrência transações")
    .exec(
      http("validações concorrência")
      .post("/clientes/4/transacoes")
          .header("content-type", "application/json")
          .body(StringBody("""{"valor": 1, "tipo": "d", "descricao": "validacao"}"""))
          .check(status.is(200))
    )
  
  val validacaDebitosConcorrentesExtrato = scenario("validação concorrência extrato")
    .exec(
      http("validações concorrência")
      .get("/clientes/4/extrato")
      .check(
        jmesPath("saldo.total").ofType[Int].is(validacaDebitosConcorrentesNumRequests * -1)
    )
  )

  val saldosIniciaisClientes = Array(
    Map("id" -> 1, "limite" ->   1000 * 100),
    Map("id" -> 2, "limite" ->    800 * 100),
    Map("id" -> 3, "limite" ->  10000 * 100),
    Map("id" -> 4, "limite" -> 100000 * 100),
    Map("id" -> 5, "limite" ->   5000 * 100),
  )

  val criteriosClientes = scenario("validações")
    .feed(saldosIniciaisClientes)
    .exec(
      /*
        Os valores de http(...) essão duplicados propositalmente
        para que sejam agrupados no relatório e ocupem menos espaço.
        O lado negativo é que, em caso de falha, pode não ser possível
        saber sua causa exata.
      */ 
      http("test_basic_extrato")
      .get("/clientes/#{id}/extrato")
      .check(
        status.is(200),
        jmesPath("saldo.limite").ofType[String].is("#{limite}"),
        jmesPath("saldo.total").ofType[String].is("0")
      )
    )
    .exec(
      http("test_notfound_extrato")
      .get("/clientes/6/extrato")
      .check(status.is(404))
    )
    .exec(
      http("test_basic_transacao_credit")
      .post("/clientes/1/transacoes")
          .header("content-type", "application/json")
          .body(StringBody(s"""{"valor": 1, "tipo": "c", "descricao": "toma"}"""))
          .check(
            status.in(200),
            jmesPath("limite").saveAs("limite"),
            jmesPath("saldo").validate("ConsistenciaSaldoLimite - Transação", validarConsistenciaSaldoLimite)
          )
    )
    .exec(
      http("test_basic_transacao_debit")
      .post("/clientes/1/transacoes")
          .header("content-type", "application/json")
          .body(StringBody(s"""{"valor": 1, "tipo": "d", "descricao": "devolve"}"""))
          .check(
            status.in(200),
            jmesPath("limite").saveAs("limite"),
            jmesPath("saldo").validate("ConsistenciaSaldoLimite - Transação", validarConsistenciaSaldoLimite)
          )
    )
    .exec(
      http("test_basic_extrato_after_transacao")
      .get("/clientes/1/extrato")
      .check(
        jmesPath("ultimas_transacoes[0].descricao").ofType[String].is("devolve"),
        jmesPath("ultimas_transacoes[0].tipo").ofType[String].is("d"),
        jmesPath("ultimas_transacoes[0].valor").ofType[Int].is("1"),
        jmesPath("ultimas_transacoes[1].descricao").ofType[String].is("toma"),
        jmesPath("ultimas_transacoes[1].tipo").ofType[String].is("c"),
        jmesPath("ultimas_transacoes[1].valor").ofType[Int].is("1")
      )
    )
    .exec(
      http("test_invalid_transacao_float")
      .post("/clientes/1/transacoes")
          .header("content-type", "application/json")
          .body(StringBody(s"""{"valor": 1.2, "tipo": "d", "descricao": "devolve"}"""))
          .check(status.in(422))
    )
    .exec(
      http("test_invalid_transacao_tipo")
      .post("/clientes/1/transacoes")
          .header("content-type", "application/json")
          .body(StringBody(s"""{"valor": 1, "tipo": "x", "descricao": "devolve"}"""))
          .check(status.in(422))
    )
    .exec(
      http("test_invalid_transacao_descricao_max_len")
      .post("/clientes/1/transacoes")
          .header("content-type", "application/json")
          .body(StringBody(s"""{"valor": 1, "tipo": "c", "descricao": "123456789 e mais um pouco"}"""))
          .check(status.in(422))
    )
    .exec(
      http("test_invalid_transacao_descricao_min_len")
      .post("/clientes/1/transacoes")
          .header("content-type", "application/json")
          .body(StringBody(s"""{"valor": 1, "tipo": "c", "descricao": ""}"""))
          .check(status.in(422))
    )
    .exec(
      http("test_invalid_transacao_descricao_null")
      .post("/clientes/1/transacoes")
          .header("content-type", "application/json")
          .body(StringBody(s"""{"valor": 1, "tipo": "c", "descricao": null}"""))
          .check(status.in(422))
    )

  /* 
    Separar créditos e débitos dá uma visão
    melhor sobre como as duas operações se
    comportam individualmente.
  */
  setUp(
    validacaDebitosConcorrentesTransacaoes.inject(
      atOnceUsers(validacaDebitosConcorrentesNumRequests)
    ).andThen(
      validacaDebitosConcorrentesExtrato.inject(
        atOnceUsers(1)
      )
    ).andThen(
    criteriosClientes.inject(
      atOnceUsers(1)
    )
    .andThen(
      debitos.inject(
        rampUsersPerSec(1).to(220).during(2.minutes),
        constantUsersPerSec(220).during(2.minutes)
      ),
      creditos.inject(
        rampUsersPerSec(1).to(110).during(2.minutes),
        constantUsersPerSec(110).during(2.minutes)
      ),
      extratos.inject(
        rampUsersPerSec(1).to(10).during(2.minutes),
        constantUsersPerSec(10).during(2.minutes)
        )
      )
    )
  ).protocols(httpProtocol)
}
