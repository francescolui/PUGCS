#include "visual/HorizonteArtificial.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "modelo/Telemetria.hpp"
#include "visual/Terminal.hpp"

namespace gcs {
namespace {

/// Quantos graus cada caractere vale. Como um caractere de terminal é mais ou
/// menos duas vezes mais alto do que largo, uma coluna vale metade de uma linha
/// — é isso que mantém o horizonte com a inclinação correta.
constexpr double GRAUS_POR_LINHA = 4.0;
constexpr double GRAUS_POR_COLUNA = 2.0;

constexpr double LIMITE_ROLAGEM_DEG = 35.0;
constexpr double LIMITE_ARFAGEM_DEG = 25.0;
constexpr double LIMITE_BATERIA_PCT = 20.0;

constexpr double PI = 3.14159265358979323846;

/// O que há atrás de cada caractere.
enum class Fundo { Ceu, Chao, Horizonte, Caixa, Alerta };
/// A cor do caractere desenhado em cima do fundo.
enum class Tinta { Padrao, Simbolo, Texto };

struct Celula {
    Fundo fundo = Fundo::Ceu;
    Tinta tinta = Tinta::Padrao;
    char glifo = ' ';
};

const std::string FUNDO_CEU = terminal::fundo(0x2A, 0x78, 0xD6);
const std::string FUNDO_CHAO = terminal::fundo(0x8C, 0x5A, 0x2B);
const std::string FUNDO_HORIZONTE = terminal::fundo(0xFC, 0xFC, 0xFB);
const std::string FUNDO_CAIXA = terminal::fundo(0x1A, 0x1A, 0x19);
const std::string FUNDO_ALERTA = terminal::fundo(0xB8, 0x22, 0x22);
const std::string FRENTE_PADRAO = terminal::frente(0xFC, 0xFC, 0xFB);
const std::string FRENTE_SIMBOLO = terminal::frente(0xED, 0xA1, 0x00);
const std::string FRENTE_TEXTO = terminal::frente(0xFC, 0xFC, 0xFB);

const std::string& codigo(Fundo fundo) {
    switch (fundo) {
        case Fundo::Chao: return FUNDO_CHAO;
        case Fundo::Horizonte: return FUNDO_HORIZONTE;
        case Fundo::Caixa: return FUNDO_CAIXA;
        case Fundo::Alerta: return FUNDO_ALERTA;
        case Fundo::Ceu: break;
    }
    return FUNDO_CEU;
}

const std::string& codigo(Tinta tinta) {
    switch (tinta) {
        case Tinta::Simbolo: return FRENTE_SIMBOLO;
        case Tinta::Texto: return FRENTE_TEXTO;
        case Tinta::Padrao: break;
    }
    return FRENTE_PADRAO;
}

std::string formatar_numero(double valor, int largura, int decimais, bool mostrar_sinal = false) {
    std::ostringstream fluxo;
    fluxo << std::fixed << std::setprecision(decimais);
    if (mostrar_sinal) {
        fluxo << std::showpos;
    }
    fluxo << std::setw(largura) << valor;
    return fluxo.str();
}

std::string graus(double valor) {
    return formatar_numero(valor, 7, 1, true);
}

std::string rotulo_bussola(int grau_normalizado) {
    switch (grau_normalizado) {
        case 0: return "N";
        case 45: return "NE";
        case 90: return "L";
        case 135: return "SE";
        case 180: return "S";
        case 225: return "SO";
        case 270: return "O";
        case 315: return "NO";
        default: return "";
    }
}

}  // namespace

HorizonteArtificial::HorizonteArtificial(int largura, int altura)
    : Painel("Horizonte artificial"),
      largura_(std::max(40, largura)),
      altura_(std::max(11, altura | 1)) {}  // altura ímpar: existe uma linha central

std::string HorizonteArtificial::quadro(const Historico& historico) const {
    bool recebeu = false;
    const EstadoVeiculo estado = historico.ultimo(&recebeu);
    std::ostringstream saida;
    if (!recebeu) {
        saida << titulo() << terminal::LIMPAR_LINHA << "\n\n"
              << "aguardando telemetria..." << terminal::LIMPAR_LINHA << "\n";
        return saida.str();
    }

    // 1. Decide, caractere por caractere, se ali é céu, chão ou horizonte.
    const double seno = std::sin(estado.atitude.rolagem * PI / 180.0);
    const double cosseno = std::cos(estado.atitude.rolagem * PI / 180.0);
    const double centro_linha = (altura_ - 1) / 2.0;
    const double centro_coluna = (largura_ - 1) / 2.0;

    std::vector<std::vector<Celula>> tela(
        static_cast<std::size_t>(altura_),
        std::vector<Celula>(static_cast<std::size_t>(largura_)));

    for (int linha = 0; linha < altura_; ++linha) {
        for (int coluna = 0; coluna < largura_; ++coluna) {
            const double y = (centro_linha - linha) * GRAUS_POR_LINHA;
            const double x = (coluna - centro_coluna) * GRAUS_POR_COLUNA;
            const double u = -x * seno + y * cosseno + estado.atitude.arfagem;
            Celula& celula = tela[static_cast<std::size_t>(linha)][static_cast<std::size_t>(coluna)];
            if (std::abs(u) <= GRAUS_POR_LINHA * 0.6) {
                celula.fundo = Fundo::Horizonte;
            } else {
                celula.fundo = (u > 0.0) ? Fundo::Ceu : Fundo::Chao;
            }
        }
    }

    const int linha_central = altura_ / 2;
    const int coluna_central = largura_ / 2;

    const auto escrever = [&](int linha, int coluna, char glifo, Tinta tinta,
                              std::optional<Fundo> fundo = std::nullopt) {
        if (linha < 0 || linha >= altura_ || coluna < 0 || coluna >= largura_) {
            return;
        }
        Celula& celula = tela[static_cast<std::size_t>(linha)][static_cast<std::size_t>(coluna)];
        celula.glifo = glifo;
        celula.tinta = tinta;
        if (fundo.has_value()) {
            celula.fundo = *fundo;
        }
    };

    const auto escrever_texto = [&](int linha, int coluna, const std::string& texto, Tinta tinta,
                                    std::optional<Fundo> fundo = std::nullopt) {
        for (std::size_t i = 0; i < texto.size(); ++i) {
            escrever(linha, coluna + static_cast<int>(i), texto[i], tinta, fundo);
        }
    };

    // 2. Escada de arfagem (pitch ladder): linhas a cada 10 graus rotacionadas
    //    junto com o horizonte, com o valor numérico na extremidade direita.
    for (int degrau = -30; degrau <= 30; degrau += 10) {
        if (degrau == 0) {
            continue;
        }
        const double u_rel = static_cast<double>(degrau) - estado.atitude.arfagem;
        const char traco = (degrau > 0) ? '-' : '.';

        for (int v_deg = -10; v_deg <= 10; v_deg += 2) {
            if (std::abs(v_deg) < 4) {
                continue;  // deixa o centro livre para o símbolo da aeronave
            }
            const double v = static_cast<double>(v_deg);
            const double x = v * cosseno - u_rel * seno;
            const double y = v * seno + u_rel * cosseno;
            const int lin = static_cast<int>(std::lround(centro_linha - y / GRAUS_POR_LINHA));
            const int col = static_cast<int>(std::lround(centro_coluna + x / GRAUS_POR_COLUNA));
            if (lin >= 2 && lin < altura_ - 2) {
                escrever(lin, col, traco, Tinta::Padrao);
            }
        }

        // Rótulo numérico na ponta direita da barra (ex.: +10, -10)
        const double v_rotulo = 12.0;
        const double x_rot = v_rotulo * cosseno - u_rel * seno;
        const double y_rot = v_rotulo * seno + u_rel * cosseno;
        const int lin_rot = static_cast<int>(std::lround(centro_linha - y_rot / GRAUS_POR_LINHA));
        const int col_rot = static_cast<int>(std::lround(centro_coluna + x_rot / GRAUS_POR_COLUNA));
        if (lin_rot >= 2 && lin_rot < altura_ - 2) {
            std::ostringstream oss;
            oss << std::showpos << degrau;
            escrever_texto(lin_rot, col_rot, oss.str(), Tinta::Padrao);
        }
    }

    // 3. Símbolo da aeronave, sempre fixo no centro (referência do piloto).
    for (int distancia = 5; distancia <= 11; ++distancia) {
        escrever(linha_central, coluna_central - distancia, '=', Tinta::Simbolo);
        escrever(linha_central, coluna_central + distancia, '=', Tinta::Simbolo);
    }
    escrever(linha_central, coluna_central, 'o', Tinta::Simbolo);

    // 4. Barra superior: indicador de rolagem (-30 a +30 deg) com ponteiro móvel.
    for (int col = 0; col < largura_; ++col) {
        escrever(0, col, ' ', Tinta::Texto, Fundo::Caixa);
    }
    for (int marca = -30; marca <= 30; marca += 15) {
        const int col_marca =
            coluna_central + static_cast<int>(std::lround(marca / GRAUS_POR_COLUNA));
        escrever(0, col_marca, (marca == 0) ? '|' : '.', Tinta::Texto, Fundo::Caixa);
    }
    const double rolagem_limitada = std::clamp(estado.atitude.rolagem, -45.0, 45.0);
    const int col_ponteiro_rol =
        coluna_central + static_cast<int>(std::lround(rolagem_limitada / GRAUS_POR_COLUNA));
    const bool alerta_rolagem = std::abs(estado.atitude.rolagem) > LIMITE_ROLAGEM_DEG;
    const bool alerta_arfagem = std::abs(estado.atitude.arfagem) > LIMITE_ARFAGEM_DEG;
    escrever(0, col_ponteiro_rol, 'v', alerta_rolagem ? Tinta::Padrao : Tinta::Simbolo,
             alerta_rolagem ? std::optional<Fundo>(Fundo::Alerta) : std::nullopt);

    // 5. Fita de rumo (compasso) na base do HUD.
    const int linha_compasso = altura_ - 1;
    for (int col = 0; col < largura_; ++col) {
        escrever(linha_compasso, col, ' ', Tinta::Texto, Fundo::Caixa);
        const double offset_deg = (col - coluna_central) * 3.0;
        const int rumo_col = static_cast<int>(std::lround(estado.atitude.guinada + offset_deg));
        const int rumo_norm = ((rumo_col % 360) + 360) % 360;

        if (rumo_norm % 45 == 0) {
            const std::string cardeal = rotulo_bussola(rumo_norm);
            escrever_texto(linha_compasso, col, cardeal, Tinta::Simbolo, Fundo::Caixa);
        } else if (rumo_norm % 15 == 0) {
            escrever(linha_compasso, col, '|', Tinta::Texto, Fundo::Caixa);
        }
    }
    escrever(linha_compasso - 1, coluna_central, '^', Tinta::Simbolo);

    // 6. Caixas de telemetria (esquerda: atitude/modo; direita: altitude/vel/bateria)
    //    e destaques em vermelho quando houver alerta ou falha.
    const bool alerta_bateria = estado.bateria.percentual < LIMITE_BATERIA_PCT;
    const bool alerta_gps = estado.gps.num_satelites < 6;
    const bool modo_alerta = (estado.modo_de_voo == "RTL" || estado.modo_de_voo == "LAND" ||
                              estado.modo_de_voo == "DESCONHECIDO");

    struct LinhaCaixa {
        std::string texto;
        bool perigo = false;
    };

    std::vector<LinhaCaixa> caixa_esq{
        {" modo    " + estado.modo_de_voo, modo_alerta},
        {" rolagem " + graus(estado.atitude.rolagem) + " deg", alerta_rolagem},
        {" arfagem " + graus(estado.atitude.arfagem) + " deg", alerta_arfagem},
        {" rumo    " + formatar_numero(estado.atitude.guinada, 7, 1, false) + " deg", false},
    };

    std::size_t larg_esq = 0;
    for (const auto& item : caixa_esq) {
        larg_esq = std::max(larg_esq, item.texto.size() + 1);
    }
    for (std::size_t i = 0; i < caixa_esq.size(); ++i) {
        caixa_esq[i].texto.resize(larg_esq, ' ');
        const Fundo fundo_linha = caixa_esq[i].perigo ? Fundo::Alerta : Fundo::Caixa;
        escrever_texto(static_cast<int>(i) + 2, 2, caixa_esq[i].texto, Tinta::Texto, fundo_linha);
    }

    std::vector<LinhaCaixa> caixa_dir{
        {" alt " + formatar_numero(estado.posicao.altitude_relativa_m, 6, 1) + " m", false},
        {" vel " + formatar_numero(estado.velocidade.velocidade_solo_ms, 6, 1) + " m/s", false},
        {" bat " + formatar_numero(estado.bateria.percentual, 6, 0) + " %", alerta_bateria},
        {" gps " + std::to_string(estado.gps.num_satelites) + " sat", alerta_gps},
    };

    std::size_t larg_dir = 0;
    for (const auto& item : caixa_dir) {
        larg_dir = std::max(larg_dir, item.texto.size() + 1);
    }
    const int col_inicio_dir = largura_ - static_cast<int>(larg_dir) - 2;
    if (col_inicio_dir > static_cast<int>(larg_esq) + 4) {
        for (std::size_t i = 0; i < caixa_dir.size(); ++i) {
            caixa_dir[i].texto.resize(larg_dir, ' ');
            const Fundo fundo_linha = caixa_dir[i].perigo ? Fundo::Alerta : Fundo::Caixa;
            escrever_texto(static_cast<int>(i) + 2, col_inicio_dir, caixa_dir[i].texto,
                           Tinta::Texto, fundo_linha);
        }
    }

    // Banner central de alerta crítico (atitude excessiva ou bateria baixa)
    if (alerta_rolagem || alerta_arfagem) {
        const std::string aviso = " ! ATITUDE CRITICA ! ";
        escrever_texto(2, coluna_central - static_cast<int>(aviso.size() / 2), aviso,
                       Tinta::Texto, Fundo::Alerta);
    } else if (alerta_bateria) {
        const std::string aviso = " ! BATERIA BAIXA ! ";
        escrever_texto(2, coluna_central - static_cast<int>(aviso.size() / 2), aviso,
                       Tinta::Texto, Fundo::Alerta);
    }

    // 7. Emite a tela, trocando as cores só quando elas mudam de verdade.
    saida << terminal::NEGRITO << titulo() << terminal::NORMAL << terminal::LIMPAR_LINHA << "\n";
    for (const std::vector<Celula>& linha : tela) {
        const Fundo* fundo_atual = nullptr;
        const Tinta* tinta_atual = nullptr;
        for (const Celula& celula : linha) {
            if (fundo_atual == nullptr || celula.fundo != *fundo_atual) {
                saida << codigo(celula.fundo);
                fundo_atual = &celula.fundo;
            }
            if (tinta_atual == nullptr || celula.tinta != *tinta_atual) {
                saida << codigo(celula.tinta);
                tinta_atual = &celula.tinta;
            }
            saida << celula.glifo;
        }
        saida << terminal::NORMAL << terminal::LIMPAR_LINHA << "\n";
    }
    saida << "\n"
          << terminal::frente(0x82, 0x81, 0x7C) << "[Enter] volta para o console"
          << terminal::NORMAL << terminal::LIMPAR_LINHA << "\n";
    return saida.str();
}

}  // namespace gcs

// TODO (aluno): o horizonte está no mínimo necessário. Melhorias naturais:
//  * escada de arfagem: linhas a cada 10 graus, com o número ao lado;
//  * arco de rolagem no topo, com um ponteiro triangular;
//  * fita de rumo (compasso) na base e fitas de altitude/velocidade nas laterais;
//  * pintar de vermelho quando a rolagem passar de um limite de segurança;
//  * indicador de bateria e de modo de voo com destaque quando houver falha.

// namespace gcs
