// ============================================================
//  CheatMenu SA v2.10
//  main.cpp v3 — Fix crash: trampoline real con mmap
// ============================================================

#include "game.h"
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



// ── Estado global ─────────────────────────────────────────────
static bool  g_menuVisible = false;
static bool  g_imguiReady  = false;
static int   g_screenW     = 2400;
static int   g_screenH     = 1080;
static float g_btnX        = 20.0f;
static float g_btnY        = 200.0f;

// Cheats
static bool  ch_godMode     = false;
static bool  ch_neverWanted = false;
static bool  ch_freezeTime  = false;
static int   ch_wantedLevel = 0;
static int   ch_weatherIdx  = 0;
static int   ch_timeHours   = 12;
static int   ch_timeMinutes = 0;

// Punteros del juego
static fn_GiveWeapon  pfn_GiveWeapon  = nullptr;
static fn_SetWanted   pfn_SetWanted   = nullptr;
static fn_CreateCar   pfn_CreateCar   = nullptr;
static fn_ReqModel    pfn_ReqModel    = nullptr;
static fn_LoadModels  pfn_LoadModels  = nullptr;

// ── Trampoline real (fix recursión infinita) ──────────────────
//
// PROBLEMA anterior:
//   real_eglSwapBuffers apuntaba a la dirección ya hookeada
//   → llamarla invocaba nuestra hook otra vez → crash
//
// SOLUCIÓN:
//   1. Copiar los primeros 16 bytes originales a un buffer mmap ejecutable
//   2. Agregar un salto al resto de la función original (orig + 16)
//   3. real_eglSwapBuffers apunta a ese buffer → ejecuta la función real

using fn_eglSwap = EGLBoolean(*)(EGLDisplay, EGLSurface);
static fn_eglSwap  real_eglSwapBuffers = nullptr; // apunta al trampoline
static uint8_t*    g_trampoline        = nullptr; // buffer ejecutable

static fn_eglSwap InstallHook(void* target, void* hookFn) {
    // 1. Allocar página ejecutable para el trampoline
    g_trampoline = (uint8_t*)mmap(
        nullptr, 4096,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
    );
    if (g_trampoline == MAP_FAILED) {
        LOGE("mmap fallo para trampoline");
        return nullptr;
    }

    // 2. Hacer escribible la página de la función original
    uintptr_t page = (uintptr_t)target & ~(uintptr_t)0xFFF;
    mprotect((void*)page, 0x4000, PROT_READ | PROT_WRITE | PROT_EXEC);

    // 3. Copiar 16 bytes originales al trampoline
    memcpy(g_trampoline, target, 16);

    // 4. Al final del trampoline, saltar a target+16 (continuar función real)
    //    arm64: LDR X16, #8  +  BR X16  +  [dirección 64-bit]
    uintptr_t cont = (uintptr_t)target + 16;
    uint32_t* jmp  = (uint32_t*)(g_trampoline + 16);
    jmp[0] = 0x58000050; // LDR X16, #8
    jmp[1] = 0xD61F0200; // BR  X16
    memcpy(&jmp[2], &cont, sizeof(uintptr_t));

    // 5. Flush icache del trampoline
    __builtin___clear_cache((char*)g_trampoline, (char*)g_trampoline + 32);

    // 6. Escribir el hook en la función original
    uint32_t hook[4];
    hook[0] = 0x58000050; // LDR X16, #8
    hook[1] = 0xD61F0200; // BR  X16
    memcpy(&hook[2], &hookFn, sizeof(uintptr_t));
    memcpy(target, hook, 16);
    __builtin___clear_cache((char*)target, (char*)target + 16);

    // 7. Restaurar protección
    mprotect((void*)page, 0x4000, PROT_READ | PROT_EXEC);

    LOGI("Hook instalado: target=%p trampoline=%p cont=0x%lX",
         target, g_trampoline, cont);

    return (fn_eglSwap)g_trampoline; // ← llamar esto ejecuta la función real
}

// ── Cheats ────────────────────────────────────────────────────
static void SetHP(float v)    { auto* p=GetLocalPed(); if(p) *(float*)((uintptr_t)p+Off::PED_HEALTH)=v; }
static void SetArmor(float v) { auto* p=GetLocalPed(); if(p) *(float*)((uintptr_t)p+Off::PED_ARMOR)=v;  }
static void SetMoney(int v)   {
    auto* pl=GetLocalPlayer();
    if(pl&&pl->pPlayerInfo) *(int*)((uintptr_t)pl->pPlayerInfo+Off::PED_MONEY)=v;
}
static float GetPX() { auto*p=GetLocalPed(); return (p&&p->pMatrix)?*(float*)((uintptr_t)p->pMatrix+Off::PED_MATRIX_POS_X):0; }
static float GetPY() { auto*p=GetLocalPed(); return (p&&p->pMatrix)?*(float*)((uintptr_t)p->pMatrix+Off::PED_MATRIX_POS_Y):0; }
static float GetPZ() { auto*p=GetLocalPed(); return (p&&p->pMatrix)?*(float*)((uintptr_t)p->pMatrix+Off::PED_MATRIX_POS_Z):0; }
static void TeleportTo(float x,float y,float z) {
    auto*p=GetLocalPed();
    if(!p||!p->pMatrix) return;
    *(float*)((uintptr_t)p->pMatrix+Off::PED_MATRIX_POS_X)=x;
    *(float*)((uintptr_t)p->pMatrix+Off::PED_MATRIX_POS_Y)=y;
    *(float*)((uintptr_t)p->pMatrix+Off::PED_MATRIX_POS_Z)=z;
}
static void GiveAllWeapons() {
    auto*p=GetLocalPed();
    if(!p||!pfn_GiveWeapon) return;
    for(int i=0;Weapons[i].name;i++) pfn_GiveWeapon(p,Weapons[i].id,9999,true);
}
static void SetWanted(int lv) {
    auto*pl=GetLocalPlayer();
    if(!pl||!pfn_SetWanted) return;
    pfn_SetWanted(pl->pPlayerPed, lv);  // CPlayerPed* — BIEN
    ch_wantedLevel=lv;
}
static void SpawnVeh(int model) {
    if(!pfn_CreateCar||!pfn_ReqModel||!pfn_LoadModels) return;
    pfn_ReqModel(model,2);
    pfn_LoadModels(true);
    pfn_CreateCar(GetPX()+3,GetPY(),GetPZ()+1,model,true);
}
static void SetWeather(int i) {
    if(!g_libGTASA) return;
    *(int16_t*)ADDR(Off::CWeather_OldWeather)=(int16_t)i;
    *(int16_t*)ADDR(Off::CWeather_NewWeather)=(int16_t)i;
}
static void SetTime(int h,int m) {
    if(!g_libGTASA) return;
    *(uint8_t*)ADDR(Off::CClock_GameHours)=(uint8_t)h;
    *(uint8_t*)ADDR(Off::CClock_GameMinutes)=(uint8_t)m;
}
static void TickCheats() {
    if(ch_godMode)    { SetHP(200); SetArmor(200); }
    if(ch_neverWanted)  SetWanted(0);
    if(ch_freezeTime)   SetTime(ch_timeHours,ch_timeMinutes);
}

// ── Tabs ──────────────────────────────────────────────────────
static void TabTeleport() {
    static int sel=-1;
    ImGui::BeginChild("TL",ImVec2(0,270),true);
    for(int i=0;Locations[i].name;i++)
        if(ImGui::Selectable(Locations[i].name,sel==i)) sel=i;
    ImGui::EndChild();
    if(sel>=0) {
        ImGui::TextColored({0.4f,1,0.4f,1},"-> %s",Locations[sel].name);
        if(ImGui::Button("Teleportar!",{-1,0}))
            TeleportTo(Locations[sel].x,Locations[sel].y,Locations[sel].z);
    }
    ImGui::Separator();
    ImGui::Text("Pos: %.1f / %.1f / %.1f",GetPX(),GetPY(),GetPZ());
}
static void TabPlayer() {
    ImGui::Checkbox("God Mode",&ch_godMode);
    ImGui::Checkbox("Sin busqueda",&ch_neverWanted);
    ImGui::Separator();
    if(ImGui::SliderInt("Busqueda",&ch_wantedLevel,0,6)) SetWanted(ch_wantedLevel);
    ImGui::Separator();
    static float hp=100,ar=100;
    ImGui::SliderFloat("Salud",&hp,0,200);
    if(ImGui::Button("Aplicar salud")) SetHP(hp);
    ImGui::SliderFloat("Armadura",&ar,0,200);
    if(ImGui::Button("Aplicar armadura")) SetArmor(ar);
    ImGui::Separator();
    static int money=9999999;
    ImGui::InputInt("Dinero",&money,10000);
    if(ImGui::Button("Dar")) SetMoney(money);
    ImGui::SameLine();
    if(ImGui::Button("Max $99M")) SetMoney(99999999);
}
static void TabVehicle() {
    static char flt[32]=""; static int sel=-1;
    ImGui::InputText("Buscar##v",flt,sizeof(flt));
    ImGui::BeginChild("VL",ImVec2(0,240),true);
    for(int i=0;Vehicles[i].name;i++){
        if(flt[0]){
            bool ok=false; std::string n(Vehicles[i].name),f(flt);
            for(size_t j=0;j+f.size()<=n.size();j++){
                bool m=true;
                for(size_t k=0;k<f.size();k++) if(tolower(n[j+k])!=tolower(f[k])){m=false;break;}
                if(m){ok=true;break;}
            }
            if(!ok) continue;
        }
        char lb[48]; snprintf(lb,sizeof(lb),"[%d] %s",Vehicles[i].id,Vehicles[i].name);
        if(ImGui::Selectable(lb,sel==i)) sel=i;
    }
    ImGui::EndChild();
    if(sel>=0){
        ImGui::TextColored({0.4f,1,0.4f,1},"-> %s",Vehicles[sel].name);
        if(ImGui::Button("Spawnear!",{-1,0})) SpawnVeh(Vehicles[sel].id);
    }
}
static void TabWeapon() {
    if(ImGui::Button("Dar TODAS las armas",{-1,0})) GiveAllWeapons();
    ImGui::Separator();
    static int sel=0;
    ImGui::BeginChild("WL",ImVec2(0,260),true);
    for(int i=0;Weapons[i].name;i++){
        char lb[48]; snprintf(lb,sizeof(lb),"[%d] %s",Weapons[i].id,Weapons[i].name);
        if(ImGui::Selectable(lb,sel==i)) sel=i;
    }
    ImGui::EndChild();
    if(ImGui::Button("Dar arma seleccionada",{-1,0})){
        auto*p=GetLocalPed();
        if(p&&pfn_GiveWeapon) pfn_GiveWeapon(p,Weapons[sel].id,9999,true);
    }
}
static void TabGame() {
    if(ImGui::Combo("Clima",&ch_weatherIdx,WeatherNames,20)) SetWeather(ch_weatherIdx);
    ImGui::Separator();
    ImGui::SliderInt("Horas",&ch_timeHours,0,23);
    ImGui::SliderInt("Minutos",&ch_timeMinutes,0,59);
    if(ImGui::Button("Aplicar hora")) SetTime(ch_timeHours,ch_timeMinutes);
    ImGui::SameLine();
    ImGui::Checkbox("Congelar",&ch_freezeTime);
    ImGui::Separator();
    if(ImGui::Button("Amanecer")) { ch_timeHours=6;  SetTime(6,0);  }
    ImGui::SameLine();
    if(ImGui::Button("Mediodia")) { ch_timeHours=12; SetTime(12,0); }
    ImGui::SameLine();
    if(ImGui::Button("Noche"))    { ch_timeHours=22; SetTime(22,0); }
}

// ── Render ────────────────────────────────────────────────────
static void ApplyStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle&s=ImGui::GetStyle();
    s.WindowRounding=6; s.FrameRounding=4;
    s.Colors[ImGuiCol_WindowBg]     ={0.08f,0.12f,0.20f,0.96f};
    s.Colors[ImGuiCol_TitleBgActive]={0.15f,0.30f,0.55f,1.00f};
    s.Colors[ImGuiCol_TabActive]    ={0.20f,0.45f,0.80f,1.00f};
    s.Colors[ImGuiCol_Button]       ={0.15f,0.30f,0.55f,1.00f};
    s.Colors[ImGuiCol_ButtonHovered]={0.25f,0.45f,0.75f,1.00f};
    s.Colors[ImGuiCol_CheckMark]    ={0.30f,0.80f,1.00f,1.00f};
}

static void RenderFloatingButton() {
    float sz=g_screenH*0.07f;
    ImGui::SetNextWindowPos({g_btnX,g_btnY},ImGuiCond_Always);
    ImGui::SetNextWindowSize({sz,sz});
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::Begin("##btn",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoScrollbar|
        ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoBringToFrontOnFocus);
    if(ImGui::IsWindowHovered()&&ImGui::IsMouseDragging(0)){
        ImVec2 d=ImGui::GetIO().MouseDelta;
        g_btnX+=d.x; g_btnY+=d.y;
    }
    const char* lbl=g_menuVisible?"X":"CM";
    float lw=ImGui::CalcTextSize(lbl).x;
    ImGui::SetCursorPosX((sz-lw)*0.5f);
    if(ImGui::Button(lbl,{sz-16,sz-16})) g_menuVisible=!g_menuVisible;
    ImGui::End();
}

static void RenderMenu() {
    ApplyStyle();
    float sc=(float)g_screenW/1920.0f;
    if(sc<1) sc=1;
    ImGui::GetIO().FontGlobalScale=sc*0.85f;

    RenderFloatingButton();

    if(!g_menuVisible) return;

    ImGui::SetNextWindowPos({g_screenW*0.1f,g_screenH*0.05f},ImGuiCond_Once);
    ImGui::SetNextWindowSize({420*sc,500*sc},ImGuiCond_Once);
    ImGui::Begin("CheatMenu SA v2.10",&g_menuVisible,ImGuiWindowFlags_NoScrollbar);
    if(ImGui::BeginTabBar("T")){
        if(ImGui::BeginTabItem("Teleport")) { TabTeleport(); ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem("Player"))   { TabPlayer();   ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem("Vehiculo")) { TabVehicle();  ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem("Armas"))    { TabWeapon();   ImGui::EndTabItem(); }
        if(ImGui::BeginTabItem("Juego"))    { TabGame();     ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::End();
    TickCheats();
}

// ── Hook de eglSwapBuffers ────────────────────────────────────
EGLBoolean hk_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    if(!g_imguiReady) {
        int w=0,h=0;
        eglQuerySurface(dpy,surface,EGL_WIDTH,&w);
        eglQuerySurface(dpy,surface,EGL_HEIGHT,&h);
        if(w>0&&h>0){ g_screenW=w; g_screenH=h; }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO&io=ImGui::GetIO();
        io.DisplaySize={( float)g_screenW,(float)g_screenH};
        io.IniFilename=nullptr;
        io.Fonts->AddFontDefault();
        io.Fonts->Build();
        ImGui_ImplOpenGL3_Init("#version 300 es");
        g_imguiReady=true;
        LOGI("ImGui OK — pantalla %dx%d",g_screenW,g_screenH);
    }

    ImGui::GetIO().DisplaySize={(float)g_screenW,(float)g_screenH};
    ImGui::GetIO().DeltaTime=1.0f/60.0f;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    RenderMenu();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Llamar la función REAL a través del trampoline (sin recursión)
    return real_eglSwapBuffers(dpy, surface);
}

// ── Hilo de init ──────────────────────────────────────────────
static void* InitThread(void*) {
    LOGI("InitThread iniciando...");
    sleep(3); // Dar tiempo al juego de cargar

    // Obtener base de libGTASA
    void* lib = dlopen("libGTASA.so", RTLD_NOLOAD|RTLD_NOW);
    if(lib){ g_libGTASA=(uintptr_t)lib; dlclose(lib); }
    LOGI("libGTASA: 0x%lX", g_libGTASA);

    if(g_libGTASA){
        pfn_GiveWeapon=(fn_GiveWeapon)ADDR(Off::GiveWeapon);
        pfn_SetWanted =(fn_SetWanted) ADDR(Off::SetWantedLevel);
        pfn_CreateCar =(fn_CreateCar) ADDR(Off::CreateCar);
        pfn_ReqModel  =(fn_ReqModel)  ADDR(Off::RequestModel);
        pfn_LoadModels=(fn_LoadModels)ADDR(Off::LoadAllModels);
    }

    // Buscar libEGL en varias rutas
    const char* eglPaths[]={
        "libEGL.so",
        "/system/lib64/libEGL.so",
        "/system/vendor/lib64/libEGL.so",
        "/vendor/lib64/libEGL.so",
        nullptr
    };
    void* libEGL=nullptr;
    for(int i=0;eglPaths[i];i++){
        libEGL=dlopen(eglPaths[i],RTLD_NOW|RTLD_GLOBAL);
        if(libEGL){ LOGI("libEGL cargada: %s",eglPaths[i]); break; }
    }

    if(libEGL){
        void* swapFn=dlsym(libEGL,"eglSwapBuffers");
        if(swapFn){
            // InstallHook devuelve el trampoline — sin recursión
            real_eglSwapBuffers = InstallHook(swapFn,(void*)hk_eglSwapBuffers);
            if(real_eglSwapBuffers)
                LOGI("Hook instalado correctamente");
            else
                LOGE("Fallo al crear trampoline");
        } else {
            LOGE("No se encontro eglSwapBuffers");
        }
    } else {
        LOGE("No se pudo cargar libEGL");
    }

    LOGI("Init completo!");
    return nullptr;
}

// ── Constructor ───────────────────────────────────────────────
__attribute__((constructor))
static void OnLoad() {
    LOGI("=== CheatMenu SA v2.10 cargando ===");
    pthread_t tid;
    pthread_create(&tid,nullptr,InitThread,nullptr);
    pthread_detach(tid);
}

// ── Stub AML ──────────────────────────────────────────────────
#include "AML/AML.h"
extern "C" __attribute__((visibility("default")))
void AML_ModMain(IAML* aml) {
    LOGI("AML_ModMain — hook ya instalado por constructor");
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
