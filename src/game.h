#pragma once
// ============================================================
//  CheatMenu SA v2.10 — game.h
//  Offsets REALES extraídos del APK (base.apk, arm64-v8a)
//  Librería: libGTASA.so — ELF 64-bit ARM aarch64, not stripped
// ============================================================

#include <stdint.h>
#include <string>
#include <android/log.h>



// ── Base del juego ───────────────────────────────────────────
inline uintptr_t g_libGTASA = 0;
#define ADDR(offset) (g_libGTASA + (offset))

// ── Offsets REALES (verificados con readelf en el APK) ───────
namespace Off {

    // ── Variables globales ────────────────────────────────────
    // CWorld::Players (array de CPlayerInfo, tamaño 944 bytes)
    constexpr uintptr_t CWorld_Players      = 0x00BDC738;

    // CGame::currArea
    constexpr uintptr_t CGame_currArea      = 0x00BC2418;

    // CClock
    constexpr uintptr_t CClock_GameHours    = 0x00BBBC1A; // ms_nGameClockHours   (uint8)
    constexpr uintptr_t CClock_GameMinutes  = 0x00BBBC1B; // ms_nGameClockMinutes (uint8)

    // CWeather
    constexpr uintptr_t CWeather_OldWeather = 0x00D216F0; // OldWeatherType (int16)
    constexpr uintptr_t CWeather_NewWeather = 0x00D216F2; // NewWeatherType (int16)

    // ── Funciones del juego ───────────────────────────────────
    // CPed::GiveWeapon(eWeaponType, uint ammo, bool)
    constexpr uintptr_t GiveWeapon          = 0x0059525C;

    // CPlayerPed::SetWantedLevel(int)
    constexpr uintptr_t SetWantedLevel      = 0x005C76D0;

    // CCarCtrl::CreateCarForScript(int model, CVector, uchar)
    constexpr uintptr_t CreateCar           = 0x003AFA70;

    // CStreaming::RequestModel(int, int)
    constexpr uintptr_t RequestModel        = 0x003949E0;

    // CStreaming::LoadAllRequestedModels(bool)
    constexpr uintptr_t LoadAllModels       = 0x00396B28;

    // CVehicle::Fix()
    constexpr uintptr_t FixCar              = 0x0068F22C;

    // ── Offsets DENTRO de estructuras ────────────────────────
    // CPed (relativos al puntero del ped)
    // Estos son estándar en GTA SA Android — ajustar si crashea
    constexpr uintptr_t PED_HEALTH          = 0x540;
    constexpr uintptr_t PED_ARMOR           = 0x548;

    // PlayerInfo (dentro de CPlayerInfo)
    constexpr uintptr_t PED_MONEY           = 0xB8;  // $nMoney dentro de CPlayerInfo

    // CEntity::matrix (puntero a CMatrix dentro de CPlaceable)
    constexpr uintptr_t ENTITY_MATRIX_PTR   = 0x14;  // puntero a RwMatrix*

    // Posición dentro de RwMatrix (X,Y,Z de la traslación)
    constexpr uintptr_t PED_MATRIX_POS_X    = 0x30;
    constexpr uintptr_t PED_MATRIX_POS_Y    = 0x34;
    constexpr uintptr_t PED_MATRIX_POS_Z    = 0x38;

    // CVehicle
    constexpr uintptr_t VEHICLE_HEALTH      = 0x4C0;
    constexpr uintptr_t VEHICLE_SPEED_X     = 0x0B4;
    constexpr uintptr_t VEHICLE_SPEED_Y     = 0x0B8;
    constexpr uintptr_t VEHICLE_SPEED_Z     = 0x0BC;
}

// ── Estructuras básicas ───────────────────────────────────────
struct CVector { float x, y, z; };

struct CMatrix {
    float right[3];  float pad0;
    float up[3];     float pad1;
    float at[3];     float pad2;
    float pos[3];    float pad3; // pos[0]=X, pos[1]=Y, pos[2]=Z
};

struct CPlayerInfo {
    uint8_t pad[0xB8];
    int     nMoney;   // offset 0xB8
};

struct CPed {
    uint8_t     pad0[0x14];
    CMatrix*    pMatrix;    // offset 0x14
    uint8_t     pad1[0x540 - 0x14 - 8];
    float       fHealth;    // offset 0x540
    uint8_t     pad2[4];
    float       fArmor;     // offset 0x548
};

struct CPlayerPed : public CPed {};

struct CPlayerData {
    CPlayerPed* pPlayerPed;  // offset 0x00
    uint8_t     pad[0x48];
    CPlayerInfo* pPlayerInfo; // ajustar si no funciona
};

// ── Acceso al jugador ─────────────────────────────────────────
// CWorld::Players es un array de CPlayerData
// Players[0] = jugador local

inline CPlayerData* GetLocalPlayer() {
    if(!g_libGTASA) return nullptr;
    return reinterpret_cast<CPlayerData*>(ADDR(Off::CWorld_Players));
}

inline CPed* GetLocalPed() {
    auto* pl = GetLocalPlayer();
    if(!pl) return nullptr;
    return pl->pPlayerPed;
}

// ── Typedef de funciones del juego ───────────────────────────
using fn_GiveWeapon = void(*)(CPed*, int weaponId, uint32_t ammo, bool);
using fn_SetWanted  = void(*)(CPlayerPed*, int level);
using fn_CreateCar  = void(*)(float x, float y, float z, int model, bool);
using fn_ReqModel   = void(*)(int model, int flags);
using fn_LoadModels = void(*)(bool block);
using fn_FixCar     = void(*)(void* vehicle);

// ── Datos de armas ────────────────────────────────────────────
struct WeaponEntry { int id; const char* name; };
static const WeaponEntry Weapons[] = {
    { 1,  "Brass Knuckles" }, { 2,  "Golf Club"    }, { 3,  "Nightstick" },
    { 4,  "Knife"          }, { 5,  "Baseball Bat" }, { 6,  "Shovel"     },
    { 7,  "Pool Cue"       }, { 8,  "Katana"       }, { 9,  "Chainsaw"   },
    { 10, "Purple Dildo"   }, { 22, "9mm"          }, { 23, "Silenced 9mm"},
    { 24, "Desert Eagle"   }, { 25, "Shotgun"      }, { 26, "Sawnoff SG" },
    { 27, "Combat SG"      }, { 28, "Micro Uzi"    }, { 29, "MP5"        },
    { 30, "AK-47"          }, { 31, "M4"           }, { 32, "Tec-9"      },
    { 33, "Country Rifle"  }, { 34, "Sniper Rifle" }, { 35, "RPG"        },
    { 36, "HS Rocket"      }, { 37, "Flamethrower" }, { 38, "Minigun"    },
    { 39, "Satchel Charge" }, { 40, "Detonator"    }, { 41, "Spraycan"   },
    { 42, "Fire Extinguish"}, { 43, "Camera"       }, { 44, "Night Vision"},
    { 45, "Thermal Vision" }, { 46, "Parachute"    },
    { 0, nullptr }
};

// ── Datos de vehículos ────────────────────────────────────────
struct VehicleEntry { int id; const char* name; };
static const VehicleEntry Vehicles[] = {
    { 400, "Landstalker" }, { 401, "Bravura"    }, { 402, "Buffalo"   },
    { 403, "Linerunner"  }, { 404, "Perenial"   }, { 405, "Sentinel"  },
    { 406, "Dumper"      }, { 407, "Firetruck"  }, { 408, "Trashmaster"},
    { 409, "Stretch"     }, { 410, "Manana"     }, { 411, "Infernus"  },
    { 412, "Voodoo"      }, { 413, "Pony"       }, { 414, "Mule"      },
    { 415, "Cheetah"     }, { 416, "Ambulance"  }, { 418, "Moonbeam"  },
    { 419, "Esperanto"   }, { 420, "Taxi"       }, { 421, "Washington"},
    { 422, "Bobcat"      }, { 423, "Mr Whoopee" }, { 424, "BF Injection"},
    { 425, "Hunter"      }, { 426, "Premier"    }, { 427, "Enforcer"  },
    { 428, "Securicar"   }, { 429, "Banshee"    }, { 430, "Predator"  },
    { 431, "Bus"         }, { 432, "Rhino"      }, { 433, "Barracks OL"},
    { 434, "Hotknife"    }, { 436, "Trailer"    }, { 437, "Previon"   },
    { 438, "Coach"       }, { 439, "Cabbie"     }, { 440, "Stallion"  },
    { 441, "Rumpo"       }, { 442, "RC Bandit"  }, { 443, "Romero"    },
    { 444, "Packer"      }, { 445, "Monster"    }, { 446, "Admiral"   },
    { 451, "Turismo"     }, { 452, "Speeder"    }, { 453, "Reefer"    },
    { 454, "Tropic"      }, { 455, "Flatbed"    }, { 456, "Yankee"    },
    { 457, "Caddy"       }, { 458, "Solair"     }, { 459, "Berkley RC Van"},
    { 460, "Skimmer"     }, { 461, "PCJ-600"    }, { 462, "Faggio"    },
    { 463, "Freeway"     }, { 464, "RC Baron"   }, { 465, "RC Raider" },
    { 466, "Glendale"    }, { 467, "Oceanic"    }, { 468, "Sanchez"   },
    { 469, "Sparrow"     }, { 470, "Patriot"    }, { 471, "Quad"      },
    { 472, "Coastguard"  }, { 473, "Dinghy"     }, { 474, "Hermes"    },
    { 475, "Sabre"       }, { 476, "Rustler"    }, { 477, "ZR-350"    },
    { 478, "Walton"      }, { 479, "Regina"     }, { 480, "Comet"     },
    { 481, "BMX"         }, { 482, "Burrito"    }, { 483, "Camper"    },
    { 484, "Marquis"     }, { 485, "Baggage"    }, { 486, "Dozer"     },
    { 487, "Maverick"    }, { 488, "News Chopper"}, { 489, "Rancher"  },
    { 490, "FBI Rancher" }, { 491, "Virgo"      }, { 492, "Greenwood" },
    { 494, "Hotring A"   }, { 495, "Sandy"      }, { 496, "Domestobot"},
    { 497, "Stretch Limo"}, { 498, "Phat Quad"  }, { 499, "Club"      },
    { 500, "Freight Train"}, {502, "Tug"        }, { 503, "Trailer 3" },
    { 504, "Hotring B"   }, { 505, "Bloodring"  }, { 506, "Rancher 2" },
    { 507, "Super GT"    }, { 508, "Elegant"    }, { 509, "Journey"   },
    { 510, "Bike"        }, { 511, "Mountain Bike"}, {512, "Beagle"   },
    { 513, "Cropduster"  }, { 514, "Stuntplane" }, { 515, "Petro Tanker"},
    { 516, "Roadtrain"   }, { 517, "Nebula"     }, { 518, "Majestic"  },
    { 519, "Buccaneer"   }, { 520, "Shamal"     }, { 521, "Hydra"     },
    { 522, "FCR-900"     }, { 523, "NRG-500"    }, { 524, "HPV1000"   },
    { 525, "Cement Truck"}, { 526, "Towtruck"   }, { 527, "Fortune"   },
    { 528, "Cadrona"     }, { 529, "FBI Truck"  }, { 530, "Willard"   },
    { 531, "Forklift"    }, { 532, "Tractor"    }, { 533, "Combine"   },
    { 534, "Feltzer"     }, { 535, "Remington"  }, { 536, "Slamvan"   },
    { 537, "Blade"       }, { 538, "Freight"    }, { 539, "Streak"    },
    { 540, "Vortex"      }, { 541, "Vincent"    }, { 542, "Bullet"    },
    { 543, "Clover"      }, { 544, "Sadler"     }, { 545, "Firetruck 2"},
    { 546, "Hustler"     }, { 547, "Intruder"   }, { 548, "Primo"     },
    { 549, "Cargobob"    }, { 550, "Tampa"      }, { 551, "Sunrise"   },
    { 552, "Merit"       }, { 553, "Utility Van"}, { 554, "Nevada"    },
    { 555, "Yosemite"    }, { 556, "Windsor"    }, { 557, "Monster A" },
    { 558, "Monster B"   }, { 559, "Uranus"     }, { 560, "Jester"    },
    { 561, "Sultan"      }, { 562, "Stratum"    }, { 563, "Elegy"     },
    { 564, "Raindance"   }, { 565, "RC Tiger"   }, { 566, "Flash"     },
    { 567, "Tahoma"      }, { 568, "Savanna"    }, { 569, "Bandito"   },
    { 570, "Freight Flat"}, { 571, "Streak Carriage"}, {572,"Kart"    },
    { 573, "Mower"       }, { 574, "Duneride"   }, { 575, "Sweeper"   },
    { 576, "Broadway"    }, { 577, "Tornado"    }, { 578, "AT-400"    },
    { 579, "DFT-30"      }, { 580, "Huntley"    }, { 581, "Stafford"  },
    { 582, "BF-400"      }, { 583, "Newsvan"    }, { 584, "Tug"       },
    { 585, "Petrotrailer"}, { 586, "Emperor"    }, { 587, "Wayfarer"  },
    { 588, "Euros"       }, { 589, "Hotdog"     }, { 590, "Club"      },
    { 591, "Freight Trailer"}, {592,"Streak Trailer"}, {593,"Kart 2"  },
    { 594, "Bugged"      }, { 595, "Camper Van" }, { 596, "Dozer 2"   },
    { 597, "RC Cam"      }, { 598, "Launch"     }, { 599, "Police LS" },
    { 600, "Police SF"   }, { 601, "Police LV"  }, { 602, "Police Ranger"},
    { 603, "Picador"     }, { 604, "S.W.A.T."   }, { 605, "Alpha"     },
    { 606, "Phoenix"     }, { 607, "Glendale 2" }, { 608, "Sadler 2"  },
    { 609, "Luggage Trailer"}, {610,"Luggage Trailer 2"}, {611,"Stair Trailer"},
    { 612, "Boxville 2"  }, { 613, "Farm Trailer"}, {614,"Street Cleaner"},
    { 0, nullptr }
};

// ── Datos de teletransporte ───────────────────────────────────
struct LocationEntry { const char* name; float x, y, z; };
static const LocationEntry Locations[] = {
    { "Los Santos - Grove St.",   2495.0f, -1666.0f,  13.3f },
    { "Los Santos - Centro",      1371.0f, -1270.0f,  13.4f },
    { "Los Santos - Aeropuerto",  1686.0f,  -2437.0f, 13.6f },
    { "San Fierro - Centro",     -1270.0f,   129.0f,  19.2f },
    { "San Fierro - Aeropuerto", -1580.0f,  -107.0f,  14.5f },
    { "Las Venturas - Strip",     2000.0f,  1007.0f,  10.8f },
    { "Las Venturas - Aeropuerto",1672.0f,  1447.0f,  10.8f },
    { "Tierra Robada",           -967.0f,  1895.0f,   5.3f  },
    { "Bone County",              109.0f,  1244.0f,   19.6f },
    { "Monte Chiliad",           -2178.0f,  -2427.0f, 31.0f },
    { "Area 69",                  213.0f,   1870.0f,  17.6f },
    { "Desierto Rojo",            632.0f,    -521.0f, 12.0f },
    { nullptr, 0, 0, 0 }
};

// ── Nombres de clima ──────────────────────────────────────────
static const char* WeatherNames[] = {
    "Extra Sunny LS", "Sunny LS", "Cloudy LS", "Rainy LS",
    "Foggy SF", "Sunny SF", "Extra Sunny LV", "Dust Storm LV",
    "Very Cloudy LV", "Overcast", "Rainy", "Thunder",
    "Very Cloudy", "Sunny", "Extra Sunny", "Swamp Sunset",
    "Stormy", "Sandy", "Sandy 2", "Dark"
};
