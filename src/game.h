#pragma once
// ============================================================
//  CheatMenu SA v2.10 — Tu mod 100% tuyo
//  game.h — Estructuras y direcciones de GTA SA Android arm64
// ============================================================

#include <stdint.h>
#include <string>

// ── Logging ─────────────────────────────────────────────────
#include <android/log.h>
#define LOG_TAG "CheatMenuSA"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ── Base del juego (se rellena en MOD_MAIN) ──────────────────
inline uintptr_t g_libGTASA = 0;
#define ADDR(offset) (g_libGTASA + (offset))

// ── Offsets GTA SA v2.10 arm64-v8a ───────────────────────────
// Si algo no funciona → busca el patrón en IDA/Ghidra y
// actualiza el offset aquí. Todo está centralizado en este archivo.
namespace Off {

    // Punteros globales del juego
    constexpr uintptr_t CWorld_Players      = 0x008E4A58; // CWorld::Players[0]
    constexpr uintptr_t CGame_currArea      = 0x008E4960;
    constexpr uintptr_t CClock_GameHours    = 0x008E4900;
    constexpr uintptr_t CClock_GameMinutes  = 0x008E4902;
    constexpr uintptr_t CWeather_OldWeather = 0x00C88070;
    constexpr uintptr_t CWeather_NewWeather = 0x00C88072;

    // Funciones del juego
    constexpr uintptr_t GiveWeapon          = 0x004AE830; // CPed::GiveWeapon(int id, int ammo)
    constexpr uintptr_t SetWantedLevel      = 0x004D4430; // CPlayerPed::SetWantedLevel(int)
    constexpr uintptr_t TeleportPlayer      = 0x004E8A20; // CEntity::Teleport(float x,y,z)
    constexpr uintptr_t CreateCar           = 0x004AA550; // CCarCtrl::CreateCarForScript(int model,float x,y,z)
    constexpr uintptr_t SetHeading          = 0x0047C7B0; // CPlaceable::SetHeading(float)
    constexpr uintptr_t FixCar              = 0x006A3400; // CVehicle::Fix()
    constexpr uintptr_t FlipCar             = 0x006A3510; // CVehicle::Flip()
    constexpr uintptr_t RequestModel        = 0x004087B0; // CStreaming::RequestModel(int,int)
    constexpr uintptr_t LoadAllModels       = 0x00408990; // CStreaming::LoadAllRequestedModels(bool)
    constexpr uintptr_t MarkModelAsNoLongerNeeded = 0x00408B80;

    // Offsets DENTRO de la estructura del player (relativos al puntero del ped)
    constexpr uintptr_t PED_HEALTH          = 0x540;
    constexpr uintptr_t PED_ARMOR           = 0x548;
    constexpr uintptr_t PED_MONEY           = 0x004; // dentro de PlayerInfo
    constexpr uintptr_t PED_MATRIX_POS_X    = 0x030;
    constexpr uintptr_t PED_MATRIX_POS_Y    = 0x034;
    constexpr uintptr_t PED_MATRIX_POS_Z    = 0x038;
    constexpr uintptr_t VEHICLE_SPEED_X     = 0x0B4;
    constexpr uintptr_t VEHICLE_SPEED_Y     = 0x0B8;
    constexpr uintptr_t VEHICLE_SPEED_Z     = 0x0BC;
    constexpr uintptr_t VEHICLE_HEALTH      = 0x4C0;
}

// ── Estructuras del juego ─────────────────────────────────────

struct CVector {
    float x, y, z;
};

struct CPlayerInfo {
    uint8_t  pad0[4];
    int      nMoney;       // Offset 0x004
    // ... más campos
};

struct CPed {
    uint8_t     pad0[0x18];
    CVector*    pMatrix;        // Puntero a la matriz de posición
    uint8_t     pad1[0x518];
    float       fHealth;        // Offset 0x540
    uint8_t     pad2[4];
    float       fArmor;         // Offset 0x548
    // ... más campos
};

struct CPlayerPed : public CPed {
    // Hereda de CPed
};

struct CWorldPlayers {
    CPlayerInfo*   pPlayerInfo;  // +0x00
    CPlayerPed*    pPed;         // +0x08 (arm64, puntero 8 bytes)
};

// ── Getters rápidos ───────────────────────────────────────────

inline CWorldPlayers* GetLocalPlayer() {
    if (!g_libGTASA) return nullptr;
    return reinterpret_cast<CWorldPlayers*>(ADDR(Off::CWorld_Players));
}

inline CPlayerPed* GetLocalPed() {
    auto* p = GetLocalPlayer();
    if (!p) return nullptr;
    return p->pPed;
}

inline bool IsPlayerValid() {
    return GetLocalPed() != nullptr;
}

// ── Tipos de función del juego ────────────────────────────────

using fn_GiveWeapon  = void(*)(CPed*, int weaponId, int ammo, bool);
using fn_SetWanted   = void(*)(void*, int level);
using fn_CreateCar   = void*(*)(int model, float x, float y, float z, bool);
using fn_FixCar      = void(*)(void*);
using fn_LoadModels  = void(*)(bool);
using fn_ReqModel    = void(*)(int model, int flags);

// ── IDs de clima ─────────────────────────────────────────────
enum WeatherType : int {
    WEATHER_EXTRASUNNY_LA  = 0,
    WEATHER_SUNNY_LA       = 1,
    WEATHER_CLOUDY_LA      = 2,
    WEATHER_SUNNY_SF       = 3,
    WEATHER_FOGGY_SF       = 7,
    WEATHER_SUNNY_VEGAS    = 11,
    WEATHER_THUNDER        = 16,
    WEATHER_RAIN           = 8,
    WEATHER_SANDSTORM      = 19,
    WEATHER_COUNT          = 20
};

static const char* WeatherNames[] = {
    "Extra Soleado LA", "Soleado LA", "Nublado LA",
    "Soleado SF", "Lluvioso SF", "Nublado SF",
    "Brumoso SF", "Brumoso SF2", "Lluvia",
    "Lluvia pesada", "Llovizna", "Soleado Vegas",
    "Soleado Vegas2", "Nublado Vegas", "Muy nublado",
    "Tormenta eléctrica", "Sandy", "Sandy2",
    "Overcast", "Tormenta de arena"
};

// ── IDs de arma ───────────────────────────────────────────────
struct WeaponEntry { int id; const char* name; };
static const WeaponEntry Weapons[] = {
    {1,  "Puños"}, {2, "Maza de golf"}, {3, "Nightstick"},
    {4,  "Cuchillo"}, {5, "Bat"}, {6, "Pala"},
    {8,  "Katana"}, {9, "Cadena"}, {22, "Pistola"},
    {23, "Pistola silenciada"}, {24, "Desert Eagle"},
    {25, "Escopeta"}, {26, "Escopeta recortada"},
    {27, "SPAS-12"}, {28, "Micro-Uzi"}, {29, "MP5"},
    {30, "AK-47"}, {31, "M4"}, {32, "Tec-9"},
    {33, "Country Rifle"}, {34, "Sniper"}, {35, "Rocket Launcher"},
    {36, "RPG"}, {37, "Heatseeker"}, {38, "Flamethrower"},
    {39, "Minigun"}, {40, "Satchel Charges"}, {41, "Detonador"},
    {42, "Spray"}, {43, "Extintor"}, {44, "Cámara"},
    {46, "Paracaídas"}, {0, nullptr}
};

// ── Ubicaciones de teletransporte ─────────────────────────────
struct TeleportLocation { const char* name; float x, y, z; };
static const TeleportLocation Locations[] = {
    // Los Santos
    {"Grove Street",         2495.0f, -1688.0f,  13.3f},
    {"Aeropuerto LS",        1682.0f, -2407.0f,  13.5f},
    {"Vinewood",             1370.0f, -1325.0f,  13.4f},
    {"Santa Maria Beach",     339.0f, -1600.0f,   7.5f},
    {"Verdant Bluffs",       1108.0f, -1754.0f,  23.5f},
    {"Willow Field",         2640.0f, -1982.0f,  13.5f},
    {"Commerce",              711.0f, -1460.0f,  24.0f},
    {"Little Mexico",         521.0f, -1302.0f,  17.0f},
    // San Fierro
    {"SF Downtown",         -1982.0f,   411.0f,  35.0f},
    {"SF Aeropuerto",       -1390.0f,  -26.0f,   14.0f},
    {"Chinatown SF",        -2183.0f,   641.0f,  35.0f},
    {"Doherty Garage SF",   -1937.0f,   227.0f,  34.0f},
    // Las Venturas
    {"LV Strip",             2003.0f,  1015.0f,  10.7f},
    {"LV Aeropuerto",        1667.0f,  1203.0f,  10.8f},
    {"Caligula's Palace",    2228.0f,  1584.0f,  10.8f},
    {"Four Dragons Casino",   2020.0f,  1008.0f, 10.8f},
    // Campo
    {"Mount Chiliad Pico",    -2186.0f, -1436.0f, 469.0f},
    {"Tierra Robada",        -869.0f,  1374.0f,   7.2f},
    {"Bone County",           280.0f,  1449.0f,   9.3f},
    {"Area 69",               213.0f,  1870.0f,  17.6f},
    {nullptr, 0, 0, 0}
};

// ── Modelos de vehículo ───────────────────────────────────────
struct VehicleEntry { int id; const char* name; };
static const VehicleEntry Vehicles[] = {
    // Coches
    {400, "Landstalker"}, {401, "Bravura"}, {402, "Buffalo"},
    {403, "Linerunner"}, {404, "Perenniel"}, {405, "Sentinel"},
    {409, "Stretch"}, {410, "Manana"}, {411, "Infernus"},
    {412, "Voodoo"}, {415, "Cheetah"}, {416, "Ambulance"},
    {418, "Moonbeam"}, {419, "Esperanto"}, {420, "Taxi"},
    {421, "Washington"}, {422, "Bobcat"}, {426, "Premier"},
    {429, "Banshee"}, {436, "Previon"}, {438, "Cabbie"},
    {439, "Stallion"}, {445, "Admiral"}, {451, "Turismo"},
    {458, "Solair"}, {466, "Supergt"}, {474, "Hermes"},
    {475, "Sabre"}, {477, "ZR-350"}, {480, "Comet"},
    {489, "Rancher"}, {491, "Virgo"}, {492, "Greenwood"},
    {494, "Regina"}, {495, "Hotknife"}, {496, "Sandking"},
    {500, "Mesa"}, {502, "Hotring Racer"}, {503, "Hotring Racer2"},
    {516, "Nebula"}, {517, "Majestic"}, {518, "Buccaneer"},
    {526, "Fortune"}, {527, "Cadrona"}, {533, "Feltzer"},
    {534, "Remington"}, {535, "Slamvan"}, {536, "Blade"},
    {540, "Vincent"}, {541, "Bullet"}, {542, "Clover"},
    {543, "Sadler"}, {550, "Club"}, {551, "Frieght"}, // tren
    {558, "Uranus"}, {559, "Jester"}, {560, "Sultan"},
    {561, "Stratum"}, {562, "Elegy"}, {565, "Flash"},
    {566, "Tahoma"}, {567, "Savanna"}, {575, "Broadway"},
    {576, "Tornado"}, {579, "Huntley"}, {580, "Stafford"},
    {585, "Euros"}, {587, "Phoenix"}, {589, "Club"},
    // Motos
    {448, "Freeway"}, {461, "PCJ-600"}, {462, "Faggio"},
    {463, "Freeway"}, {468, "Sanchez"}, {471, "Quad"},
    {521, "FCR-900"}, {522, "NRG-500"}, {523, "HPV1000"},
    {581, "BF-400"}, {586, "Wayfarer"},
    // Botes
    {430, "Predator"}, {446, "Squalo"}, {452, "Speeder"},
    {453, "Reefer"}, {454, "Tropic"}, {472, "Coastguard"},
    {473, "Dinghy"}, {484, "Marquis"}, {493, "Jetmax"},
    // Aviones/Helicópteros
    {417, "Leviathan"}, {425, "Hunter"}, {432, "Maverick"},
    {433, "Santoas Dumont"}, {434, "Sea Sparrow"},
    {447, "Seasparrow"}, {460, "Skimmer"}, {469, "Sparrow"},
    {476, "Rustler"}, {487, "Maverick"}, {488, "VCN Maverick"},
    {497, "Police Maverick"}, {513, "Andromada"}, {519, "Shamal"},
    {520, "Hydra"}, {524, "Nevada"}, {553, "Berkley's RC"},
    {563, "RC Tiger"}, {577, "AT-400"}, {592, "Andromada"},
    {593, "Dodo"},
    {0, nullptr}
};
