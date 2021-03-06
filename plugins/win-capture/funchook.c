#include <windows.h>
#include <stdlib.h>
#include "funchook.h"

#define JMP_64_SIZE 14
#define JMP_32_SIZE 5

#define X86_NOP 0x90
#define X86_JMP_NEG_5 0xF9EB

static inline void fix_permissions(void *addr, size_t size)
{
	DWORD protect_val;
	VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &protect_val);
}

void hook_init(struct func_hook *hook, void *func_addr, void *hook_addr,
	       const char *name)
{
	memset(hook, 0, sizeof(*hook));

	hook->func_addr = (uintptr_t)func_addr;
	hook->hook_addr = (uintptr_t)hook_addr;
	hook->name = name;

	fix_permissions((void *)(hook->func_addr - JMP_32_SIZE),
			JMP_64_SIZE + JMP_32_SIZE);

	memcpy(hook->unhook_data, func_addr, JMP_64_SIZE);
}

static inline size_t patch_size(struct func_hook *hook)
{
	return hook->is_64bit_jump ? JMP_64_SIZE : JMP_32_SIZE;
}

static const uint8_t longjmp64[6] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};

static inline void rehook64(struct func_hook *hook)
{
	uint8_t data[JMP_64_SIZE];
	uintptr_t *ptr_loc = (uintptr_t *)((uint8_t *)data + sizeof(longjmp64));

	fix_permissions((void *)hook->func_addr, JMP_64_SIZE);

	memcpy(data, (void *)hook->func_addr, JMP_64_SIZE);
	memcpy(data, longjmp64, sizeof(longjmp64));
	*ptr_loc = hook->hook_addr;

	hook->call_addr = (void *)hook->func_addr;
	hook->type = HOOKTYPE_FORWARD_OVERWRITE;
	hook->hooked = true;

	memcpy((void *)hook->func_addr, data, JMP_64_SIZE);
}

static inline void hook_reverse_new(struct func_hook *hook, uint8_t *p)
{
	hook->call_addr = (void *)(hook->func_addr + 2);
	hook->type = HOOKTYPE_REVERSE_CHAIN;
	hook->hooked = true;

	p[0] = 0xE9;
	*((uint32_t *)&p[1]) = (uint32_t)(hook->hook_addr - hook->func_addr);
	*((uint16_t *)&p[5]) = X86_JMP_NEG_5;
}

static inline void hook_reverse_chain(struct func_hook *hook, uint8_t *p)
{
	if (hook->type != HOOKTYPE_FORWARD_OVERWRITE)
		return;

	hook->call_addr = (void *)(hook->func_addr + *((int32_t *)&p[1]));
	hook->type = HOOKTYPE_REVERSE_CHAIN;
	hook->hooked = true;

	*((uint32_t *)&p[1]) = (uint32_t)(hook->hook_addr - hook->func_addr);
}

static inline void hook_forward_chain(struct func_hook *hook, uint8_t *p,
				      intptr_t offset)
{
	int32_t cur_offset = *(int32_t *)&p[6];

	if (hook->type != HOOKTYPE_FORWARD_OVERWRITE)
		return;

	hook->call_addr = (void *)(hook->func_addr + JMP_32_SIZE + cur_offset);
	hook->type = HOOKTYPE_FORWARD_CHAIN;
	hook->hooked = true;

	*((int32_t *)&p[6]) = (int32_t)offset;
}

static inline void hook_forward_overwrite(struct func_hook *hook,
					  intptr_t offset)
{
	uint8_t *ptr = (uint8_t *)hook->func_addr;

	hook->call_addr = (void *)hook->func_addr;
	hook->type = HOOKTYPE_FORWARD_OVERWRITE;
	hook->hooked = true;

	*(ptr++) = 0xE9;
	*((int32_t *)ptr) = (int32_t)offset;
}

static inline void rehook32(struct func_hook *hook, bool force, intptr_t offset)
{
	fix_permissions((void *)(hook->func_addr - JMP_32_SIZE),
			JMP_32_SIZE * 2);

	if (force || !hook->started) {
		uint8_t *p = (uint8_t *)hook->func_addr - JMP_32_SIZE;
		size_t nop_count = 0;

		/* check for reverse chain hook availability */
		for (size_t i = 0; i < JMP_32_SIZE; i++) {
			if (p[i] == X86_NOP)
				nop_count++;
		}

		if (nop_count == JMP_32_SIZE && p[5] == 0x8B && p[6] == 0xFF) {
			hook_reverse_new(hook, p);

		} else if (p[0] == 0xE9 &&
			   *(uint16_t *)&p[5] == X86_JMP_NEG_5) {
			hook_reverse_chain(hook, p);

		} else if (p[5] == 0xE9) {
			hook_forward_chain(hook, p, offset);

		} else if (hook->type != HOOKTYPE_FORWARD_OVERWRITE) {
			hook->type = HOOKTYPE_FORWARD_OVERWRITE;
		}

		hook->started = true;
	}

	if (hook->type == HOOKTYPE_FORWARD_OVERWRITE) {
		hook_forward_overwrite(hook, offset);
	}
}

/*
 * Creates memory close to the target function, used to force the actual hook
 * to use a 32bit jump instead of a 64bit jump, thus preventing the chance of
 * overwriting adjacent functions, which can cause a crash.  (by R1CH)
 */
static void setup_64bit_bounce(struct func_hook *hook, intptr_t *offset)
{
	MEMORY_BASIC_INFORMATION mbi;
	uintptr_t address;
	uintptr_t newdiff;
	SYSTEM_INFO si;
	bool success;
	int pagesize;
	int i;

	success = VirtualQueryEx(GetCurrentProcess(),
				 (const void *)hook->func_addr, &mbi,
				 sizeof(mbi));
	if (!success)
		return;

	GetSystemInfo(&si);
	pagesize = (int)si.dwAllocationGranularity;

	address = (uintptr_t)mbi.AllocationBase - pagesize;
	for (i = 0; i < 256; i++, address -= pagesize) {
		hook->bounce_addr = VirtualAlloc((LPVOID)address, pagesize,
						 MEM_RESERVE | MEM_COMMIT,
						 PAGE_EXECUTE_READWRITE);
		if (hook->bounce_addr)
			break;
	}

	if (!hook->bounce_addr) {
		address = (uintptr_t)mbi.AllocationBase + mbi.RegionSize +
			  pagesize;
		for (i = 0; i < 256; i++, address += pagesize) {
			hook->bounce_addr =
				VirtualAlloc((LPVOID)address, pagesize,
					     MEM_RESERVE | MEM_COMMIT,
					     PAGE_EXECUTE_READWRITE);
			if (hook->bounce_addr)
				break;
		}
	}

	if (!hook->bounce_addr)
		return;

	if ((hook->func_addr + 5) > (uintptr_t)hook->bounce_addr)
		newdiff = hook->func_addr + 5 - (uintptr_t)hook->bounce_addr;
	else
		newdiff = (uintptr_t)hook->bounce_addr - hook->func_addr + 5;

	if (newdiff <= 0x7ffffff0) {
		uint8_t *addr = (uint8_t *)hook->bounce_addr;

		FillMemory(hook->bounce_addr, pagesize, 0xCC);

		*(addr++) = 0xFF;
		*(addr++) = 0x25;
		*((uint32_t *)addr) = 0;
		*((uint64_t *)(addr + 4)) = hook->hook_addr;

		hook->hook_addr = (uint64_t)hook->bounce_addr;
		*offset = hook->hook_addr - hook->func_addr - JMP_32_SIZE;
		hook->is_64bit_jump = false;
	}
}

void do_hook(struct func_hook *hook, bool force)
{
	intptr_t offset;

	if (!force && hook->hooked)
		return;

	/* copy back the memory that was previously encountered to preserve
	 * the current hook and any newer hooks on top */
	if (hook->started && !force) {
		uintptr_t addr;
		size_t size;

		if (hook->type == HOOKTYPE_REVERSE_CHAIN) {
			addr = hook->func_addr - JMP_32_SIZE;
			size = JMP_32_SIZE;
		} else {
			addr = hook->func_addr;
			size = patch_size(hook);
		}

		memcpy((void *)addr, hook->rehook_data, size);
		hook->hooked = true;
		return;
	}

	offset = hook->hook_addr - hook->func_addr - JMP_32_SIZE;

#ifdef _WIN64
	hook->is_64bit_jump = (llabs(offset) >= 0x7fffffff);

	if (hook->is_64bit_jump) {
		if (!hook->attempted_bounce) {
			hook->attempted_bounce = true;
			setup_64bit_bounce(hook, &offset);
		}

		if (hook->is_64bit_jump) {
			rehook64(hook);
			return;
		}
	}
#endif

	rehook32(hook, force, offset);
}

void unhook(struct func_hook *hook)
{
	uintptr_t addr;
	size_t size;

	if (!hook->hooked)
		return;

	if (hook->type == HOOKTYPE_REVERSE_CHAIN) {
		size = JMP_32_SIZE;
		addr = (hook->func_addr - JMP_32_SIZE);
	} else {
		size = patch_size(hook);
		addr = hook->func_addr;
	}

	fix_permissions((void *)addr, size);
	memcpy(hook->rehook_data, (void *)addr, size);

	if (hook->type == HOOKTYPE_FORWARD_OVERWRITE)
		memcpy((void *)hook->func_addr, hook->unhook_data, size);

	hook->hooked = false;
}
