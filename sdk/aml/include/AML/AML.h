#pragma once
#include <stdint.h>
#include <stddef.h>
#include <dlfcn.h>

enum class AMLModType { Lib=0, Tool=1, Patcher=2 };

class IAML {
public:
    virtual ~IAML(){}
    virtual uintptr_t GetLib(const char* n){ return 0; }
    virtual void* GetSym(uintptr_t l, const char* s){ return nullptr; }
    virtual bool Hook(uintptr_t a, void* f, void** o){ return false; }
    virtual void PlaceB(uintptr_t a, uintptr_t d){}
    virtual void PlaceBL(uintptr_t a, uintptr_t d){}
    virtual void PlaceBLX(uintptr_t a, uintptr_t d){}
    virtual void Redirect(uintptr_t a, uintptr_t d){}
    virtual void Write(uintptr_t a, uintptr_t v, size_t s){}
    virtual void Read(uintptr_t a, uintptr_t o, size_t s){}
    virtual bool IsGameVersion(int v){ return true; }
    virtual uintptr_t PatternScan(const char* p, const char* m){ return 0; }
};

typedef void (*AMLMainFn)(IAML* aml);

#define MOD_MAIN AML_OnModMain

struct AMLModInfo {
    const char* szGUID;
    const char* szName;
    const char* szVersion;
    const char* szAuthor;
    const char* szDescription;
    AMLMainFn   fnMain;
    AMLModType  eType;
};

#define AML_MOD_DEFINE static AMLModInfo __aml_modinfo =

void AML_OnModMain(IAML* aml);
