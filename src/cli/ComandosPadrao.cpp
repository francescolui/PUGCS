#include "cli/ComandosPadrao.hpp"

#include <iostream>
#include <memory>

#include "EstacaoDeControle.hpp"
#include "modelo/Veiculo.hpp"
#include "visual/GraficoTempo.hpp"
#include "visual/HorizonteArtificial.hpp"

namespace gcs {
namespace {

// ---------------------------------------------------------------------------
// Comandos de exemplo. Cada um é uma classe pequena; o console não sabe quais
// existem, só pede ao registro o comando com o nome digitado.
// ---------------------------------------------------------------------------

class Ajuda : public Comando {
public:
    std::string nome() const override { return "ajuda"; }
    std::string ajuda() const override { return "lista os comandos disponíveis"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        std::cout << "Comandos:\n";
        for (const Comando* comando : estacao.comandos().todos()) {
            std::cout << "  " << comando->linha_de_ajuda() << '\n';
        }
    }
};

class Status : public Comando {
public:
    std::string nome() const override { return "status"; }
    std::string ajuda() const override { return "mostra o modo de voo e a telemetria atual"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        std::cout << estacao.veiculo() << '\n';
    }
};

class Modo : public Comando {
public:
    std::string nome() const override { return "modo"; }
    std::string uso() const override { return "modo <NOME>"; }
    std::string ajuda() const override { return "troca o modo de voo (ex.: modo RTL)"; }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        estacao.veiculo().alterar_modo_de_voo(argumentos.front());
        std::cout << "pedido de troca de modo enviado\n";
    }
};

class Plotar : public Comando {
public:
    std::string nome() const override { return "plotar"; }
    std::string uso() const override { return "plotar <campo>"; }
    std::string ajuda() const override {
        return "abre o gráfico de um campo: " + nomes_dos_campos();
    }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        const auto& catalogo = campos_disponiveis();
        const auto encontrado = catalogo.find(argumentos.front());
        if (encontrado == catalogo.end()) {
            throw ErroDeComando("campo '" + argumentos.front() +
                                "' não existe. Disponíveis: " + nomes_dos_campos());
        }
        const GraficoTempo grafico{encontrado->second};
        estacao.exibir_painel(grafico);
    }
};

class Horizonte : public Comando {
public:
    std::string nome() const override { return "horizonte"; }
    std::string ajuda() const override { return "abre o horizonte artificial"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        const HorizonteArtificial horizonte;
        estacao.exibir_painel(horizonte);
    }
};

class Armar : public Comando {
public:
    std::string nome() const override { return "armar"; }
    std::string ajuda() const override { return "arma os motores do drone"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().armar();
        std::cout << "comando de armar enviado\n";
    }
};

class Desarmar : public Comando {
public:
    std::string nome() const override { return "desarmar"; }
    std::string ajuda() const override { return "desarma os motores do drone"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().desarmar();
        std::cout << "comando de desarmar enviado\n";
    }
};

class Decolar : public Comando {
public:
    std::string nome() const override { return "decolar"; }
    std::string uso() const override { return "decolar <altura_m>"; }
    std::string ajuda() const override {
        return "arma o drone e decola ate a altura pedida";
    }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        double altura = 0.0;
        try {
            altura = std::stod(argumentos.front());
        } catch (...) {
            throw ErroDeComando("altura invalida: '" + argumentos.front() + "'");
        }
        if (altura <= 0.0) {
            throw ErroDeComando("a altura deve ser maior que zero");
        }

        estacao.veiculo().armar();
        estacao.veiculo().decolar(altura);
        std::cout << "comando de decolagem para " << altura << "m enviado\n";
    }
};

class Pousar : public Comando {
public:
    std::string nome() const override { return "pousar"; }
    std::string ajuda() const override { return "inicia o pouso no local atual"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().pousar();
        std::cout << "comando de pouso enviado\n";
    }
};

class Rtl : public Comando {
public:
    std::string nome() const override { return "rtl"; }
    std::string ajuda() const override { return "retorna ao ponto de decolagem (Return To Launch)"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().retornar_para_casa();
        std::cout << "comando RTL enviado\n";
    }
};

class Limpar : public Comando {
public:
    std::string nome() const override { return "limpar"; }
    std::string ajuda() const override { return "apaga o historico de telemetria"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.historico().limpar();
        std::cout << "historico limpo\n";
    }
};

class Gravar : public Comando {
public:
    std::string nome() const override { return "gravar"; }
    std::string uso() const override { return "gravar <arquivo.csv>"; }
    std::string ajuda() const override { return "salva o historico de telemetria em CSV"; }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        try {
            estacao.historico().exportar_csv(argumentos.front());
        } catch (const std::exception& erro) {
            throw ErroDeComando(erro.what());
        }
        std::cout << "historico exportado para '" << argumentos.front() << "'\n";
    }
};

class Sair : public Comando {
public:
    std::string nome() const override { return "sair"; }
    std::string ajuda() const override { return "encerra a estação de controle"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.encerrar();
    }
};

}  // namespace

RegistroDeComandos comandos_padrao() {
    RegistroDeComandos registro;
    registro.registrar(std::make_unique<Ajuda>());
    registro.registrar(std::make_unique<Status>());
    registro.registrar(std::make_unique<Modo>());
    registro.registrar(std::make_unique<Plotar>());
    registro.registrar(std::make_unique<Horizonte>());
    registro.registrar(std::make_unique<Armar>());
    registro.registrar(std::make_unique<Desarmar>());
    registro.registrar(std::make_unique<Decolar>());
    registro.registrar(std::make_unique<Pousar>());
    registro.registrar(std::make_unique<Rtl>());
    registro.registrar(std::make_unique<Limpar>());
    registro.registrar(std::make_unique<Gravar>());
    registro.registrar(std::make_unique<Sair>());
    return registro;
}

}  // namespace gcs
