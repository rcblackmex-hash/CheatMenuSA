cmake_minimum_required(VERSION 3.18)
project(CheatMenuSA)

# ── Configuración ──────────────────────────────────────────────
set(MOD_NAME "CheatMenu")
set(MOD_VERSION "2.10")

# Apuntar al SDK de AML (cambia esta ruta a donde tengas el SDK)
set(AML_SDK_PATH "${CMAKE_SOURCE_DIR}/sdk/aml")
set(IMGUI_PATH   "${CMAKE_SOURCE_DIR}/imgui")

# ── Fuentes ImGui ──────────────────────────────────────────────
set(IMGUI_SOURCES
    ${IMGUI_PATH}/imgui.cpp
    ${IMGUI_PATH}/imgui_draw.cpp
    ${IMGUI_PATH}/imgui_tables.cpp
    ${IMGUI_PATH}/imgui_widgets.cpp
    ${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp
    # imgui_impl_android.cpp si quieres touch nativo de ImGui
)

# ── Biblioteca principal ───────────────────────────────────────
add_library(
    ${MOD_NAME}
    SHARED
    src/main.cpp
    ${IMGUI_SOURCES}
)

# ── Includes ───────────────────────────────────────────────────
target_include_directories(${MOD_NAME} PRIVATE
    src/
    ${IMGUI_PATH}
    ${IMGUI_PATH}/backends
    ${AML_SDK_PATH}/include
)

# ── Bibliotecas del sistema Android ───────────────────────────
find_library(log-lib       log)
find_library(android-lib   android)
find_library(EGL-lib       EGL)
find_library(GLESv3-lib    GLESv3)

target_link_libraries(${MOD_NAME}
    ${log-lib}
    ${android-lib}
    ${EGL-lib}
    ${GLESv3-lib}
)

# ── Flags de compilación ───────────────────────────────────────
target_compile_options(${MOD_NAME} PRIVATE
    -O2
    -fvisibility=hidden     # Solo exportar lo que necesitamos
    -fno-exceptions
    -fno-rtti
    -std=c++17
    -Wall
    -Wno-unused-variable
)

# Definir arquitectura target
target_compile_definitions(${MOD_NAME} PRIVATE
    ANDROID
    __ANDROID__
    IMGUI_IMPL_OPENGL_ES3
)

# ── Output ─────────────────────────────────────────────────────
# El .so resultante se llama libCheatMenu.so
# Copiarlo a: Android/data/com.rockstargames.gtasa/mods/
set_target_properties(${MOD_NAME} PROPERTIES
    OUTPUT_NAME "${MOD_NAME}"
    PREFIX "lib"
)
