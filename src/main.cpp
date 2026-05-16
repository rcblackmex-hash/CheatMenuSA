// ============================================================
//  CheatMenu SA v2.10 — Tu mod 100% tuyo
//  main.cpp — Punto de entrada AML + Hooks + Menú ImGui
//
//  Autor: Tú (basado en AML framework by RusJJ)
//  Requiere: Android NDK r25+, AML SDK, ImGui 1.91+
// ============================================================

#include <jni.h>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/input.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cmath>

// AML — Android Mod Loader (por RusJJ)
#include <AML/AML.h>
#include <AML/ARMHook.h>
#include <AML/MemoryMgr.h>

// ImGui
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"

#include "game.h"

// ── Versión del mod ──────────────────────────────────────────
#define MOD_NAME    "CheatMenu SA"
#define MOD_VERSION "2.10"
#define MOD_GUID    "com.tuusuario.cheatmenu"

// ── Estado del menú ───────────────────────────────────────────
static bool  g_menuVisible    = false;
static bool  g_imguiReady     = false;
static int   g_currentTab     = 0;
static float g_menuPosX       = 50.0f;
static float g_menuPosY       = 50.0f;

// ── Estado de cheats activos ──────────────────────────────────
static bool  ch_godMode       = false;
static bool  ch_neverWanted   = false;
static bool  ch_infiniteAmmo  = false;
static bool  ch_noClip        = false;
static bool  ch_freezeTime    = false;
static int   ch_wantedLevel   = 0;
static float ch_moveSpeed     = 1.0f;
static int   ch_spawnModel    = 411;   // Infernus por defecto
static int   ch_weatherIdx    = 0;
static int   ch_timeHours     = 12;
static int   ch_timeMinutes   = 0;

// ── Punteros a funciones del juego ───────────────────────────
static fn_GiveWeapon  pfn_GiveWeapon  = nullptr;
static fn_SetWanted   pfn_SetWanted   = nullptr;
static fn_CreateCar   pfn_CreateCar   = nullptr;
static fn_FixCar      pfn_FixCar      = nullptr;
static fn_LoadModels  pfn_LoadModels  = nullptr;
static fn_ReqModel    pfn_ReqModel    = nullptr;

// ── Hook de eglSwapBuffers ────────────────────────────────────
using fn_eglSwapBuffers = EGLBoolean(*)(EGLDisplay, EGLSurface);
static fn_eglSwapBuffers orig_eglSwapBuffers = nullptr;
static EGLDisplay g_eglDisplay = EGL_NO_DISPLAY;
static EGLSurface g_eglSurface = EGL_NO_SURFACE;
static EGLContext g_eglContext = EGL_NO_CONTEXT;
static int g_screenW = 1080, g_screenH = 2400; // Honor 200 default

// ── Hook de touch ─────────────────────────────────────────────
using fn_ProcessEvents = void(*)(void*);
static fn_ProcessEvents orig_ProcessEvents = nullptr;

// ── Macros de ayuda ───────────────────────────────────────────
#define CALL_FUNC(fn, ...) if(fn) fn(__VA_ARGS__)
#define READ_PTR(addr) (*reinterpret_cast<uintptr_t*>(addr))

// =============================================================
//  FUNCIONES DE CHEATS
// =============================================================

// Dar salud y armadura
static void SetPlayerHealth(float hp) {
    auto* ped = GetLocalPed();
    if (!ped) return;
    *reinterpret_cast<float*>((uintptr_t)ped + Off::PED_HEALTH) = hp;
}

static void SetPlayerArmor(float armor) {
    auto* ped = GetLocalPed();
    if (!ped) return;
    *reinterpret_cast<float*>((uintptr_t)ped + Off::PED_ARMOR) = armor;
}

static void SetPlayerMoney(int amount) {
    auto* player = GetLocalPlayer();
    if (!player || !player->pPlayerInfo) return;
    *reinterpret_cast<int*>((uintptr_t)player->pPlayerInfo + Off::PED_MONEY) = amount;
}

static float GetPlayerX() {
    auto* ped = GetLocalPed();
    if (!ped || !ped->pMatrix) return 0.0f;
    return *reinterpret_cast<float*>((uintptr_t)ped->pMatrix + Off::PED_MATRIX_POS_X);
}

static float GetPlayerY() {
    auto* ped = GetLocalPed();
    if (!ped || !ped->pMatrix) return 0.0f;
    return *reinterpret_cast<float*>((uintptr_t)ped->pMatrix + Off::PED_MATRIX_POS_Y);
}

static float GetPlayerZ() {
    auto* ped = GetLocalPed();
    if (!ped || !ped->pMatrix) return 0.0f;
    return *reinterpret_cast<float*>((uintptr_t)ped->pMatrix + Off::PED_MATRIX_POS_Z);
}

static void TeleportPlayerTo(float x, float y, float z) {
    auto* ped = GetLocalPed();
    if (!ped || !ped->pMatrix) return;
    *reinterpret_cast<float*>((uintptr_t)ped->pMatrix + Off::PED_MATRIX_POS_X) = x;
    *reinterpret_cast<float*>((uintptr_t)ped->pMatrix + Off::PED_MATRIX_POS_Y) = y;
    *reinterpret_cast<float*>((uintptr_t)ped->pMatrix + Off::PED_MATRIX_POS_Z) = z;
}

static void GiveAllWeapons() {
    auto* ped = GetLocalPed();
    if (!ped || !pfn_GiveWeapon) return;
    for (int i = 0; Weapons[i].name != nullptr; i++) {
        pfn_GiveWeapon(ped, Weapons[i].id, 9999, true);
    }
}

static void GiveWeaponById(int id) {
    auto* ped = GetLocalPed();
    if (!ped || !pfn_GiveWeapon) return;
    pfn_GiveWeapon(ped, id, 9999, true);
}

static void SetWanted(int level) {
    auto* player = GetLocalPlayer();
    if (!player || !pfn_SetWanted) return;
    pfn_SetWanted(player, level);
    ch_wantedLevel = level;
}

static void SpawnVehicle(int model) {
    if (!pfn_CreateCar || !pfn_ReqModel || !pfn_LoadModels) return;
    float x = GetPlayerX() + 3.0f;
    float y = GetPlayerY();
    float z = GetPlayerZ() + 1.0f;
    pfn_ReqModel(model, 2);
    pfn_LoadModels(true);
    pfn_CreateCar(model, x, y, z, true);
}

static void SetWeather(int idx) {
    if (!g_libGTASA) return;
    *reinterpret_cast<int16_t*>(ADDR(Off::CWeather_OldWeather)) = (int16_t)idx;
    *reinterpret_cast<int16_t*>(ADDR(Off::CWeather_NewWeather)) = (int16_t)idx;
}

static void SetTime(int hours, int minutes) {
    if (!g_libGTASA) return;
    *reinterpret_cast<uint8_t*>(ADDR(Off::CClock_GameHours))   = (uint8_t)hours;
    *reinterpret_cast<uint8_t*>(ADDR(Off::CClock_GameMinutes)) = (uint8_t)minutes;
}

// Aplicar god mode (se llama cada frame cuando está activo)
static void TickCheats() {
    if (ch_godMode) {
        SetPlayerHealth(200.0f);
        SetPlayerArmor(200.0f);
    }
    if (ch_neverWanted) {
        SetWanted(0);
    }
    if (ch_freezeTime) {
        SetTime(ch_timeHours, ch_timeMinutes);
    }
}

// =============================================================
//  RENDERIZADO IMGUI — TABS DEL MENÚ
// =============================================================

static void RenderTabTeleport() {
    ImGui::Text("Elige un lugar:");
    ImGui::Separator();

    static int selected = -1;
    ImGui::BeginChild("TeleportList", ImVec2(0, 300), true);
    for (int i = 0; Locations[i].name != nullptr; i++) {
        bool isSelected = (selected == i);
        if (ImGui::Selectable(Locations[i].name, isSelected)) {
            selected = i;
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();
    if (selected >= 0) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "→ %s", Locations[selected].name);
        if (ImGui::Button("¡Teleportar!", ImVec2(-1, 0))) {
            TeleportPlayerTo(Locations[selected].x,
                             Locations[selected].y,
                             Locations[selected].z);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Posición actual:");
    ImGui::Text("X: %.2f  Y: %.2f  Z: %.2f",
                GetPlayerX(), GetPlayerY(), GetPlayerZ());
}

static void RenderTabPlayer() {
    // God Mode
    if (ImGui::Checkbox("God Mode (HP+Armor 200)", &ch_godMode)) {}

    // Never Wanted
    if (ImGui::Checkbox("Sin busqueda policial", &ch_neverWanted)) {
        if (!ch_neverWanted) SetWanted(0);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Nivel de busqueda:");
    if (ImGui::SliderInt("##wanted", &ch_wantedLevel, 0, 6)) {
        SetWanted(ch_wantedLevel);
    }

    ImGui::Spacing();
    ImGui::Separator();
    // HP manual
    static float hp = 100.0f;
    ImGui::Text("Salud:");
    ImGui::SliderFloat("##health", &hp, 0.0f, 200.0f);
    if (ImGui::Button("Aplicar salud")) SetPlayerHealth(hp);
    ImGui::SameLine();
    // Armor manual
    static float armor = 100.0f;
    ImGui::Text("  Armadura:");
    ImGui::SliderFloat("##armor", &armor, 0.0f, 200.0f);
    if (ImGui::Button("Aplicar armadura")) SetPlayerArmor(armor);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Dinero:");
    static int money = 99999999;
    ImGui::InputInt("##money", &money, 1000, 100000);
    if (ImGui::Button("Dar dinero")) SetPlayerMoney(money);
    if (ImGui::Button("Dar $99,999,999")) SetPlayerMoney(99999999);
}

static void RenderTabVehicle() {
    ImGui::Text("Spawnear vehículo:");

    // Filtro de búsqueda
    static char filter[64] = "";
    ImGui::InputText("Buscar", filter, sizeof(filter));

    ImGui::BeginChild("VehList", ImVec2(0, 250), true);
    static int selectedVeh = -1;
    for (int i = 0; Vehicles[i].name != nullptr; i++) {
        // Filtro
        if (filter[0] != '\0') {
            std::string name(Vehicles[i].name);
            std::string flt(filter);
            // case-insensitive simple
            bool match = false;
            for (size_t j = 0; j + flt.size() <= name.size(); j++) {
                bool ok = true;
                for (size_t k = 0; k < flt.size(); k++) {
                    if (tolower(name[j+k]) != tolower(flt[k])) { ok = false; break; }
                }
                if (ok) { match = true; break; }
            }
            if (!match) continue;
        }

        char label[64];
        snprintf(label, sizeof(label), "[%d] %s", Vehicles[i].id, Vehicles[i].name);
        bool sel = (selectedVeh == i);
        if (ImGui::Selectable(label, sel)) selectedVeh = i;
    }
    ImGui::EndChild();

    if (selectedVeh >= 0) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
                           "→ %s", Vehicles[selectedVeh].name);
        if (ImGui::Button("Spawnear", ImVec2(-1, 0))) {
            SpawnVehicle(Vehicles[selectedVeh].id);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    // Reparar/Voltear el vehículo actual
    if (ImGui::Button("Reparar vehículo actual", ImVec2(-1, 0))) {
        // TODO: obtener el vehículo en que está el jugador y llamar a pfn_FixCar
        LOGI("Fix car requested");
    }
    if (ImGui::Button("Voltear vehículo", ImVec2(-1, 0))) {
        LOGI("Flip car requested");
    }
}

static void RenderTabWeapon() {
    if (ImGui::Button("Dar TODAS las armas (9999 balas)", ImVec2(-1, 0))) {
        GiveAllWeapons();
    }

    ImGui::Checkbox("Munición infinita (en desarrollo)", &ch_infiniteAmmo);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Dar arma específica:");

    static int selectedWeapon = 0;
    ImGui::BeginChild("WeaponList", ImVec2(0, 280), true);
    for (int i = 0; Weapons[i].name != nullptr; i++) {
        char label[64];
        snprintf(label, sizeof(label), "[%d] %s", Weapons[i].id, Weapons[i].name);
        bool sel = (selectedWeapon == i);
        if (ImGui::Selectable(label, sel)) selectedWeapon = i;
    }
    ImGui::EndChild();

    if (ImGui::Button("Dar arma seleccionada", ImVec2(-1, 0))) {
        GiveWeaponById(Weapons[selectedWeapon].id);
    }
}

static void RenderTabGame() {
    ImGui::Text("Clima:");
    if (ImGui::Combo("##weather", &ch_weatherIdx, WeatherNames, 20)) {
        SetWeather(ch_weatherIdx);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Hora del juego:");
    ImGui::SliderInt("Horas##h", &ch_timeHours, 0, 23);
    ImGui::SliderInt("Minutos##m", &ch_timeMinutes, 0, 59);

    if (ImGui::Button("Aplicar hora")) {
        SetTime(ch_timeHours, ch_timeMinutes);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Congelar hora", &ch_freezeTime)) {}

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Atajos de hora:");
    if (ImGui::Button("Amanecer (6:00)")) { ch_timeHours=6; ch_timeMinutes=0; SetTime(6,0); }
    ImGui::SameLine();
    if (ImGui::Button("Mediodía (12:00)")) { ch_timeHours=12; ch_timeMinutes=0; SetTime(12,0); }
    ImGui::SameLine();
    if (ImGui::Button("Noche (22:00)")) { ch_timeHours=22; ch_timeMinutes=0; SetTime(22,0); }
}

static void RenderTabAbout() {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%s v%s", MOD_NAME, MOD_VERSION);
    ImGui::Separator();
    ImGui::Text("Tu mod 100%% propio para GTA SA Android");
    ImGui::Spacing();
    ImGui::Text("Motor:   ImGui 1.91.9");
    ImGui::Text("Framework: AML (RusJJ)");
    ImGui::Text("Arquitectura: arm64-v8a");
    ImGui::Text("Target: GTA SA v2.10");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Tabs activos:");
    ImGui::BulletText("Teleport — %d ubicaciones", (int)(sizeof(Locations)/sizeof(Locations[0]))-1);
    ImGui::BulletText("Player   — HP, Armor, Dinero, Busqueda");
    ImGui::BulletText("Vehículo — %d modelos", (int)(sizeof(Vehicles)/sizeof(Vehicles[0]))-1);
    ImGui::BulletText("Armas    — %d armas", (int)(sizeof(Weapons)/sizeof(Weapons[0]))-1);
    ImGui::BulletText("Juego    — Clima, Hora");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Abrir/Cerrar menu: mantén 2 dedos 1s");
}

// =============================================================
//  RENDER PRINCIPAL DEL MENÚ
// =============================================================

static void RenderMenu() {
    if (!g_menuVisible) return;

    // Tema oscuro tipo ARM
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 6.0f;
    style.FrameRounding     = 4.0f;
    style.ItemSpacing       = ImVec2(8, 6);
    style.WindowPadding     = ImVec2(10, 10);
    style.Colors[ImGuiCol_WindowBg]    = ImVec4(0.08f, 0.12f, 0.20f, 0.95f);
    style.Colors[ImGuiCol_TitleBg]     = ImVec4(0.10f, 0.20f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]= ImVec4(0.15f, 0.30f, 0.55f, 1.00f);
    style.Colors[ImGuiCol_Tab]         = ImVec4(0.10f, 0.20f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_TabActive]   = ImVec4(0.20f, 0.45f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_Button]      = ImVec4(0.15f, 0.30f, 0.55f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered]= ImVec4(0.25f, 0.45f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_Header]      = ImVec4(0.20f, 0.40f, 0.65f, 0.80f);
    style.Colors[ImGuiCol_CheckMark]   = ImVec4(0.30f, 0.80f, 1.00f, 1.00f);

    // Escalar para pantalla del Honor 200 (2400x1080 landscape = 2400 ancho)
    float scale = (float)g_screenW / 1920.0f;
    if (scale < 1.0f) scale = 1.0f;
    ImGui::GetIO().FontGlobalScale = scale * 0.9f;

    ImGui::SetNextWindowPos(ImVec2(g_menuPosX, g_menuPosY), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(420 * scale, 550 * scale), ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);

    char title[64];
    snprintf(title, sizeof(title), "%s v%s", MOD_NAME, MOD_VERSION);

    ImGui::Begin(title, &g_menuVisible,
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoScrollbar);

    if (ImGui::BeginTabBar("MainTabs")) {
        if (ImGui::BeginTabItem("Teleport"))  { RenderTabTeleport(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Player"))    { RenderTabPlayer();   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Vehiculo"))  { RenderTabVehicle();  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Armas"))     { RenderTabWeapon();   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Juego"))     { RenderTabGame();     ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Acerca"))    { RenderTabAbout();    ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    // Guardar posición para próxima vez
    ImVec2 pos = ImGui::GetWindowPos();
    g_menuPosX = pos.x;
    g_menuPosY = pos.y;

    ImGui::End();

    // Aplicar cheats activos cada frame
    TickCheats();
}

// =============================================================
//  HOOK eglSwapBuffers — Aquí renderizamos el menú
// =============================================================

static EGLBoolean hk_eglSwapBuffers(EGLDisplay display, EGLSurface surface) {
    // Inicializar ImGui la primera vez
    if (!g_imguiReady) {
        g_eglDisplay = display;
        g_eglSurface = surface;
        g_eglContext = eglGetCurrentContext();

        // Obtener tamaño de pantalla
        eglQuerySurface(display, surface, EGL_WIDTH,  &g_screenW);
        eglQuerySurface(display, surface, EGL_HEIGHT, &g_screenH);
        LOGI("Screen: %dx%d", g_screenW, g_screenH);

        // Inicializar ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)g_screenW, (float)g_screenH);
        io.IniFilename = nullptr; // No queremos guardar imgui.ini

        // Cargar fuente si existe
        const char* fontPath = "/storage/emulated/0/Android_unprotected/"
                               "data/com.rockstargames.gtasa/files/ARM/CheatMenu/Font/MyFont.ttf";
        float fontSize = (float)g_screenH * 0.022f;
        if (access(fontPath, R_OK) == 0) {
            io.Fonts->AddFontFromFileTTF(fontPath, fontSize);
        } else {
            io.Fonts->AddFontDefault();
        }
        io.Fonts->Build();

        ImGui_ImplOpenGL3_Init("#version 300 es");
        ImGui::StyleColorsDark();

        g_imguiReady = true;
        LOGI("ImGui inicializado OK");
    }

    // Actualizar tamaño en caso de rotación
    eglQuerySurface(display, surface, EGL_WIDTH,  &g_screenW);
    eglQuerySurface(display, surface, EGL_HEIGHT, &g_screenH);
    ImGui::GetIO().DisplaySize = ImVec2((float)g_screenW, (float)g_screenH);

    // Nuevo frame de ImGui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // Renderizar nuestro menú
    RenderMenu();

    // Renderizar ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Llamar al original para que el juego siga renderizando
    return orig_eglSwapBuffers(display, surface);
}

// =============================================================
//  HOOK DE TOUCH — Detectar gesto para abrir/cerrar menú
// =============================================================
// Detecta mantener 2 dedos por ~1 segundo para toggle del menú

static int   g_touchCount      = 0;
static float g_touchHoldTime   = 0.0f;
static bool  g_toggleTriggered = false;

// Esta función se llama antes de que el juego procese el input
// Necesita ser hookeada en la función de input de GTA SA
// Por ahora usamos un thread separado como fallback

static void* TouchWatcherThread(void*) {
    // Método simple: monitorear /proc/self/fd o usar AInput
    // Esta es una implementación básica — mejorar con hook de AInputQueue
    while (true) {
        usleep(16000); // ~60fps
    }
    return nullptr;
}

// Alternativa: botón flotante visible siempre (más simple y confiable)
// Se puede activar desde game.h cambiando USE_FLOAT_BUTTON a 1

// =============================================================
//  PUNTO DE ENTRADA AML
// =============================================================

extern "C" __attribute__((visibility("default")))
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    return JNI_VERSION_1_6;
}

// Esta es la función que AML llama al cargar el mod
extern "C" __attribute__((visibility("default")))
void MOD_MAIN(IAML* aml) {
    LOGI("=== %s v%s cargando... ===", MOD_NAME, MOD_VERSION);

    // 1. Obtener base de libGTASA.so
    g_libGTASA = aml->GetLib("libGTASA.so");
    if (!g_libGTASA) {
        LOGE("ERROR: No se pudo obtener libGTASA.so");
        return;
    }
    LOGI("libGTASA.so base: 0x%lX", g_libGTASA);

    // 2. Obtener punteros a funciones del juego
    pfn_GiveWeapon = reinterpret_cast<fn_GiveWeapon>(ADDR(Off::GiveWeapon));
    pfn_SetWanted  = reinterpret_cast<fn_SetWanted>(ADDR(Off::SetWantedLevel));
    pfn_CreateCar  = reinterpret_cast<fn_CreateCar>(ADDR(Off::CreateCar));
    pfn_FixCar     = reinterpret_cast<fn_FixCar>(ADDR(Off::FixCar));
    pfn_LoadModels = reinterpret_cast<fn_LoadModels>(ADDR(Off::LoadAllModels));
    pfn_ReqModel   = reinterpret_cast<fn_ReqModel>(ADDR(Off::RequestModel));
    LOGI("Punteros de funciones OK");

    // 3. Hookear eglSwapBuffers (para el render del menú)
    void* libEGL = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
    if (libEGL) {
        void* eglSwap = dlsym(libEGL, "eglSwapBuffers");
        if (eglSwap) {
            aml->Hook(reinterpret_cast<uintptr_t>(eglSwap),
                      reinterpret_cast<void*>(hk_eglSwapBuffers),
                      reinterpret_cast<void**>(&orig_eglSwapBuffers));
            LOGI("Hook eglSwapBuffers OK");
        } else {
            LOGE("ERROR: No se encontró eglSwapBuffers");
        }
    }

    // 4. Iniciar thread de watcher de touch (método de respaldo)
    pthread_t tid;
    pthread_create(&tid, nullptr, TouchWatcherThread, nullptr);
    pthread_detach(tid);

    LOGI("=== %s cargado correctamente ===", MOD_NAME);
}

// Registro del mod para AML
// Este bloque declara el GUID y metadatos del mod
AML_MOD_DEFINE {
    .szGUID     = MOD_GUID,
    .szName     = MOD_NAME " v" MOD_VERSION,
    .szAuthor   = "Tu nombre",
    .szVersion  = MOD_VERSION,
    .eType      = AMLModType::Lib,
    .fnMain     = MOD_MAIN,
};
