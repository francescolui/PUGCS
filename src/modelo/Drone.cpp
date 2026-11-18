#define MAVSDK_ENABLE_MAVLINK_C_API

#include "modelo/Drone.hpp"
#include "modelo/Veiculo.hpp"

#include <mavsdk/mavsdk.hpp>
#include <mavsdk/plugins/action/action.hpp>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.hpp>
#include <mavsdk/plugins/telemetry/telemetry.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <map>
#include <sstream>
#include <utility>

namespace gcs {
namespace {

std::string para_maiusculas(std::string texto) {
    std::transform(texto.begin(), texto.end(), texto.begin(), [](unsigned char letra) {
        return static_cast<char>(std::toupper(letra));
    });
    return texto;
}

/// O MAVSDK imprime os enums com operator<<; é a forma mais simples de
/// transformar um FlightMode em texto.
template <typename T>
std::string para_texto(const T& valor) {
    std::ostringstream fluxo;
    fluxo << valor;
    return fluxo.str();
}

}  // namespace

std::string normalizar_endereco(const std::string& endereco) {
    if(endereco.find("://") != std::string::npos) {
        return endereco;  // já está no formato de URL
    }
    const std::size_t separador = endereco.find(':');
    if(separador == std::string::npos) {
        return endereco;
    }
    std::string esquema = endereco.substr(0, separador);
    const std::string resto = endereco.substr(separador + 1);
    // "tcp" é ambíguo para o MAVSDK; para falar com o SITL queremos "tcpout".
    if(esquema == "tcp") {
        esquema = "tcpout";
    }
    return esquema + "://" + resto;
}

/// Tudo que depende do MAVSDK vive aqui dentro.
class Drone::Impl {
public:
    Impl()
        : nucleo(mavsdk::Mavsdk::Configuration{mavsdk::ComponentType::GroundStation}) {}

    mavsdk::Mavsdk nucleo;
    std::shared_ptr<mavsdk::System> sistema;
    std::unique_ptr<mavsdk::Telemetry> telemetria;
    std::unique_ptr<mavsdk::Action> acoes;
    std::unique_ptr<mavsdk::MavlinkPassthrough> passthrough;
};

Drone::Drone(std::string nome, std::string endereco)
    : Veiculo(std::move(nome)),
      impl_(std::make_unique<Impl>()),
      endereco_(std::move(endereco)) {}

Drone::~Drone() = default;  // precisa estar no .cpp por causa do pimpl

void Drone::conectar() {
    const std::string url = normalizar_endereco(endereco_);
    std::cout << "[" << nome() << "] conectando em " << url << " ..." << std::endl;

    const mavsdk::ConnectionResult resultado = impl_->nucleo.add_any_connection(url);
    if(resultado != mavsdk::ConnectionResult::Success) {
        throw ErroDeVeiculo("não foi possível abrir " + url + ": " + para_texto(resultado));
    }

    // O primeiro autopiloto que aparecer na rede MAVLink é o nosso drone.
    const auto sistema = impl_->nucleo.first_autopilot(TEMPO_LIMITE_S);
    if(!sistema) {
        throw ErroDeVeiculo(
            "nenhum autopiloto respondeu em " + url + ". O SITL do ArduPilot está rodando?");
    }

    impl_->sistema = sistema.value();
    impl_->telemetria = std::make_unique<mavsdk::Telemetry>(impl_->sistema);
    impl_->acoes = std::make_unique<mavsdk::Action>(impl_->sistema);
    impl_->passthrough = std::make_unique<mavsdk::MavlinkPassthrough>(impl_->sistema);
    std::cout << "[" << nome() << "] conectado." << std::endl;
}

void Drone::iniciar_telemetria() {
    if(!impl_->telemetria) {
        throw ErroDeVeiculo("iniciar_telemetria() antes de conectar()");
    }
    mavsdk::Telemetry& telemetria = *impl_->telemetria;

    // Solicita explicitamente os streams MAVLink (necessário na SERIAL1 / portas auxiliares)
    telemetria.set_rate_position(4.0);
    telemetria.set_rate_attitude_euler(10.0);
    telemetria.set_rate_battery(1.0);
    telemetria.set_rate_gps_info(2.0);
    telemetria.set_rate_velocity_ned(4.0);
    telemetria.set_rate_in_air(2.0);

    telemetria.subscribe_flight_mode([this](mavsdk::Telemetry::FlightMode modo) {
        const std::string texto = para_texto(modo);
        atualizar([&texto](EstadoVeiculo& estado) { estado.modo_de_voo = texto; });
    });

    telemetria.subscribe_attitude_euler([this](mavsdk::Telemetry::EulerAngle angulo) {
        atualizar([&angulo](EstadoVeiculo& estado) {
            estado.atitude.rolagem = angulo.roll_deg;
            estado.atitude.arfagem = angulo.pitch_deg;
            estado.atitude.guinada = angulo.yaw_deg;
        });
    });

    telemetria.subscribe_position([this](mavsdk::Telemetry::Position posicao) {
        atualizar([&posicao](EstadoVeiculo& estado) {
            estado.posicao.latitude = posicao.latitude_deg;
            estado.posicao.longitude = posicao.longitude_deg;
            estado.posicao.altitude_absoluta_m = posicao.absolute_altitude_m;
            estado.posicao.altitude_relativa_m = posicao.relative_altitude_m;
        });
    });

    telemetria.subscribe_battery([this](mavsdk::Telemetry::Battery bateria) {
        atualizar([&bateria](EstadoVeiculo& estado) {
            estado.bateria.tensao_v = bateria.voltage_v;
            estado.bateria.percentual = bateria.remaining_percent;
        });
    });

    telemetria.subscribe_armed([this](bool armado) {
        atualizar([armado](EstadoVeiculo& estado) { estado.armado = armado; });
    });

    telemetria.subscribe_in_air([this](bool em_voo) {
        atualizar([em_voo](EstadoVeiculo& estado) { estado.em_voo = em_voo; });
    });

    telemetria.subscribe_velocity_ned([this](mavsdk::Telemetry::VelocityNed vel) {
        atualizar([&vel](EstadoVeiculo& estado) {
            estado.velocidade.velocidade_solo_ms = std::sqrt(vel.north_m_s * vel.north_m_s +
                                                            vel.east_m_s * vel.east_m_s);
            estado.velocidade.taxa_subida_ms = -vel.down_m_s;
        });
    });

    telemetria.subscribe_gps_info([this](mavsdk::Telemetry::GpsInfo gps) {
        atualizar([&gps](EstadoVeiculo& estado) {
            estado.gps.num_satelites = gps.num_satellites;
            estado.gps.tipo_fixo = static_cast<int>(gps.fix_type);
        });
    });
}

void Drone::alterar_modo_de_voo(const std::string& modo) {
    if(!impl_->passthrough) {
        throw ErroDeVeiculo("alterar_modo_de_voo() chamado antes de conectar()");
    }

    const std::string modo_upper = para_maiusculas(modo);

    // Tabela oficial custom_mode do ArduCopter
    static const std::map<std::string, uint32_t> modos_arducopter = {
        {"STABILIZE", 0},
        {"ACRO",      1},
        {"ALT_HOLD",  2},
        {"AUTO",      3},
        {"GUIDED",    4},
        {"LOITER",    5},
        {"RTL",       6},
        {"CIRCLE",    7},
        {"LAND",      9},
        {"POSHOLD",   16},
        {"BRAKE",     17}
    };

    const auto it = modos_arducopter.find(modo_upper);
    if(it == modos_arducopter.end()) {
        throw ErroDeVeiculo("modo de voo desconhecido: " + modo);
    }

    mavsdk::MavlinkPassthrough::CommandLong cmd{};
    cmd.target_sysid = impl_->passthrough->get_target_sysid();
    cmd.target_compid = impl_->passthrough->get_target_compid();
    cmd.command = MAV_CMD_DO_SET_MODE;
    cmd.param1 = static_cast<float>(MAV_MODE_FLAG_CUSTOM_MODE_ENABLED);
    cmd.param2 = static_cast<float>(it->second);

    const auto res = impl_->passthrough->send_command_long(cmd);
    if(res != mavsdk::MavlinkPassthrough::Result::Success) {
        throw ErroDeVeiculo("falha ao alterar modo de voo para " + modo_upper + ": " + para_texto(res));
    }
}

void Drone::armar() {
    if(!impl_->acoes) {
        throw ErroDeVeiculo("armar() chamado antes de conectar()");
    }

    const auto resultado = impl_->acoes->arm();
    if(resultado != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("falha ao armar o drone: " + para_texto(resultado));
    }
}

void Drone::desarmar() {
    if(!impl_->acoes) {
        throw ErroDeVeiculo("desarmar() chamado antes de conectar()");
    }

    const auto resultado = impl_->acoes->disarm();
    if(resultado != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("falha ao desarmar o drone: " + para_texto(resultado));
    }
}

void Drone::decolar(double altura_m) {
    if(!impl_->acoes) {
        throw ErroDeVeiculo("decolar() chamado antes de conectar()");
    }

    impl_->acoes->set_takeoff_altitude(static_cast<float>(altura_m));
    const auto resultado = impl_->acoes->takeoff();
    if(resultado != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("falha ao decolar o drone: " + para_texto(resultado));
    }
}

void Drone::pousar() {
    if(!impl_->acoes) {
        throw ErroDeVeiculo("pousar() chamado antes de conectar()");
    }
    const auto resultado = impl_->acoes->land();
    if(resultado != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("falha ao pousar: " + para_texto(resultado));
    }
}

void Drone::retornar_para_casa() {
    if(!impl_->acoes) {
        throw ErroDeVeiculo("retornar_para_casa() chamado antes de conectar()");
    }
    const auto resultado = impl_->acoes->return_to_launch();
    if(resultado != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("falha ao ativar RTL: " + para_texto(resultado));
    }
}

void Drone::ir_para(double latitude_deg, double longitude_deg, float altitude_m) {
    if(!impl_->acoes) {
        throw ErroDeVeiculo("ir_para() chamado antes de conectar()");
    }
    alterar_modo_de_voo("GUIDED");

    const EstadoVeiculo atual = estado();
    const float altitude_amsl =
        atual.posicao.altitude_absoluta_m - atual.posicao.altitude_relativa_m + altitude_m;

    const auto resultado = impl_->acoes->goto_location(
        latitude_deg, longitude_deg, altitude_amsl, NAN);

    if(resultado != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("falha ao enviar ir_para: " + para_texto(resultado));
    }
}

}  // namespace gcs