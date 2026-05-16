// ============================================================
//  CheatMenu SA v2.10 — main.cpp v5
//  VERSION MINIMALISTA: solo renderiza ImGui, NO toca memoria
//  del juego hasta que el usuario presione un boton.
//  Esto elimina crashes por offsets incorrectos.
// ============================================================

#include "game.h"
#include <AML/AML.h>
#include <jni.h>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <stdio.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <string>
#include <cstdio>
#include <cstring>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"

// ── Estado ────────────────────────────────────────────────────
static bool g_menuVisible = false;
static bool g_imguiReady  = false;
static int  g_screenW     = 1080;
static int  g_screenH     = 2400;

// ── Hook ──────────────────────────────────────────────────────
using fn_eglSwap = EGLBoolean(*)(EGLDisplay, EGLSurface);
static fn_eglSwap real_eglSwapBuffers = nullptr;
static uint8_t*   g_trampoline        = nullptr;

static fn_eglSwap InstallHook(void* target, void* hookFn) {
    g_trampoline = (uint8_t*)mmap(nullptr, 4096,
        PROT_READ|PROT_WRITE|PROT_EXEC,
        MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (g_trampoline == MAP_FAILED) return nullptr;

    uintptr_t page = (uintptr_t)target & ~(uintptr_t)0xFFF;
    mprotect((void*)page, 0x4000, PROT_READ|PROT_WRITE|PROT_EXEC);
    memcpy(g_trampoline, target, 16);

    uintptr_t cont    = (uintptr_t)target + 16;
    uint32_t* jmp     = (uint32_t*)(g_trampoline + 16);
    jmp[0] = 0x58000050;
    jmp[1] = 0xD61F0200;
    memcpy(&jmp[2], &cont, 8);
    __builtin___clear_cache((char*)g_trampoline, (char*)g_trampoline + 32);

    uint32_t hook[4];
    hook[0] = 0x58000050;
    hook[1] = 0xD61F0200;
    memcpy(&hook[2], &hookFn, 8);
    memcpy(target, hook, 16);
    __builtin___clear_cache((char*)target, (char*)target + 16);
    mprotect((void*)page, 0x4000, PROT_READ|PROT_EXEC);

    LOGI("Hook OK target=%p tramp=%p", target, g_trampoline);
    return (fn_eglSwap)g_trampoline;
}

// ── Render SOLO ImGui (sin tocar memoria del juego) ───────────
EGLBoolean hk_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    if (!g_imguiReady) {
        int w=0, h=0;
        eglQuerySurface(dpy, surface, EGL_WIDTH,  &w);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
        if (w > 0 && h > 0) { g_screenW = w; g_screenH = h; }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = {(float)g_screenW, (float)g_screenH};
        io.IniFilename = nullptr;
        io.Fonts->AddFontDefault();
        io.Fonts->Build();
        ImGui_ImplOpenGL3_Init("#version 300 es");
        g_imguiReady = true;
        LOGI("ImGui listo %dx%d", g_screenW, g_screenH);
    }

    ImGui::GetIO().DisplaySize = {(float)g_screenW, (float)g_screenH};
    ImGui::GetIO().DeltaTime   = 1.0f / 60.0f;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // Boton flotante CM
    float sz = g_screenH * 0.07f;
    ImGui::SetNextWindowPos({20, 200}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({sz, sz});
    ImGui::SetNextWindowBgAlpha(0.9f);
    ImGui::Begin("##btn", nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    if (ImGui::Button(g_menuVisible ? "X" : "CM", {sz-16, sz-16}))
        g_menuVisible = !g_menuVisible;
    ImGui::End();

    // Menu principal — SOLO texto, sin tocar memoria del juego
    if (g_menuVisible) {
        ImGui::SetNextWindowPos({100, 100}, ImGuiCond_Once);
        ImGui::SetNextWindowSize({350, 300}, ImGuiCond_Once);
        ImGui::Begin("CheatMenu SA v2.10", &g_menuVisible);
        ImGui::Text("Mod cargado correctamente!");
        ImGui::Separator();
        ImGui::Text("Base libGTASA: 0x%lX", g_libGTASA);
        ImGui::Text("Pantalla: %dx%d", g_screenW, g_screenH);
        ImGui::Separator();
        ImGui::TextColored({1,1,0,1}, "Offsets listos para usar");
        ImGui::Text("GiveWeapon:  0x%lX", (uintptr_t)0x0059525C);
        ImGui::Text("SetWanted:   0x%lX", (uintptr_t)0x005C76D0);
        ImGui::Text("CreateCar:   0x%lX", (uintptr_t)0x003AFA70);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return real_eglSwapBuffers(dpy, surface);
}

// ── Init ──────────────────────────────────────────────────────
static uintptr_t GetLibBase(const char* name) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[512]; uintptr_t base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name) && strstr(line, "r-xp")) {
            base = (uintptr_t)strtoull(line, nullptr, 16);
            break;
        }
    }
    fclose(f);
    LOGI("Base %s: 0x%lX", name, base);
    return base;
}

static void* InitThread(void*) {
    LOGI("InitThread start");
    sleep(5);

    g_libGTASA = GetLibBase("libGTASA.so");

    const char* paths[] = {
        "libEGL.so",
        "/system/lib64/libEGL.so",
        "/vendor/lib64/libEGL.so",
        nullptr
    };
    void* libEGL = nullptr;
    for (int i = 0; paths[i]; i++) {
        libEGL = dlopen(paths[i], RTLD_NOW|RTLD_GLOBAL);
        if (libEGL) { LOGI("libEGL: %s", paths[i]); break; }
    }
    if (!libEGL) { LOGE("libEGL no encontrada"); return nullptr; }

    void* fn = dlsym(libEGL, "eglSwapBuffers");
    if (!fn)  { LOGE("eglSwapBuffers no encontrado"); return nullptr; }

    real_eglSwapBuffers = InstallHook(fn, (void*)hk_eglSwapBuffers);
    if (real_eglSwapBuffers)
        LOGI("Hook instalado OK");
    else
        LOGE("Hook fallo");

    return nullptr;
}

__attribute__((constructor))
static void OnLoad() {
    LOGI("=== CheatMenu SA v2.10 cargando ===");
    pthread_t tid;
    pthread_create(&tid, nullptr, InitThread, nullptr);
    pthread_detach(tid);
}

extern "C" __attribute__((visibility("default")))
void AML_ModMain(IAML* aml) {
    LOGI("AML_ModMain");
}

AML_MOD_DEFINE {
    .szGUID        = "com.tuusuario.cheatmenu",
    .szName        = "CheatMenu SA v2.10",
    .szAuthor      = "Tu nombre",
    .szVersion     = "2.10",
    .szDescription = "Cheat menu",
    .fnMain        = AML_ModMain,
    .eType         = AMLModType::Lib,
};
