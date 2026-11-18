#include <cassert>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "modelo/Historico.hpp"
#include "modelo/Telemetria.hpp"
#include "visual/GraficoTempo.hpp"
#include "visual/HorizonteArtificial.hpp"

namespace {

gcs::EstadoVeiculo criar_estado_exemplo(double altitude_m,
                                        double rolagem_deg,
                                        double arfagem_deg,
                                        double guinada_deg,
                                        double bateria_pct,
                                        int offset_ms) {
    gcs::EstadoVeiculo e{};
    e.instante = std::chrono::steady_clock::now() + std::chrono::milliseconds(offset_ms);
    e.modo_de_voo = "GUIDED";
    e.armado = true;
    e.em_voo = true;

    e.posicao.latitude = -30.0580;
    e.posicao.longitude = -51.1730;
    e.posicao.altitude_absoluta_m = 100.0 + altitude_m;
    e.posicao.altitude_relativa_m = altitude_m;

    e.atitude.rolagem = rolagem_deg;
    e.atitude.arfagem = arfagem_deg;
    e.atitude.guinada = guinada_deg;

    e.velocidade.velocidade_solo_ms = 5.2;
    e.velocidade.taxa_subida_ms = 1.1;

    e.bateria.tensao_v = 12.1;
    e.bateria.percentual = bateria_pct;

    e.gps.num_satelites = 12;
    e.gps.tipo_fixo = 3;
    return e;
}

void testar_buffer_circular_historico() {
    gcs::Historico historico(3);
    assert(historico.vazio());

    bool recebeu = true;
    historico.ultimo(&recebeu);
    assert(!recebeu);

    for (int i = 1; i <= 5; ++i) {
        historico.registrar(
            criar_estado_exemplo(i * 10.0, 0.0, 0.0, 90.0, 100.0 - i * 5.0, i * 100));
    }

    assert(!historico.vazio());
    assert(historico.tamanho() == 3);

    const gcs::EstadoVeiculo ultimo = historico.ultimo(&recebeu);
    assert(recebeu);
    assert(ultimo.posicao.altitude_relativa_m == 50.0);

    const gcs::Historico::Serie serie = historico.serie(
        [](const gcs::EstadoVeiculo& e) { return e.posicao.altitude_relativa_m; });

    assert(!serie.vazia());
    assert(serie.valores.size() == 3);
    assert(serie.valores[0] == 30.0);
    assert(serie.valores[1] == 40.0);
    assert(serie.valores[2] == 50.0);

    historico.limpar();
    assert(historico.vazio());
    historico.ultimo(&recebeu);
    assert(!recebeu);
}

void testar_exportacao_csv() {
    gcs::Historico historico;
    historico.registrar(criar_estado_exemplo(15.0, 5.0, -2.0, 180.0, 88.0, 0));
    historico.registrar(criar_estado_exemplo(20.0, 6.0, -1.0, 185.0, 87.0, 200));

    const std::string caminho = "/tmp/pugcs_teste_voo.csv";
    historico.exportar_csv(caminho);

    std::ifstream arquivo(caminho);
    assert(arquivo.is_open());

    std::string cabecalho;
    std::getline(arquivo, cabecalho);
    assert(!cabecalho.empty());

    int linhas_dados = 0;
    std::string linha;
    while (std::getline(arquivo, linha)) {
        if (!linha.empty()) {
            ++linhas_dados;
        }
    }
    assert(linhas_dados == 2);

    arquivo.close();
    std::remove(caminho.c_str());
}

void testar_paineis_como_funcoes_puras() {
    gcs::Historico historico;
    historico.registrar(criar_estado_exemplo(12.0, 10.0, 4.0, 45.0, 85.0, 0));
    historico.registrar(criar_estado_exemplo(28.0, 42.0, -8.0, 135.0, 14.0, 500));

    const gcs::HorizonteArtificial horizonte;
    const std::string quadro_horizonte = horizonte.quadro(historico);
    assert(!quadro_horizonte.empty());
    assert(quadro_horizonte.find("GUIDED") != std::string::npos);

    const auto& campos = gcs::campos_disponiveis();
    assert(campos.count("altitude") == 1);
    assert(campos.count("velocidade") == 1);
    assert(campos.count("tensao") == 1);

    const gcs::GraficoTempo grafico_alt(campos.at("altitude"));
    const std::string quadro_alt = grafico_alt.quadro(historico);
    assert(!quadro_alt.empty());

    const gcs::GraficoTempo grafico_vel(campos.at("velocidade"));
    const std::string quadro_vel = grafico_vel.quadro(historico);
    assert(!quadro_vel.empty());
}

}  // namespace

int main() {
    testar_buffer_circular_historico();
    testar_exportacao_csv();
    testar_paineis_como_funcoes_puras();

    std::cout << "[OK] Todos os testes de unidade passaram com sucesso.\n";
    return 0;
}
