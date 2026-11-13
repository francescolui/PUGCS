#include <fstream>
#include <stdexcept>

#include "modelo/Historico.hpp"

namespace gcs {

Historico::Historico(std::size_t capacidade) : capacidade_(capacidade) {}

void Historico::registrar(const EstadoVeiculo& estado) {
    std::lock_guard<std::mutex> trava(mutex_);
    estados_.push_back(estado);
    while (estados_.size() > capacidade_) {
        estados_.pop_front();
    }
}

std::size_t Historico::tamanho() const {
    std::lock_guard<std::mutex> trava(mutex_);
    return estados_.size();
}

bool Historico::vazio() const {
    std::lock_guard<std::mutex> trava(mutex_);
    return estados_.empty();
}

EstadoVeiculo Historico::ultimo(bool* ok) const {
    std::lock_guard<std::mutex> trava(mutex_);
    if (estados_.empty()) {
        if (ok != nullptr) {
            *ok = false;
        }
        return EstadoVeiculo{};
    }
    if (ok != nullptr) {
        *ok = true;
    }
    return estados_.back();
}

Historico::Serie Historico::serie(const Extrator& extrator) const {
    std::lock_guard<std::mutex> trava(mutex_);
    Serie resultado;
    if (estados_.empty()) {
        return resultado;
    }
    const Instante t0 = estados_.front().instante;
    resultado.segundos.reserve(estados_.size());
    resultado.valores.reserve(estados_.size());
    for (const EstadoVeiculo& estado : estados_) {
        resultado.segundos.push_back(segundos_entre(t0, estado.instante));
        resultado.valores.push_back(extrator(estado));
    }
    return resultado;
}

void Historico::limpar() {
    std::lock_guard<std::mutex> trava(mutex_);
    estados_.clear();
}

void Historico::exportar_csv(const std::string& caminho) const {
    std::deque<EstadoVeiculo> copia;
    {
        std::lock_guard<std::mutex> trava(mutex_);
        copia = estados_;
    }

    std::ofstream arquivo(caminho);
    if (!arquivo.is_open()) {
        throw std::runtime_error("nao foi possivel abrir o arquivo para escrita: " + caminho);
    }

    arquivo << "tempo_s,modo,armado,em_voo,altitude_rel_m,rolagem_deg,arfagem_deg,guinada_deg,"
               "vel_solo_ms,taxa_subida_ms,bateria_v,bateria_pct,satelites\n";

    if (copia.empty()) {
        return;
    }

    const Instante inicio = copia.front().instante;
    for (const EstadoVeiculo& e : copia) {
        const double t = segundos_entre(inicio, e.instante);
        arquivo << t << ','
                << e.modo_de_voo << ','
                << (e.armado ? 1 : 0) << ','
                << (e.em_voo ? 1 : 0) << ','
                << e.posicao.altitude_relativa_m << ','
                << e.atitude.rolagem << ','
                << e.atitude.arfagem << ','
                << e.atitude.guinada << ','
                << e.velocidade.velocidade_solo_ms << ','
                << e.velocidade.taxa_subida_ms << ','
                << e.bateria.tensao_v << ','
                << e.bateria.percentual << ','
                << e.gps.num_satelites << '\n';
    }
}

}  // namespace gcs
