#pragma once

#include <string>

#include "modelo/Historico.hpp"

namespace gcs {

class PainelImGui {
public:
    explicit PainelImGui(std::string titulo, int largura = 900, int altura = 600);

    void exibir(const Historico& historico) const;

private:
    std::string titulo_;
    int largura_;
    int altura_;
};

}  // namespace gcs