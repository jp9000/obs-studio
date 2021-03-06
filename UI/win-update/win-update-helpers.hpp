#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Wincrypt.h>

#include <cstdint>
#include <string>

/* ------------------------------------------------------------------------ */

template<typename T, void freefunc(T)> class CustomHandle {
	T handle;

public:
	inline CustomHandle() : handle(0) {}
	inline CustomHandle(T in) : handle(in) {}
	inline ~CustomHandle()
	{
		if (handle)
			freefunc(handle);
	}

	inline T *operator&() { return &handle; }
	inline operator T() const { return handle; }
	inline T get() const { return handle; }

	inline CustomHandle<T, freefunc> &operator=(T in)
	{
		if (handle)
			freefunc(handle);
		handle = in;
		return *this;
	}

	inline bool operator!() const { return !handle; }
};

void FreeProvider(HCRYPTPROV prov);
void FreeHash(HCRYPTHASH hash);
void FreeKey(HCRYPTKEY key);

using CryptProvider = CustomHandle<HCRYPTPROV, FreeProvider>;
using CryptHash = CustomHandle<HCRYPTHASH, FreeHash>;
using CryptKey = CustomHandle<HCRYPTKEY, FreeKey>;

/* ------------------------------------------------------------------------ */

template<typename T> class LocalPtr {
	T *ptr = nullptr;

public:
	inline ~LocalPtr()
	{
		if (ptr)
			LocalFree(ptr);
	}

	inline T **operator&() { return &ptr; }
	inline operator T() const { return ptr; }
	inline T *get() const { return ptr; }

	inline bool operator!() const { return !ptr; }

	inline T *operator->() { return ptr; }
};

/* ------------------------------------------------------------------------ */

std::string vstrprintf(const char *format, va_list args);
std::string strprintf(const char *format, ...);
