#include "visual/PainelImGui.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include "modelo/Telemetria.hpp"

namespace gcs {

PainelImGui::PainelImGui(std::string titulo, int largura, int altura)
    : titulo_(std::move(titulo)), largura_(largura), altura_(altura) {}

void PainelImGui::exibir(const Historico& historico) const {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    GLFWwindow* janela = glfwCreateWindow(largura_, altura_, titulo_.c_str(), nullptr, nullptr);
    if (!janela) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(janela);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(janela, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(janela)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            glfwSetWindowShouldClose(janela, GLFW_TRUE);
        }
        ImGui::Begin(titulo_.c_str());
        if (ImGui::Button("Voltar ao console")) {
            glfwSetWindowShouldClose(janela, GLFW_TRUE);
        }
        bool recebeu = false;
        const EstadoVeiculo estado = historico.ultimo(&recebeu);
        if (!recebeu) {
            ImGui::Text("aguardando telemetria...");
        } else {
            ImGui::Text("Altitude: %.1f m", estado.posicao.altitude_relativa_m);
            ImGui::Text("Bateria:  %.0f %%", estado.bateria.percentual);
            ImGui::Text("Modo:     %s", estado.modo_de_voo.c_str());
        }

       
        const Historico::Serie serie = historico.serie(
            [](const EstadoVeiculo& e) { return e.posicao.altitude_relativa_m; });


        if (!serie.vazia() && ImPlot::BeginPlot("Altitude relativa", ImVec2(-1, -1))) {
            ImPlot::SetupAxes("tempo (s)", "altitude (m)");
            ImPlot::PlotLine("altitude",
                             serie.segundos.data(),
                             serie.valores.data(),
                             static_cast<int>(serie.valores.size()));
            ImPlot::EndPlot();
        }

        ImGui::End();
        ImGui::Render();

        int largura_fb = 0;
        int altura_fb = 0;
        glfwGetFramebufferSize(janela, &largura_fb, &altura_fb);
        glViewport(0, 0, largura_fb, altura_fb);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(janela);
    }

   
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(janela);
    glfwTerminate();
}

}  // namespace gcs