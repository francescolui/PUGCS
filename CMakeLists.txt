cmake_minimum_required(VERSION 3.16)
project(mini_gcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Liga/desliga a aplicação completa (exige MAVSDK).
# Para testar só o painel: cmake -S . -B build -DGCS_COM_MAVSDK=OFF
option(GCS_COM_MAVSDK "Compila a GCS completa (exige MAVSDK)" ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# Janela: GLFW + Dear ImGui + ImPlot (não precisam do MAVSDK)
# ---------------------------------------------------------------------------
find_package(OpenGL REQUIRED)

FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.6
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(implot
  GIT_REPOSITORY https://github.com/epezent/implot.git
  GIT_TAG v0.16
)
FetchContent_MakeAvailable(implot)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# ---------------------------------------------------------------------------
# Aplicação completa: só configura se GCS_COM_MAVSDK=ON
# ---------------------------------------------------------------------------
if(GCS_COM_MAVSDK)
    list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local")
    find_package(MAVSDK REQUIRED)
    find_package(Threads REQUIRED)

    add_executable(mini_gcs
        src/main.cpp
        src/EstacaoDeControle.cpp
        src/modelo/Telemetria.cpp
        src/modelo/Veiculo.cpp
        src/modelo/Drone.cpp
        src/modelo/Historico.cpp
        src/cli/Comando.cpp
        src/cli/ComandosPadrao.cpp
        src/cli/Console.cpp
        src/visual/Painel.cpp
        src/visual/GraficoTempo.cpp
        src/visual/HorizonteArtificial.cpp
        src/visual/PainelImGui.cpp
    )
    target_include_directories(mini_gcs PRIVATE src)
    target_link_libraries(mini_gcs PRIVATE MAVSDK::mavsdk Threads::Threads imgui)
    target_compile_options(mini_gcs PRIVATE -Wall -Wextra -Wpedantic)
endif()