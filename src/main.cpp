// ============================================================
//  CheatMenu SA v2.10 — Fix: hook real + botón flotante
//  main.cpp v2 — Sin depender de AML::Hook()
// ============================================================

#include <jni.h>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <string>
#include <cstdio>
#include <cstring>
#include <cmath>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "game.h"

#define TAG "CheatMenuSA"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ── Estado global ─────────────────────────────────────────────
static bool  g_menuVisible  = false;
static bool  g_imguiReady   = false;
static int   g_screenW      = 2400;
static int   g_screenH      = 1080;

// Botón flotante
static float g_btnX         = 20.0f;
static float g_btnY         = 200.0f;
static bool  g_btnDragging  = false;

// Cheats activos
static bool  ch_godMode      = false;
static bool  ch_neverWanted  = false;
static bool  ch_infiniteAmmo = false;
static int   ch_wantedLevel  = 0;
static int   ch_weatherIdx   = 0;
static int   ch_timeHours    = 12;
static int   ch_timeMinutes  = 0;
static bool  ch_freezeTime   = false;

// Punteros funciones del juego
static fn_GiveWeapon  pfn_GiveWeapon  = nullptr;
static fn_SetWanted   pfn_SetWanted   = nullptr;
static fn_CreateCar   pfn_CreateCar   = nullptr;
static fn_ReqModel    pfn_ReqModel    = nullptr;
static fn_LoadModels  pfn_LoadModels  = nullptr;

// ── Hook arm64 inline ─────────────────────────────────────────
// Trampoline: LDR X16, #8  +  BR X16  +  <dirección 64-bit>
// 16 bytes en total

static uint8_t g_origBytes[16]; // bytes originales de eglSwapBuffers
static void*   g_eglSwapTarget = nullptr;

static void WriteHook(void* target, void* hook) {
    uintptr_t page = (uintptr_t)target & ~(uintptr_t)0xFFF;
    mprotect((void*)page, 0x4000, PROT_READ | PROT_WRITE | PROT_EXEC);

    // Guardar bytes originales (16 bytes)
    memcpy(g_origBytes, target, 16);

    // Escribir trampoline arm64
    uint32_t stub[4];
    stub[0] = 0x58000050; // LDR X16, #8
    stub[1] = 0xD61F0200; // BR  X16
    memcpy(&stub[2], &hook, sizeof(void*)); // dirección de 64 bits

    memcpy(target, stub, 16);
    __builtin___clear_cache((char*)target, (char*)target + 16);
    mprotect((void*)page, 0x4000, PROT_READ | PROT_EXEC);
}

// ── Funciones de cheats ───────────────────────────────────────

static void SetPlayerHealth(float hp) {
    auto* ped = GetLocalPed();
    if (!ped) return;
    *reinterpret_cast<float*>((uintptr_t)ped + Off::PED_HEALTH) = hp;
}
static void SetPlayerArmor(float v) {
    auto* ped = GetLocalPed();
    if (!ped) return;
    *reinterpret_cast<float*>((uintptr_t)ped + Off::PED_ARMOR) = v;
}
static void SetPlayerMoney(int v) {
    auto* p = GetLocalPlayer();
    if (!p || !p->pPlayerInfo) return;
    *reinterpret_cast<int*>((uintptr_t)p->pPlayerInfo + Off::PED_MONEY) = v;
}
static float GetPX() {
    auto* ped = GetLocalPed();
    if (!ped || !ped->pMatrix) return 0;
    return *reinterpret_cast<float*>((uintptr_t)ped->pMatrix + Off::PED_MATRIX_POS_X);
}
static float GetPY() {
    auto* ped = GetLocalPed();
    if (!ped || !ped->pMatrix) return 0;
    return *reinterpret_cast<float*>((uintptr_t)ped->pMatrix + Off::PED_MATRIX_POS_Y);
}
static float GetPZ() {
    auto* ped = GetLocalPed();
    if (!ped || !ped->pMatrix) return 0;
    return *reinterpret_cast<float*>((uintptr_t)ped->pMatrix + Off::PED_MATRIX_POS_Z);
}
static void TeleportTo(float x, float y, float z) {
    auto* ped = GetLocalPed();
    if (!ped || !ped->pMatrix) return;
    *reinterpret_cast<float*>((uintptr_t)ped->pMatrix + Off::PED_MATRIX_POS_X) = x;
    *reinterpret_cast<float*>((uintptr_t)ped->pMatrix + Off::PED_MATRIX_POS_Y) = y;
    *reinterpret_cast<float*>((uintptr_t)ped->pMatrix + Off::PED_MATRIX_POS_Z) = z;
}
static void GiveAllWeapons() {
    auto* ped = GetLocalPed();
    if (!ped || !pfn_GiveWeapon) return;
    for (int i = 0; Weapons[i].name; i++)
        pfn_GiveWeapon(ped, Weapons[i].id, 9999, true);
}
static void SetWanted(int level) {
    auto* p = GetLocalPlayer();
    if (!p || !pfn_SetWanted) return;
    pfn_SetWanted(p, level);
    ch_wantedLevel = level;
}
static void SpawnVehicle(int model) {
    if (!pfn_CreateCar || !pfn_ReqModel || !pfn_LoadModels) return;
    pfn_ReqModel(model, 2);
    pfn_LoadModels(true);
    pfn_CreateCar(GetPX() + 3.0f, GetPY(), GetPZ() + 1.0f, model, true);
}
static void SetWeather(int idx) {
    if (!g_libGTASA) return;
    *reinterpret_cast<int16_t*>(ADDR(Off::CWeather_OldWeather)) = idx;
    *reinterpret_cast<int16_t*>(ADDR(Off::CWeather_NewWeather)) = idx;
}
static void SetTime(int h, int m) {
    if (!g_libGTASA) return;
    *reinterpret_cast<uint8_t*>(ADDR(Off::CClock_GameHours))   = h;
    *reinterpret_cast<uint8_t*>(ADDR(Off::CClock_GameMinutes)) = m;
}
static void TickCheats() {
    if (ch_godMode)     { SetPlayerHealth(200); SetPlayerArmor(200); }
    if (ch_neverWanted)   SetWanted(0);
    if (ch_freezeTime)    SetTime(ch_timeHours, ch_timeMinutes);
}

// ── Tabs del menú ─────────────────────────────────────────────

static void TabTeleport() {
    static int sel = -1;
    ImGui::BeginChild("TL", ImVec2(0, 280), true);
    for (int i = 0; Locations[i].name; i++) {
        if (ImGui::Selectable(Locations[i].name, sel == i)) sel = i;
    }
    ImGui::EndChild();
    if (sel >= 0) {
        ImGui::TextColored({0.4f,1,0.4f,1}, "-> %s", Locations[sel].name);
        if (ImGui::Button("Teleportar!", {-1,0}))
            TeleportTo(Locations[sel].x, Locations[sel].y, Locations[sel].z);
    }
    ImGui::Separator();
    ImGui::Text("Pos: %.1f / %.1f / %.1f", GetPX(), GetPY(), GetPZ());
}

static void TabPlayer() {
    ImGui::Checkbox("God Mode", &ch_godMode);
    ImGui::Checkbox("Sin busqueda", &ch_neverWanted);
    ImGui::Separator();
    ImGui::Text("Nivel busqueda:");
    if (ImGui::SliderInt("##w", &ch_wantedLevel, 0, 6)) SetWanted(ch_wantedLevel);
    ImGui::Separator();
    static float hp = 100, armor = 100;
    ImGui::SliderFloat("Salud##h", &hp, 0, 200);
    if (ImGui::Button("Dar salud")) SetPlayerHealth(hp);
    ImGui::SameLine();
    ImGui::SliderFloat("Armadura##a", &armor, 0, 200);
    if (ImGui::Button("Dar armadura")) SetPlayerArmor(armor);
    ImGui::Separator();
    static int money = 9999999;
    ImGui::InputInt("Dinero", &money, 10000);
    if (ImGui::Button("Dar dinero")) SetPlayerMoney(money);
    if (ImGui::Button("Max dinero $99M")) SetPlayerMoney(99999999);
}

static void TabVehicle() {
    static char filter[32] = "";
    static int  sel = -1;
    ImGui::InputText("Buscar##v", filter, sizeof(filter));
    ImGui::BeginChild("VL", ImVec2(0, 250), true);
    for (int i = 0; Vehicles[i].name; i++) {
        if (filter[0]) {
            bool match = false;
            std::string n(Vehicles[i].name), f(filter);
            for (size_t j = 0; j + f.size() <= n.size(); j++) {
                bool ok = true;
                for (size_t k = 0; k < f.size(); k++)
                    if (tolower(n[j+k]) != tolower(f[k])) { ok=false; break; }
                if (ok) { match=true; break; }
            }
            if (!match) continue;
        }
        char lbl[48]; snprintf(lbl, sizeof(lbl), "[%d] %s", Vehicles[i].id, Vehicles[i].name);
        if (ImGui::Selectable(lbl, sel==i)) sel=i;
    }
    ImGui::EndChild();
    if (sel>=0) {
        ImGui::TextColored({0.4f,1,0.4f,1}, "-> %s", Vehicles[sel].name);
        if (ImGui::Button("Spawnear!", {-1,0})) SpawnVehicle(Vehicles[sel].id);
    }
}

static void TabWeapon() {
    if (ImGui::Button("Dar TODAS (9999 balas)", {-1,0})) GiveAllWeapons();
    ImGui::Separator();
    static int sel = 0;
    ImGui::BeginChild("WL", ImVec2(0, 270), true);
    for (int i = 0; Weapons[i].name; i++) {
        char lbl[48]; snprintf(lbl,sizeof(lbl),"[%d] %s",Weapons[i].id,Weapons[i].name);
        if (ImGui::Selectable(lbl, sel==i)) sel=i;
    }
    ImGui::EndChild();
    if (ImGui::Button("Dar arma", {-1,0})) {
        auto* ped = GetLocalPed();
        if (ped && pfn_GiveWeapon) pfn_GiveWeapon(ped, Weapons[sel].id, 9999, true);
    }
}

static void TabGame() {
    ImGui::Text("Clima:");
    if (ImGui::Combo("##wt", &ch_weatherIdx, WeatherNames, 20)) SetWeather(ch_weatherIdx);
    ImGui::Separator();
    ImGui::SliderInt("Horas##th", &ch_timeHours, 0, 23);
    ImGui::SliderInt("Minutos##tm", &ch_timeMinutes, 0, 59);
    if (ImGui::Button("Aplicar hora")) SetTime(ch_timeHours, ch_timeMinutes);
    ImGui::SameLine();
    ImGui::Checkbox("Congelar", &ch_freezeTime);
    ImGui::Separator();
    if (ImGui::Button("Amanecer")) { ch_timeHours=6;  ch_timeMinutes=0; SetTime(6,0);  }
    ImGui::SameLine();
    if (ImGui::Button("Mediodia")) { ch_timeHours=12; ch_timeMinutes=0; SetTime(12,0); }
    ImGui::SameLine();
    if (ImGui::Button("Noche"))    { ch_timeHours=22; ch_timeMinutes=0; SetTime(22,0); }
}

// ── Render principal ──────────────────────────────────────────

static void ApplyStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 6; s.FrameRounding = 4;
    s.Colors[ImGuiCol_WindowBg]     = {0.08f,0.12f,0.20f,0.96f};
    s.Colors[ImGuiCol_TitleBgActive]= {0.15f,0.30f,0.55f,1.00f};
    s.Colors[ImGuiCol_TabActive]    = {0.20f,0.45f,0.80f,1.00f};
    s.Colors[ImGuiCol_Button]       = {0.15f,0.30f,0.55f,1.00f};
    s.Colors[ImGuiCol_ButtonHovered]= {0.25f,0.45f,0.75f,1.00f};
    s.Colors[ImGuiCol_CheckMark]    = {0.30f,0.80f,1.00f,1.00f};
}

static void RenderFloatingButton() {
    // Botón pequeño siempre visible para abrir/cerrar el menú
    float btnSize = g_screenH * 0.07f;
    ImGui::SetNextWindowPos({g_btnX, g_btnY}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({btnSize, btnSize});
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::Begin("##btn", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoScrollbar  |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_AlwaysAutoResize);

    // Arrastrable
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        g_btnX += delta.x;
        g_btnY += delta.y;
    }

    const char* label = g_menuVisible ? "X" : "CM";
    float lw = ImGui::CalcTextSize(label).x;
    ImGui::SetCursorPosX((btnSize - lw) * 0.5f);
    if (ImGui::Button(label, {btnSize - 16, btnSize - 16}))
        g_menuVisible = !g_menuVisible;

    ImGui::End();
}

static void RenderMenu() {
    ApplyStyle();

    float scale = (float)g_screenW / 1920.0f;
    if (scale < 1.0f) scale = 1.0f;
    ImGui::GetIO().FontGlobalScale = scale * 0.85f;

    // Botón flotante siempre visible
    RenderFloatingButton();

    if (!g_menuVisible) return;

    ImGui::SetNextWindowPos({g_screenW * 0.1f, g_screenH * 0.05f}, ImGuiCond_Once);
    ImGui::SetNextWindowSize({430 * scale, 520 * scale}, ImGuiCond_Once);

    ImGui::Begin("CheatMenu SA v2.10", &g_menuVisible,
        ImGuiWindowFlags_NoScrollbar);

    if (ImGui::BeginTabBar("Tabs")) {
        if (ImGui::BeginTabItem("Teleport")) { TabTeleport(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Player"))   { TabPlayer();   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Vehiculo")) { TabVehicle();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Armas"))    { TabWeapon();   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Juego"))    { TabGame();     ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::End();

    TickCheats();
}

// ── Hook de eglSwapBuffers ────────────────────────────────────

using fn_eglSwap = EGLBoolean(*)(EGLDisplay, EGLSurface);
static fn_eglSwap real_eglSwapBuffers = nullptr;

EGLBoolean hk_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    if (!g_imguiReady) {
        int w = 0, h = 0;
        eglQuerySurface(dpy, surface, EGL_WIDTH,  &w);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
        if (w > 0 && h > 0) {
            g_screenW = w; g_screenH = h;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize  = {(float)g_screenW, (float)g_screenH};
        io.IniFilename  = nullptr;
        io.MouseDrawCursor = false;

        // Fuente más grande para pantalla de teléfono
        float fontSize = g_screenH * 0.025f;
        io.Fonts->AddFontDefault();
        io.Fonts->Build();

        ImGui_ImplOpenGL3_Init("#version 300 es");
        g_imguiReady = true;
        LOGI("ImGui listo! Pantalla: %dx%d", g_screenW, g_screenH);
    }

    // Actualizar tamaño
    ImGui::GetIO().DisplaySize = {(float)g_screenW, (float)g_screenH};

    // Simular input táctil básico (toque simple)
    // ImGui usa MousePos para simular el toque
    // El sistema Android ya debería pasar eventos si usamos imgui_impl_android
    // Por ahora usamos posición fija — mejorar en v2.11
    ImGui::GetIO().DeltaTime = 1.0f / 60.0f;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    RenderMenu();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return real_eglSwapBuffers(dpy, surface);
}

// ── Hilo de inicialización ───────────────────────────────────
// Se ejecuta al cargar la biblioteca (.so)
// No depende de AML para el hook

static void* InitThread(void*) {
    LOGI("InitThread arrancando...");

    // Esperar a que el juego cargue libGTASA.so
    for (int i = 0; i < 60; i++) {
        void* lib = dlopen("libGTASA.so", RTLD_NOLOAD | RTLD_NOW);
        if (lib) {
            g_libGTASA = (uintptr_t)lib;
            dlclose(lib);
            break;
        }
        sleep(1);
    }
    LOGI("libGTASA base: 0x%lX", g_libGTASA);

    // Obtener punteros de funciones
    if (g_libGTASA) {
        pfn_GiveWeapon = (fn_GiveWeapon)(ADDR(Off::GiveWeapon));
        pfn_SetWanted  = (fn_SetWanted) (ADDR(Off::SetWantedLevel));
        pfn_CreateCar  = (fn_CreateCar) (ADDR(Off::CreateCar));
        pfn_ReqModel   = (fn_ReqModel)  (ADDR(Off::RequestModel));
        pfn_LoadModels = (fn_LoadModels)(ADDR(Off::LoadAllModels));
    }

    // Hookear eglSwapBuffers
    void* libEGL = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
    if (!libEGL) libEGL = dlopen("/system/lib64/libEGL.so", RTLD_NOW);
    if (!libEGL) libEGL = dlopen("/system/vendor/lib64/libEGL.so", RTLD_NOW);

    if (libEGL) {
        void* swapAddr = dlsym(libEGL, "eglSwapBuffers");
        if (swapAddr) {
            real_eglSwapBuffers = (fn_eglSwap)swapAddr;
            g_eglSwapTarget = swapAddr;
            WriteHook(swapAddr, (void*)hk_eglSwapBuffers);
            LOGI("eglSwapBuffers hookeado en %p", swapAddr);
        } else {
            LOGE("No se encontro eglSwapBuffers");
        }
    } else {
        LOGE("No se pudo abrir libEGL.so");
    }

    LOGI("Init completo!");
    return nullptr;
}

// ── Constructor — se ejecuta al cargar el .so ─────────────────
__attribute__((constructor))
static void OnLoad() {
    LOGI("=== CheatMenu SA v2.10 cargando ===");
    pthread_t tid;
    pthread_create(&tid, nullptr, InitThread, nullptr);
    pthread_detach(tid);
}

// ── Stub AML (requerido para que AML cargue el .so) ──────────
#include "AML/AML.h"
extern "C" __attribute__((visibility("default")))
void AML_ModMain(IAML* aml) {
    // El hook real ya se instala desde OnLoad() / InitThread()
    // Este stub solo existe para que AML reconozca el mod
    LOGI("AML_ModMain llamado (hook ya instalado)");
}

AML_MOD_DEFINE {
    .szGUID        = "com.tuusuario.cheatmenu",
    .szName        = "CheatMenu SA v2.10",
    .szAuthor      = "Tu nombre",
    .szVersion     = "2.10",
    .szDescription = "Cheat menu 100% propio",
    .fnMain        = AML_ModMain,
    .eType         = AMLModType::Lib,
};
