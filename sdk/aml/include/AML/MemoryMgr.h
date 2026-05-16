#pragma once
#include <stdint.h>
#include <sys/mman.h>
#include <string.h>
inline void Unprotect(uintptr_t a, size_t s=0x1000){
    mprotect((void*)(a & ~(uintptr_t)0xFFF), s+0x1000, 7);
}
template<typename T>
inline void MemWrite(uintptr_t a, T v){
    Unprotect(a, sizeof(T));
    *reinterpret_cast<T*>(a) = v;
}
template<typename T>
inline T MemRead(uintptr_t a){ return *reinterpret_cast<T*>(a); }
