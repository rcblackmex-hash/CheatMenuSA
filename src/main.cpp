// ============================================================
//  CheatMenu SA v2.10 — main.cpp v6
//  CORREGIDO: usa GOT hook en vez de trampoline roto
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
#include <elf.h>
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

// Busca el puntero de eglSwapBuffers dentro de libGTASA
// y lo reemplaza por el nuestro. Sin copiar instrucciones = sin crash.
static fn_eglSwap InstallGOTHook(uintptr_t base, void* hookFn) {
    auto* ehdr = (Elf64_Ehdr*)base;
    auto* phdr = (Elf64_Phdr*)(base + ehdr->e_phoff);

    uintptr_t dynOff = 0;
    for (int i = 0; i < ehdr->e_phnum; i++)
        if (phdr[i].p_type == PT_DYNAMIC) { dynOff = phdr[i].p_vaddr; break; }
    if (!dynOff) { LOGE("PT_DYNAMIC no encontrado"); return nullptr; }

    auto*    dyn    = (Elf64_Dyn*)(base + dynOff);
    uintptr_t symtab = 0, strtab = 0, jmprel = 0;
    size_t    pltsz  = 0;

    for (; dyn->d_tag != DT_NULL; dyn++) {
        switch (dyn->d_tag) {
            case DT_SYMTAB:   symtab = base + dyn->d_un.d_ptr; break;
            case DT_STRTAB:   strtab = base + dyn->d_un.d_ptr; break;
            case DT_JMPREL:   jmprel = base + dyn->d_un.d_ptr; break;
            case DT_PLTRELSZ: pltsz  = dyn->d_un.d_val;        break;
        }
    }
    if (!symtab || !strtab || !jmprel) { LOGE("ELF tables no encontradas"); return nullptr; }

    size_t count = pltsz / sizeof(Elf64_Rela);
    auto*  rela  = (Elf64_Rela*)jmprel;
    auto*  syms  = (Elf64_Sym*)symtab;

    for (size_t i = 0; i < count; i++) {
        uint32_t    idx  = ELF64_R_SYM(rela[i].r_info);
        const char* name = (const char*)(strtab + syms[idx].st_name);

        if (strcmp(name, "eglSwapBuffers") == 0) {
            auto* slot    = (fn_eglSwap*)(base + rela[i].r_offset);
            fn_eglSwap og = *slot; // guardamos el puntero original

            uintptr_t page = (uintptr_t)slot & ~0xFFFULL;
            mprotect((void*)page, 0x2000, PROT_READ|PROT_WRITE);
            *slot = (fn_eglSwap)hookFn; // reemplazamos el puntero
            mprotect((void*)page, 0x2000, PROT_READ);

            LOGI("GOT hook OK → slot=%p original=%p", slot, og);
            return og;
        }
    }

    LOGE("eglSwapBuffers no encontrado en GOT");
    return nullptr;
}

// ── Render ────────────────────────────────────────────────────
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

    // Boton CM flotante
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

    // Menu principal
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
    sleep(20);

    g_libGTASA = GetLibBase("libGTASA.so");
    if (!g_libGTASA) { LOGE("libGTASA no encontrada"); return nullptr; }

    // Hook via GOT — sin trampoline, sin crash
    real_eglSwapBuffers = InstallGOTHook(g_libGTASA, (void*)hk_eglSwapBuffers);
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
