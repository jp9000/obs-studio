/******************************************************************************
    Copyright (C) 2015 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <windows.h>
#include <time.h>
#include <dbghelp.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <inttypes.h>

#include "obs-config.h"
#include "util/dstr.h"
#include "util/platform.h"
#include "util/windows/win-version.h"

typedef BOOL(WINAPI *ENUMERATELOADEDMODULES64)(
	HANDLE process,
	PENUMLOADED_MODULES_CALLBACK64 enum_loaded_modules_callback,
	PVOID user_context);
typedef DWORD(WINAPI *SYMSETOPTIONS)(DWORD sym_options);
typedef BOOL(WINAPI *SYMINITIALIZE)(HANDLE process, PCTSTR user_search_path,
				    BOOL invade_process);
typedef BOOL(WINAPI *SYMCLEANUP)(HANDLE process);
typedef BOOL(WINAPI *STACKWALK64)(
	DWORD machine_type, HANDLE process, HANDLE thread,
	LPSTACKFRAME64 stack_frame, PVOID context_record,
	PREAD_PROCESS_MEMORY_ROUTINE64 read_memory_routine,
	PFUNCTION_TABLE_ACCESS_ROUTINE64 function_table_access_routine,
	PGET_MODULE_BASE_ROUTINE64 get_module_base_routine,
	PTRANSLATE_ADDRESS_ROUTINE64 translate_address);
typedef BOOL(WINAPI *SYMREFRESHMODULELIST)(HANDLE process);

typedef PVOID(WINAPI *SYMFUNCTIONTABLEACCESS64)(HANDLE process,
						DWORD64 addr_base);
typedef DWORD64(WINAPI *SYMGETMODULEBASE64)(HANDLE process, DWORD64 addr);
typedef BOOL(WINAPI *SYMFROMADDR)(HANDLE process, DWORD64 address,
				  PDWORD64 displacement, PSYMBOL_INFOW symbol);
typedef BOOL(WINAPI *SYMGETMODULEINFO64)(HANDLE process, DWORD64 addr,
					 PIMAGEHLP_MODULE64 module_info);

typedef DWORD64(WINAPI *SYMLOADMODULE64)(HANDLE process, HANDLE file,
					 PSTR image_name, PSTR module_name,
					 DWORD64 base_of_dll,
					 DWORD size_of_dll);

typedef BOOL(WINAPI *MINIDUMPWRITEDUMP)(
	HANDLE process, DWORD process_id, HANDLE file, MINIDUMP_TYPE dump_type,
	PMINIDUMP_EXCEPTION_INFORMATION exception_param,
	PMINIDUMP_USER_STREAM_INFORMATION user_stream_param,
	PMINIDUMP_CALLBACK_INFORMATION callback_param);

typedef HINSTANCE(WINAPI *SHELLEXECUTEA)(HWND hwnd, LPCTSTR operation,
					 LPCTSTR file, LPCTSTR parameters,
					 LPCTSTR directory, INT show_flags);

typedef HRESULT(WINAPI *GETTHREADDESCRIPTION)(HANDLE thread, PWSTR *desc);

struct stack_trace {
	CONTEXT context;
	DWORD64 instruction_ptr;
	STACKFRAME64 frame;
	DWORD image_type;
};

struct exception_handler_data {
	SYMINITIALIZE sym_initialize;
	SYMCLEANUP sym_cleanup;
	SYMSETOPTIONS sym_set_options;
	SYMFUNCTIONTABLEACCESS64 sym_function_table_access64;
	SYMGETMODULEBASE64 sym_get_module_base64;
	SYMFROMADDR sym_from_addr;
	SYMGETMODULEINFO64 sym_get_module_info64;
	SYMREFRESHMODULELIST sym_refresh_module_list;
	STACKWALK64 stack_walk64;
	ENUMERATELOADEDMODULES64 enumerate_loaded_modules64;
	MINIDUMPWRITEDUMP minidump_write_dump;

	HMODULE dbghelp;
	SYMBOL_INFOW *sym_info;
	PEXCEPTION_POINTERS exception;
	struct win_version_info win_version;
	SYSTEMTIME time_info;
	HANDLE process;

	struct stack_trace main_trace;

	struct dstr str;
	struct dstr cpu_info;
	struct dstr module_name;
	struct dstr module_list;
};

static inline void
exception_handler_data_free(struct exception_handler_data *data)
{
	LocalFree(data->sym_info);
	dstr_free(&data->str);
	dstr_free(&data->cpu_info);
	dstr_free(&data->module_name);
	dstr_free(&data->module_list);
	FreeLibrary(data->dbghelp);
}

static inline void *get_proc(HMODULE module, const char *func)
{
	return (void *)GetProcAddress(module, func);
}

#define GET_DBGHELP_IMPORT(target, str)                      \
	do {                                                 \
		data->target = get_proc(data->dbghelp, str); \
		if (!data->target)                           \
			return false;                        \
	} while (false)

static inline bool get_dbghelp_imports(struct exception_handler_data *data)
{
	data->dbghelp = LoadLibraryW(L"DbgHelp");
	if (!data->dbghelp)
		return false;

	GET_DBGHELP_IMPORT(sym_initialize, "SymInitialize");
	GET_DBGHELP_IMPORT(sym_cleanup, "SymCleanup");
	GET_DBGHELP_IMPORT(sym_set_options, "SymSetOptions");
	GET_DBGHELP_IMPORT(sym_function_table_access64,
			   "SymFunctionTableAccess64");
	GET_DBGHELP_IMPORT(sym_get_module_base64, "SymGetModuleBase64");
	GET_DBGHELP_IMPORT(sym_from_addr, "SymFromAddrW");
	GET_DBGHELP_IMPORT(sym_get_module_info64, "SymGetModuleInfo64");
	GET_DBGHELP_IMPORT(sym_refresh_module_list, "SymRefreshModuleList");
	GET_DBGHELP_IMPORT(stack_walk64, "StackWalk64");
	GET_DBGHELP_IMPORT(enumerate_loaded_modules64,
			   "EnumerateLoadedModulesW64");
	GET_DBGHELP_IMPORT(minidump_write_dump, "MiniDumpWriteDump");

	return true;
}

static inline void init_instruction_data(struct stack_trace *trace)
{
#ifdef _WIN64
	trace->instruction_ptr = trace->context.Rip;
	trace->frame.AddrPC.Offset = trace->instruction_ptr;
	trace->frame.AddrFrame.Offset = trace->context.Rbp;
	trace->frame.AddrStack.Offset = trace->context.Rsp;
	trace->image_type = IMAGE_FILE_MACHINE_AMD64;
#else
	trace->instruction_ptr = trace->context.Eip;
	trace->frame.AddrPC.Offset = trace->instruction_ptr;
	trace->frame.AddrFrame.Offset = trace->context.Ebp;
	trace->frame.AddrStack.Offset = trace->context.Esp;
	trace->image_type = IMAGE_FILE_MACHINE_I386;
#endif

	trace->frame.AddrFrame.Mode = AddrModeFlat;
	trace->frame.AddrPC.Mode = AddrModeFlat;
	trace->frame.AddrStack.Mode = AddrModeFlat;
}

extern bool sym_initialize_called;

static inline void init_sym_info(struct exception_handler_data *data)
{
	data->sym_set_options(SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS |
			      SYMOPT_LOAD_ANYTHING);

	if (!sym_initialize_called)
		data->sym_initialize(data->process, NULL, true);
	else
		data->sym_refresh_module_list(data->process);

	data->sym_info = LocalAlloc(LPTR, sizeof(*data->sym_info) + 256);
	data->sym_info->SizeOfStruct = sizeof(SYMBOL_INFO);
	data->sym_info->MaxNameLen = 256;
}

static inline void init_version_info(struct exception_handler_data *data)
{
	get_win_ver(&data->win_version);
}

#define PROCESSOR_REG_KEY L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"
#define CPU_ERROR "<unable to query>"

static inline void init_cpu_info(struct exception_handler_data *data)
{
	HKEY key;
	LSTATUS status;

	status = RegOpenKeyW(HKEY_LOCAL_MACHINE, PROCESSOR_REG_KEY, &key);
	if (status == ERROR_SUCCESS) {
		wchar_t str[1024];
		DWORD size = 1024;

		status = RegQueryValueExW(key, L"ProcessorNameString", NULL,
					  NULL, (LPBYTE)str, &size);
		if (status == ERROR_SUCCESS)
			dstr_from_wcs(&data->cpu_info, str);
		else
			dstr_copy(&data->cpu_info, CPU_ERROR);
	} else {
		dstr_copy(&data->cpu_info, CPU_ERROR);
	}
}

static BOOL CALLBACK enum_all_modules(PCTSTR module_name, DWORD64 module_base,
				      ULONG module_size,
				      struct exception_handler_data *data)
{
	char name_utf8[MAX_PATH];
	os_wcs_to_utf8(module_name, 0, name_utf8, MAX_PATH);

	if (data->main_trace.instruction_ptr >= module_base &&
	    data->main_trace.instruction_ptr < module_base + module_size) {

		dstr_copy(&data->module_name, name_utf8);
		strlwr(data->module_name.array);
	}

#ifdef _WIN64
	dstr_catf(&data->module_list, "%016" PRIX64 "-%016" PRIX64 " %s\r\n",
		  module_base, module_base + module_size, name_utf8);
#else
	dstr_catf(&data->module_list, "%08" PRIX64 "-%08" PRIX64 " %s\r\n",
		  module_base, module_base + module_size, name_utf8);
#endif
	return true;
}

static inline void init_module_info(struct exception_handler_data *data)
{
	data->enumerate_loaded_modules64(
		data->process, (PENUMLOADED_MODULES_CALLBACK64)enum_all_modules,
		data);
}

extern const char *get_win_release_id();

static inline void write_header(struct exception_handler_data *data)
{
	char date_time[80];
	time_t now = time(0);
	struct tm ts;
	ts = *localtime(&now);
	strftime(date_time, sizeof(date_time), "%Y-%m-%d, %X", &ts);

	const char *obs_bitness;
	if (sizeof(void *) == 8)
		obs_bitness = "64";
	else
		obs_bitness = "32";

	const char *release_id = get_win_release_id();

	dstr_catf(&data->str,
		  "Unhandled exception: %x\r\n"
		  "Date/Time: %s\r\n"
		  "Fault address: %" PRIX64 " (%s)\r\n"
		  "libobs version: " OBS_VERSION " (%s-bit)\r\n"
		  "Windows version: %d.%d build %d (release: %s; revision: %d; "
		  "%s-bit)\r\n"
		  "CPU: %s\r\n\r\n",
		  data->exception->ExceptionRecord->ExceptionCode, date_time,
		  data->main_trace.instruction_ptr, data->module_name.array,
		  obs_bitness, data->win_version.major, data->win_version.minor,
		  data->win_version.build, release_id, data->win_version.revis,
		  is_64_bit_windows() ? "64" : "32", data->cpu_info.array);
}

struct module_info {
	DWORD64 addr;
	char name_utf8[MAX_PATH];
};

static BOOL CALLBACK enum_module(PCTSTR module_name, DWORD64 module_base,
				 ULONG module_size, struct module_info *info)
{
	if (info->addr >= module_base &&
	    info->addr < module_base + module_size) {

		os_wcs_to_utf8(module_name, 0, info->name_utf8, MAX_PATH);
		strlwr(info->name_utf8);
		return false;
	}

	return true;
}

static inline void get_module_name(struct exception_handler_data *data,
				   struct module_info *info)
{
	data->enumerate_loaded_modules64(
		data->process, (PENUMLOADED_MODULES_CALLBACK64)enum_module,
		info);
}

static inline bool walk_stack(struct exception_handler_data *data,
			      HANDLE thread, struct stack_trace *trace)
{
	struct module_info module_info = {0};
	DWORD64 func_offset;
	char sym_name[256];
	char *p;

	bool success = data->stack_walk64(trace->image_type, data->process,
					  thread, &trace->frame,
					  &trace->context, NULL,
					  data->sym_function_table_access64,
					  data->sym_get_module_base64, NULL);
	if (!success)
		return false;

	module_info.addr = trace->frame.AddrPC.Offset;
	get_module_name(data, &module_info);

	if (!!module_info.name_utf8[0]) {
		p = strrchr(module_info.name_utf8, '\\');
		p = p ? (p + 1) : module_info.name_utf8;
	} else {
		strcpy(module_info.name_utf8, "<unknown>");
		p = module_info.name_utf8;
	}

	success = !!data->sym_from_addr(data->process,
					trace->frame.AddrPC.Offset,
					&func_offset, data->sym_info);

	if (success)
		os_wcs_to_utf8(data->sym_info->Name, 0, sym_name, 256);

#ifdef _WIN64
#define SUCCESS_FORMAT                         \
	"%016I64X %016I64X %016I64X %016I64X " \
	"%016I64X %016I64X %s!%s+0x%I64x\r\n"
#define FAIL_FORMAT                            \
	"%016I64X %016I64X %016I64X %016I64X " \
	"%016I64X %016I64X %s!0x%I64x\r\n"
#else
#define SUCCESS_FORMAT                             \
	"%08.8I64X %08.8I64X %08.8I64X %08.8I64X " \
	"%08.8I64X %08.8I64X %s!%s+0x%I64x\r\n"
#define FAIL_FORMAT                                \
	"%08.8I64X %08.8I64X %08.8I64X %08.8I64X " \
	"%08.8I64X %08.8I64X %s!0x%I64x\r\n"

	trace->frame.AddrStack.Offset &= 0xFFFFFFFFF;
	trace->frame.AddrPC.Offset &= 0xFFFFFFFFF;
	trace->frame.Params[0] &= 0xFFFFFFFF;
	trace->frame.Params[1] &= 0xFFFFFFFF;
	trace->frame.Params[2] &= 0xFFFFFFFF;
	trace->frame.Params[3] &= 0xFFFFFFFF;
#endif

	if (success && (data->sym_info->Flags & SYMFLAG_EXPORT) == 0) {
		dstr_catf(&data->str, SUCCESS_FORMAT,
			  trace->frame.AddrStack.Offset,
			  trace->frame.AddrPC.Offset, trace->frame.Params[0],
			  trace->frame.Params[1], trace->frame.Params[2],
			  trace->frame.Params[3], p, sym_name, func_offset);
	} else {
		dstr_catf(&data->str, FAIL_FORMAT,
			  trace->frame.AddrStack.Offset,
			  trace->frame.AddrPC.Offset, trace->frame.Params[0],
			  trace->frame.Params[1], trace->frame.Params[2],
			  trace->frame.Params[3], p,
			  trace->frame.AddrPC.Offset);
	}

	return true;
}

#ifdef _WIN64
#define TRACE_TOP                                             \
	"Stack            EIP              Arg0             " \
	"Arg1             Arg2             Arg3             Address\r\n"
#else
#define TRACE_TOP                     \
	"Stack    EIP      Arg0     " \
	"Arg1     Arg2     Arg3     Address\r\n"
#endif

static inline char *get_thread_name(HANDLE thread)
{
	static GETTHREADDESCRIPTION get_thread_desc = NULL;
	static bool failed = false;

	if (!get_thread_desc) {
		if (failed) {
			return NULL;
		}

		HMODULE k32 = LoadLibraryW(L"kernel32.dll");
		get_thread_desc = (GETTHREADDESCRIPTION)GetProcAddress(
			k32, "GetThreadDescription");
		if (!get_thread_desc) {
			failed = true;
			return NULL;
		}
	}

	wchar_t *w_name;
	HRESULT hr = get_thread_desc(thread, &w_name);
	if (FAILED(hr) || !w_name) {
		return NULL;
	}

	struct dstr name = {0};
	dstr_from_wcs(&name, w_name);
	if (name.len)
		dstr_insert_ch(&name, 0, ' ');
	LocalFree(w_name);

	return name.array;
}

static inline void write_thread_trace(struct exception_handler_data *data,
				      THREADENTRY32 *entry, bool first_thread)
{
	bool crash_thread = entry->th32ThreadID == GetCurrentThreadId();
	struct stack_trace trace = {0};
	struct stack_trace *ptrace;
	HANDLE thread;
	char *thread_name;

	if (first_thread != crash_thread)
		return;

	if (entry->th32OwnerProcessID != GetCurrentProcessId())
		return;

	thread = OpenThread(THREAD_ALL_ACCESS, false, entry->th32ThreadID);
	if (!thread)
		return;

	trace.context.ContextFlags = CONTEXT_ALL;
	GetThreadContext(thread, &trace.context);
	init_instruction_data(&trace);

	thread_name = get_thread_name(thread);

	dstr_catf(&data->str, "\r\nThread %lX:%s%s\r\n" TRACE_TOP,
		  entry->th32ThreadID, thread_name ? thread_name : "",
		  crash_thread ? " (Crashed)" : "");

	bfree(thread_name);

	ptrace = crash_thread ? &data->main_trace : &trace;

	while (walk_stack(data, thread, ptrace))
		;

	CloseHandle(thread);
}

static inline void write_thread_traces(struct exception_handler_data *data)
{
	THREADENTRY32 entry = {0};
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,
						   GetCurrentProcessId());
	bool success;

	if (snapshot == INVALID_HANDLE_VALUE)
		return;

	entry.dwSize = sizeof(entry);

	success = !!Thread32First(snapshot, &entry);
	while (success) {
		write_thread_trace(data, &entry, true);
		success = !!Thread32Next(snapshot, &entry);
	}

	success = !!Thread32First(snapshot, &entry);
	while (success) {
		write_thread_trace(data, &entry, false);
		success = !!Thread32Next(snapshot, &entry);
	}

	CloseHandle(snapshot);
}

static inline void write_module_list(struct exception_handler_data *data)
{
	dstr_cat(&data->str, "\r\nLoaded modules:\r\n");
#ifdef _WIN64
	dstr_cat(&data->str, "Base Address                      Module\r\n");
#else
	dstr_cat(&data->str, "Base Address      Module\r\n");
#endif
	dstr_cat_dstr(&data->str, &data->module_list);
}

/* ------------------------------------------------------------------------- */

static inline void handle_exception(struct exception_handler_data *data,
				    PEXCEPTION_POINTERS exception)
{
	if (!get_dbghelp_imports(data))
		return;

	data->exception = exception;
	data->process = GetCurrentProcess();
	data->main_trace.context = *exception->ContextRecord;
	GetSystemTime(&data->time_info);

	init_sym_info(data);
	init_version_info(data);
	init_cpu_info(data);
	init_instruction_data(&data->main_trace);
	init_module_info(data);

	write_header(data);
	write_thread_traces(data);
	write_module_list(data);
}

static LONG CALLBACK exception_handler(PEXCEPTION_POINTERS exception)
{
	struct exception_handler_data data = {0};
	static bool inside_handler = false;

	/* don't use if a debugger is present */
	if (IsDebuggerPresent())
		return EXCEPTION_CONTINUE_SEARCH;

	if (inside_handler)
		return EXCEPTION_CONTINUE_SEARCH;

	inside_handler = true;

	handle_exception(&data, exception);
	bcrash(data.str.array);
	exception_handler_data_free(&data);

	inside_handler = false;

	return EXCEPTION_CONTINUE_SEARCH;
}

void initialize_crash_handler(void)
{
	static bool initialized = false;

	if (!initialized) {
		SetUnhandledExceptionFilter(exception_handler);
		initialized = true;
	}
}
