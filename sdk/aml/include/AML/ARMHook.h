#pragma once
#include <stdint.h>
#include <sys/mman.h>
inline bool ARMHook(uintptr_t a, void* f, void** o){ return false; }
inline void UnprotectMemory(uintptr_t a, size_t s=0x1000){
    mprotect((void*)(a & ~(uintptr_t)0xFFF), s+0x1000, 7);
}
