
#include "visual/PainelImGui.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <GL/gl.h>

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

    glfwMakeContextCurrent(janela); glfwSwapInterval(1);

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
         ImGui_ImplGlfw_InitForOpenGL(janela, true);
         ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(janela)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin(titulo_.c_str());
        ImGui::Text("janela aberta");   
        ImGui::End();
        ImGui::Render();
        int w, h; glfwGetFramebufferSize(janela, &w, &h);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(janela);
    }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(janela);
        glfwTerminate();
    
    (void)historico;
}

}  // namespace gcs
```