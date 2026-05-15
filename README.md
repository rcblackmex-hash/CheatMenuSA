# CheatMenu SA v2.10 — Tu mod 100% tuyo

Cheat Menu para GTA SA Android arm64-v8a, construido sobre AML + ImGui.

---

## 📁 Estructura del proyecto

```
CheatMenuSA/
├── CMakeLists.txt          ← Configuración de compilación
├── src/
│   ├── main.cpp            ← Lógica principal + menú ImGui
│   └── game.h              ← Direcciones + estructuras del juego
├── imgui/                  ← Fuentes de ImGui (debes agregar)
│   ├── imgui.h
│   ├── imgui.cpp
│   ├── imgui_draw.cpp
│   ├── imgui_tables.cpp
│   ├── imgui_widgets.cpp
│   └── backends/
│       ├── imgui_impl_opengl3.h
│       └── imgui_impl_opengl3.cpp
├── sdk/
│   └── aml/
│       └── include/        ← Headers del SDK de AML
└── README.md
```

---

## 🛠️ Cómo compilar (PC con Windows/Linux)

### Paso 1 — Instalar Android NDK
- Descarga Android NDK r25c (o r26) desde:
  https://developer.android.com/ndk/downloads
- Extrae en una carpeta, ej: `C:\android-ndk-r25c`

### Paso 2 — Obtener ImGui
- Descarga ImGui desde: https://github.com/ocornut/imgui
- Copia los archivos a la carpeta `imgui/` de este proyecto

### Paso 3 — Obtener el SDK de AML
- Descarga el SDK de AML (RusJJ) desde:
  https://github.com/RusJJ/AndroidModLoader
- Copia los headers a `sdk/aml/include/`

### Paso 4 — Compilar

**Windows (PowerShell):**
```powershell
# Crear carpeta de build
mkdir build && cd build

# Configurar con CMake para arm64
cmake .. `
  -DCMAKE_TOOLCHAIN_FILE="C:\android-ndk-r25c\build\cmake\android.toolchain.cmake" `
  -DANDROID_ABI=arm64-v8a `
  -DANDROID_PLATFORM=android-28 `
  -DCMAKE_BUILD_TYPE=Release

# Compilar
cmake --build . --config Release
```

**Linux:**
```bash
mkdir build && cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/ndk/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Paso 5 — Instalar el mod

El archivo resultante se llama `libCheatMenu.so`.
Cópialo a tu teléfono en:
```
/storage/emulated/0/Android_unprotected/data/com.rockstargames.gtasa/mods/
```

---

## 🎮 Uso en el juego

| Acción | Resultado |
|--------|-----------|
| Mantén 2 dedos ~1 segundo | Abre/cierra el menú |
| Arrastra el título del menú | Mueve la ventana |
| Tap en cualquier tab | Cambia de sección |

---

## ⚙️ Configuración del menú

El archivo de config se guarda automáticamente en:
```
configs/tuusuario.CheatMenu.ini
```

---

## 🔧 Actualizar direcciones (game.h)

Si algo no funciona, las direcciones están en `src/game.h`
en el namespace `Off::`. Puedes actualizarlas con IDA Pro,
Ghidra, o usando el AML pattern scanner.

Para verificar una dirección con AML:
```cpp
// Ejemplo: buscar por patrón de bytes en lugar de offset fijo
uintptr_t addr = aml->FindPattern("libGTASA.so", "? ? ? ? 00 00 00 00 FF");
```

---

## 📋 Tabs del menú

| Tab | Funciones |
|-----|-----------|
| **Teleport** | 20 ubicaciones (LS, SF, LV, Campo) |
| **Player** | God Mode, Wanted, HP, Armor, Dinero |
| **Vehículo** | Spawn de 80+ vehículos con búsqueda |
| **Armas** | Dar todas o individual (30+ armas) |
| **Juego** | Clima (20 tipos), Hora del juego |
| **Acerca** | Info del mod |

---

## 📌 Notas v2.10

- Compilar para `arm64-v8a` (tu Honor 200 es 64-bit)
- AML mínimo requerido: v1.2
- GTA SA target: v2.10
- ImGui requerido: 1.91+

---

*CheatMenu SA v2.10 — 100% tuyo*
