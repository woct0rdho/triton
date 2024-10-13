#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
// Begin dlfcn-win32 amalgamated

#ifndef DLFCN_H
#define DLFCN_H
#ifdef __cplusplus
extern "C" {
#endif
#if defined(DLFCN_WIN32_SHARED)
#if defined(DLFCN_WIN32_EXPORTS)
#define DLFCN_EXPORT __declspec(dllexport)
#else
#define DLFCN_EXPORT __declspec(dllimport)
#endif
#else
#define DLFCN_EXPORT
#endif
#define RTLD_NOW 0
#define RTLD_LAZY RTLD_NOW
#define RTLD_GLOBAL (1 << 1)
#define RTLD_LOCAL (1 << 2)
#define RTLD_DEFAULT ((void *)0)
#define RTLD_NEXT ((void *)-1)
#define RTLD_NOLOAD (RTLD_LOCAL | RTLD_LAZY)
typedef struct dl_info {
  const char *dli_fname;
  void *dli_fbase;
  const char *dli_sname;
  void *dli_saddr;
} Dl_info;
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#endif
#include <intrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#if _WIN32_WINNT < 0x0500
typedef ULONG ULONG_PTR;
#endif
#ifndef GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 0x4
#endif
#ifndef GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 0x2
#endif
#ifndef IMAGE_NT_OPTIONAL_HDR_MAGIC
#ifdef _WIN64
#define IMAGE_NT_OPTIONAL_HDR_MAGIC 0x20b
#else
#define IMAGE_NT_OPTIONAL_HDR_MAGIC 0x10b
#endif
#endif
#ifndef IMAGE_DIRECTORY_ENTRY_IAT
#define IMAGE_DIRECTORY_ENTRY_IAT 12
#endif
#ifndef LOAD_WITH_ALTERED_SEARCH_PATH
#define LOAD_WITH_ALTERED_SEARCH_PATH 0x8
#endif
#ifdef _MSC_VER
#if _MSC_VER >= 1000
#pragma intrinsic(_ReturnAddress)
#else
__declspec(naked) static void *_ReturnAddress(void) {
  __asm mov eax, [ebp + 4] __asm ret
}
#define _ReturnAddress() (_alloca(1), _ReturnAddress())
#endif
#else
#ifndef _ReturnAddress
#define _ReturnAddress()                                                       \
  (__builtin_extract_return_addr(__builtin_return_address(0)))
#endif
#endif
#ifdef DLFCN_WIN32_SHARED
#define DLFCN_WIN32_EXPORTS
#endif
#if defined(_MSC_VER) && _MSC_VER >= 1300
#define DLFCN_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) &&                                                     \
    ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
#define DLFCN_NOINLINE __attribute__((noinline))
#else
#define DLFCN_NOINLINE
#endif
typedef struct local_object {
  HMODULE hModule;
  struct local_object *previous;
  struct local_object *next;
} local_object;
static local_object first_object;
static local_object *local_search(HMODULE hModule) {
  local_object *pobject;
  if (hModule == NULL)
    return NULL;
  for (pobject = &first_object; pobject; pobject = pobject->next)
    if (pobject->hModule == hModule)
      return pobject;
  return NULL;
}
static BOOL local_add(HMODULE hModule) {
  local_object *pobject;
  local_object *nobject;
  if (hModule == NULL)
    return TRUE;
  pobject = local_search(hModule);
  if (pobject != NULL)
    return TRUE;
  for (pobject = &first_object; pobject->next; pobject = pobject->next)
    ;
  nobject = (local_object *)malloc(sizeof(local_object));
  if (!nobject)
    return FALSE;
  pobject->next = nobject;
  nobject->next = NULL;
  nobject->previous = pobject;
  nobject->hModule = hModule;
  return TRUE;
}
static void local_rem(HMODULE hModule) {
  local_object *pobject;
  if (hModule == NULL)
    return;
  pobject = local_search(hModule);
  if (pobject == NULL)
    return;
  if (pobject->next)
    pobject->next->previous = pobject->previous;
  if (pobject->previous)
    pobject->previous->next = pobject->next;
  free(pobject);
}
static char error_buffer[65535];
static BOOL error_occurred;
static void save_err_str(const char *str, DWORD dwMessageId) {
  DWORD ret;
  size_t pos, len;
  len = strlen(str);
  if (len > sizeof(error_buffer) - 5)
    len = sizeof(error_buffer) - 5;
  pos = 0;
  error_buffer[pos++] = '"';
  memcpy(error_buffer + pos, str, len);
  pos += len;
  error_buffer[pos++] = '"';
  error_buffer[pos++] = ':';
  error_buffer[pos++] = ' ';
  ret = FormatMessageA(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
      dwMessageId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      error_buffer + pos, (DWORD)(sizeof(error_buffer) - pos), NULL);
  pos += ret;
  if (ret == 0)
    error_buffer[pos] = '\0';
  if (pos > 1) {
    if (error_buffer[pos - 2] == '\r' && error_buffer[pos - 1] == '\n')
      error_buffer[pos - 2] = '\0';
  }
  error_occurred = TRUE;
}
static void save_err_ptr_str(const void *ptr, DWORD dwMessageId) {
  char ptr_buf[2 + 2 * sizeof(ptr) + 1];
  char num;
  size_t i;
  ptr_buf[0] = '0';
  ptr_buf[1] = 'x';
  for (i = 0; i < 2 * sizeof(ptr); i++) {
    num = (char)((((ULONG_PTR)ptr) >> (8 * sizeof(ptr) - 4 * (i + 1))) & 0xF);
    ptr_buf[2 + i] = num + ((num < 0xA) ? '0' : ('A' - 0xA));
  }
  ptr_buf[2 + 2 * sizeof(ptr)] = 0;
  save_err_str(ptr_buf, dwMessageId);
}
static UINT MySetErrorMode(UINT uMode) {
  static BOOL(WINAPI * SetThreadErrorModePtr)(DWORD, DWORD *) = NULL;
  static BOOL failed = FALSE;
  HMODULE kernel32;
  DWORD oldMode;
  if (!failed && SetThreadErrorModePtr == NULL) {
    kernel32 = GetModuleHandleA("Kernel32.dll");
    if (kernel32 != NULL)
      SetThreadErrorModePtr = (BOOL(WINAPI *)(DWORD, DWORD *))(
          LPVOID)GetProcAddress(kernel32, "SetThreadErrorMode");
    if (SetThreadErrorModePtr == NULL)
      failed = TRUE;
  }
  if (!failed) {
    if (!SetThreadErrorModePtr(uMode, &oldMode))
      return 0;
    else
      return oldMode;
  } else {
    return SetErrorMode(uMode);
  }
}
static HMODULE MyGetModuleHandleFromAddress(const void *addr) {
  static BOOL(WINAPI * GetModuleHandleExAPtr)(DWORD, LPCSTR, HMODULE *) = NULL;
  static BOOL failed = FALSE;
  HMODULE kernel32;
  HMODULE hModule;
  MEMORY_BASIC_INFORMATION info;
  size_t sLen;
  if (!failed && GetModuleHandleExAPtr == NULL) {
    kernel32 = GetModuleHandleA("Kernel32.dll");
    if (kernel32 != NULL)
      GetModuleHandleExAPtr = (BOOL(WINAPI *)(DWORD, LPCSTR, HMODULE *))(
          LPVOID)GetProcAddress(kernel32, "GetModuleHandleExA");
    if (GetModuleHandleExAPtr == NULL)
      failed = TRUE;
  }
  if (!failed) {
    if (!GetModuleHandleExAPtr(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)addr, &hModule))
      return NULL;
  } else {
    sLen = VirtualQuery(addr, &info, sizeof(info));
    if (sLen != sizeof(info))
      return NULL;
    hModule = (HMODULE)info.AllocationBase;
  }
  return hModule;
}
static BOOL MyEnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb,
                                 LPDWORD lpcbNeeded) {
  static BOOL(WINAPI * EnumProcessModulesPtr)(HANDLE, HMODULE *, DWORD,
                                              LPDWORD) = NULL;
  static BOOL failed = FALSE;
  UINT uMode;
  HMODULE psapi;
  if (failed)
    return FALSE;
  if (EnumProcessModulesPtr == NULL) {
    psapi = GetModuleHandleA("Kernel32.dll");
    if (psapi != NULL)
      EnumProcessModulesPtr =
          (BOOL(WINAPI *)(HANDLE, HMODULE *, DWORD, LPDWORD))(
              LPVOID)GetProcAddress(psapi, "K32EnumProcessModules");
    if (EnumProcessModulesPtr == NULL) {
      uMode = MySetErrorMode(SEM_FAILCRITICALERRORS);
      psapi = LoadLibraryA("Psapi.dll");
      if (psapi != NULL) {
        EnumProcessModulesPtr =
            (BOOL(WINAPI *)(HANDLE, HMODULE *, DWORD, LPDWORD))(
                LPVOID)GetProcAddress(psapi, "EnumProcessModules");
        if (EnumProcessModulesPtr == NULL)
          FreeLibrary(psapi);
      }
      MySetErrorMode(uMode);
    }
    if (EnumProcessModulesPtr == NULL) {
      failed = TRUE;
      return FALSE;
    }
  }
  return EnumProcessModulesPtr(hProcess, lphModule, cb, lpcbNeeded);
}
DLFCN_EXPORT
void *dlopen(const char *file, int mode) {
  HMODULE hModule;
  UINT uMode;
  error_occurred = FALSE;
  uMode = MySetErrorMode(SEM_FAILCRITICALERRORS);
  if (file == NULL) {
    hModule = GetModuleHandle(NULL);
    if (!hModule)
      save_err_str("(null)", GetLastError());
  } else {
    HANDLE hCurrentProc;
    DWORD dwProcModsBefore, dwProcModsAfter;
    char lpFileName[MAX_PATH];
    size_t i, len;
    len = strlen(file);
    if (len >= sizeof(lpFileName)) {
      save_err_str(file, ERROR_FILENAME_EXCED_RANGE);
      hModule = NULL;
    } else {
      for (i = 0; i < len; i++) {
        if (file[i] == '/')
          lpFileName[i] = '\\';
        else
          lpFileName[i] = file[i];
      }
      lpFileName[len] = '\0';
      hCurrentProc = GetCurrentProcess();
      if (MyEnumProcessModules(hCurrentProc, NULL, 0, &dwProcModsBefore) == 0)
        dwProcModsBefore = 0;
      hModule = LoadLibraryExA(lpFileName, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
      if (!hModule) {
        save_err_str(lpFileName, GetLastError());
      } else {
        if (MyEnumProcessModules(hCurrentProc, NULL, 0, &dwProcModsAfter) == 0)
          dwProcModsAfter = 0;
        if ((mode & RTLD_LOCAL) && dwProcModsBefore != dwProcModsAfter) {
          if (!local_add(hModule)) {
            save_err_str(lpFileName, ERROR_NOT_ENOUGH_MEMORY);
            FreeLibrary(hModule);
            hModule = NULL;
          }
        } else if (!(mode & RTLD_LOCAL) &&
                   dwProcModsBefore == dwProcModsAfter) {
          local_rem(hModule);
        }
      }
    }
  }
  MySetErrorMode(uMode);
  return (void *)hModule;
}
DLFCN_EXPORT
int dlclose(void *handle) {
  HMODULE hModule = (HMODULE)handle;
  BOOL ret;
  error_occurred = FALSE;
  ret = FreeLibrary(hModule);
  if (ret)
    local_rem(hModule);
  else
    save_err_ptr_str(handle, GetLastError());
  ret = !ret;
  return (int)ret;
}
DLFCN_NOINLINE
DLFCN_EXPORT void *dlsym(void *handle, const char *name) {
  FARPROC symbol;
  HMODULE hCaller;
  HMODULE hModule;
  DWORD dwMessageId;
  error_occurred = FALSE;
  symbol = NULL;
  hCaller = NULL;
  hModule = GetModuleHandle(NULL);
  dwMessageId = 0;
  if (handle == RTLD_DEFAULT) {
    handle = hModule;
  } else if (handle == RTLD_NEXT) {
    hCaller = MyGetModuleHandleFromAddress(_ReturnAddress());
    if (hCaller == NULL) {
      dwMessageId = ERROR_INVALID_PARAMETER;
      goto end;
    }
  }
  if (handle != RTLD_NEXT) {
    symbol = GetProcAddress((HMODULE)handle, name);
    if (symbol != NULL)
      goto end;
  }
  if (hModule == handle || handle == RTLD_NEXT) {
    HANDLE hCurrentProc;
    HMODULE *modules;
    DWORD cbNeeded;
    DWORD dwSize;
    size_t i;
    hCurrentProc = GetCurrentProcess();
    if (MyEnumProcessModules(hCurrentProc, NULL, 0, &dwSize) != 0) {
      modules = (HMODULE *)malloc(dwSize);
      if (modules) {
        if (MyEnumProcessModules(hCurrentProc, modules, dwSize, &cbNeeded) !=
                0 &&
            dwSize == cbNeeded) {
          for (i = 0; i < dwSize / sizeof(HMODULE); i++) {
            if (handle == RTLD_NEXT && hCaller) {
              if (hCaller == modules[i])
                hCaller = NULL;
              continue;
            }
            if (local_search(modules[i]))
              continue;
            symbol = GetProcAddress(modules[i], name);
            if (symbol != NULL) {
              free(modules);
              goto end;
            }
          }
        }
        free(modules);
      } else {
        dwMessageId = ERROR_NOT_ENOUGH_MEMORY;
        goto end;
      }
    }
  }
end:
  if (symbol == NULL) {
    if (!dwMessageId)
      dwMessageId = ERROR_PROC_NOT_FOUND;
    save_err_str(name, dwMessageId);
  }
  return *(void **)(&symbol);
}
DLFCN_EXPORT
char *dlerror(void) {
  if (!error_occurred)
    return NULL;
  error_occurred = FALSE;
  return error_buffer;
}
static BOOL get_image_section(HMODULE module, int index, void **ptr,
                              DWORD *size) {
  IMAGE_DOS_HEADER *dosHeader;
  IMAGE_NT_HEADERS *ntHeaders;
  IMAGE_OPTIONAL_HEADER *optionalHeader;
  dosHeader = (IMAGE_DOS_HEADER *)module;
  if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    return FALSE;
  ntHeaders = (IMAGE_NT_HEADERS *)((BYTE *)dosHeader + dosHeader->e_lfanew);
  if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
    return FALSE;
  optionalHeader = &ntHeaders->OptionalHeader;
  if (optionalHeader->Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC)
    return FALSE;
  if (index < 0 || index >= IMAGE_NUMBEROF_DIRECTORY_ENTRIES ||
      index >= optionalHeader->NumberOfRvaAndSizes)
    return FALSE;
  if (optionalHeader->DataDirectory[index].Size == 0 ||
      optionalHeader->DataDirectory[index].VirtualAddress == 0)
    return FALSE;
  if (size != NULL)
    *size = optionalHeader->DataDirectory[index].Size;
  *ptr = (void *)((BYTE *)module +
                  optionalHeader->DataDirectory[index].VirtualAddress);
  return TRUE;
}
static const char *get_export_symbol_name(HMODULE module,
                                          IMAGE_EXPORT_DIRECTORY *ied,
                                          const void *addr,
                                          void **func_address) {
  DWORD i;
  void *candidateAddr = NULL;
  int candidateIndex = -1;
  BYTE *base = (BYTE *)module;
  DWORD *functionAddressesOffsets =
      (DWORD *)(base + (DWORD)ied->AddressOfFunctions);
  DWORD *functionNamesOffsets = (DWORD *)(base + (DWORD)ied->AddressOfNames);
  USHORT *functionNameOrdinalsIndexes =
      (USHORT *)(base + (DWORD)ied->AddressOfNameOrdinals);
  for (i = 0; i < ied->NumberOfFunctions; i++) {
    if ((void *)(base + functionAddressesOffsets[i]) > addr ||
        candidateAddr >= (void *)(base + functionAddressesOffsets[i]))
      continue;
    candidateAddr = (void *)(base + functionAddressesOffsets[i]);
    candidateIndex = i;
  }
  if (candidateIndex == -1)
    return NULL;
  *func_address = candidateAddr;
  for (i = 0; i < ied->NumberOfNames; i++) {
    if (functionNameOrdinalsIndexes[i] == candidateIndex)
      return (const char *)(base + functionNamesOffsets[i]);
  }
  return NULL;
}
static BOOL is_valid_address(const void *addr) {
  MEMORY_BASIC_INFORMATION info;
  size_t result;
  if (addr == NULL)
    return FALSE;
  result = VirtualQuery(addr, &info, sizeof(info));
  if (result == 0 || info.AllocationBase == NULL ||
      info.AllocationProtect == 0 || info.AllocationProtect == PAGE_NOACCESS)
    return FALSE;
  return TRUE;
}
#if defined(_M_ARM64) || defined(__aarch64__)
static INT64 sign_extend(UINT64 value, UINT bits) {
  const UINT left = 64 - bits;
  const INT64 m1 = -1;
  const INT64 wide = (INT64)(value << left);
  const INT64 sign = (wide < 0) ? (m1 << left) : 0;
  return value | sign;
}
#endif
static BOOL is_import_thunk(const void *addr) {
#if defined(_M_ARM64) || defined(__aarch64__)
  ULONG opCode1 = *(ULONG *)((BYTE *)addr);
  ULONG opCode2 = *(ULONG *)((BYTE *)addr + 4);
  ULONG opCode3 = *(ULONG *)((BYTE *)addr + 8);
  return (opCode1 & 0x9f00001f) == 0x90000010 &&
                 (opCode2 & 0xffe003ff) == 0xf9400210 && opCode3 == 0xd61f0200
             ? TRUE
             : FALSE;
#else
  return *(short *)addr == 0x25ff ? TRUE : FALSE;
#endif
}
static void *get_address_from_import_address_table(void *iat, DWORD iat_size,
                                                   const void *addr) {
  BYTE *thkp = (BYTE *)addr;
#if defined(_M_ARM64) || defined(__aarch64__)
  ULONG opCode1 = *(ULONG *)((BYTE *)addr);
  ULONG opCode2 = *(ULONG *)((BYTE *)addr + 4);
  UINT64 pageLow2 = (opCode1 >> 29) & 3;
  UINT64 pageHigh19 = (opCode1 >> 5) & ~(~0ull << 19);
  INT64 page = sign_extend((pageHigh19 << 2) | pageLow2, 21) << 12;
  UINT64 offset = ((opCode2 >> 10) & ~(~0ull << 12)) << 3;
  BYTE *ptr = (BYTE *)((ULONG64)thkp & ~0xfffull) + page + offset;
#else
  ULONG offset = *(ULONG *)(thkp + 2);
#if defined(_M_AMD64) || defined(__x86_64__)
  BYTE *ptr = (BYTE *)(thkp + 6 + (LONG)offset);
#else
  BYTE *ptr = (BYTE *)offset;
#endif
#endif
  if (!is_valid_address(ptr) || ptr < (BYTE *)iat ||
      ptr > (BYTE *)iat + iat_size)
    return NULL;
  return *(void **)ptr;
}
static char module_filename[2 * MAX_PATH];
static BOOL fill_info(const void *addr, Dl_info *info) {
  HMODULE hModule;
  DWORD dwSize;
  IMAGE_EXPORT_DIRECTORY *ied;
  void *funcAddress = NULL;
  hModule = MyGetModuleHandleFromAddress(addr);
  if (hModule == NULL)
    return FALSE;
  dwSize =
      GetModuleFileNameA(hModule, module_filename, sizeof(module_filename));
  if (dwSize == 0 || dwSize == sizeof(module_filename))
    return FALSE;
  info->dli_fname = module_filename;
  info->dli_fbase = (void *)hModule;
  if (get_image_section(hModule, IMAGE_DIRECTORY_ENTRY_EXPORT, (void **)&ied,
                        NULL))
    info->dli_sname = get_export_symbol_name(hModule, ied, addr, &funcAddress);
  else
    info->dli_sname = NULL;
  info->dli_saddr = info->dli_sname == NULL ? NULL
                    : funcAddress != NULL   ? funcAddress
                                            : (void *)addr;
  return TRUE;
}
DLFCN_EXPORT
int dladdr(const void *addr, Dl_info *info) {
  if (info == NULL)
    return 0;
  if (!is_valid_address(addr))
    return 0;
  if (is_import_thunk(addr)) {
    void *iat;
    DWORD iatSize;
    HMODULE hModule;
    hModule = MyGetModuleHandleFromAddress(addr);
    if (hModule == NULL)
      return 0;
    if (!get_image_section(hModule, IMAGE_DIRECTORY_ENTRY_IAT, &iat,
                           &iatSize)) {
      IMAGE_IMPORT_DESCRIPTOR *iid;
      DWORD iidSize;
      if (!get_image_section(hModule, IMAGE_DIRECTORY_ENTRY_IMPORT,
                             (void **)&iid, &iidSize))
        return 0;
      if (iid == NULL || iid->Characteristics == 0 || iid->FirstThunk == 0)
        return 0;
      iat = (void *)((BYTE *)hModule + (DWORD)iid->FirstThunk);
      iatSize = iidSize - (DWORD)((BYTE *)iat - (BYTE *)iid);
    }
    addr = get_address_from_import_address_table(iat, iatSize, addr);
    if (!is_valid_address(addr))
      return 0;
  }
  if (!fill_info(addr, info))
    return 0;
  return 1;
}
#ifdef DLFCN_WIN32_SHARED
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  (void)hinstDLL;
  (void)fdwReason;
  (void)lpvReserved;
  return TRUE;
}
#endif
#ifdef __cplusplus
}
#endif
#endif

// End dlfcn-win32 amalgamated
#else
#include <dlfcn.h>
#endif

#include "cuda.h"

#include <stdbool.h>
#define PY_SSIZE_T_CLEAN
#include <Python.h>
// #include <stdatomic.h>

// Raises a Python exception and returns false if code is not CUDA_SUCCESS.
static bool gpuAssert(CUresult code, const char *file, int line) {
  if (code == CUDA_SUCCESS)
    return true;

  const char *prefix = "Triton Error [CUDA]: ";
  const char *str;
  cuGetErrorString(code, &str);
  char err[1024] = {0};
  strcat(err, prefix);
  strcat(err, str);
  PyGILState_STATE gil_state;
  gil_state = PyGILState_Ensure();
  PyErr_SetString(PyExc_RuntimeError, err);
  PyGILState_Release(gil_state);
  return false;
}

// To be used only *outside* a Py_{BEGIN,END}_ALLOW_THREADS block.
#define CUDA_CHECK_AND_RETURN_NULL(ans)                                        \
  do {                                                                         \
    if (!gpuAssert((ans), __FILE__, __LINE__))                                 \
      return NULL;                                                             \
  } while (0)

// To be used inside a Py_{BEGIN,END}_ALLOW_THREADS block.
#define CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(ans)                          \
  do {                                                                         \
    if (!gpuAssert((ans), __FILE__, __LINE__)) {                               \
      PyEval_RestoreThread(_save);                                             \
      return NULL;                                                             \
    }                                                                          \
  } while (0)

// Used to check if functions exist in old CUDA driver versions.
#define INITIALIZE_FUNCTION_POINTER_IF_NULL(funcPointer, initializerFunction)  \
  do {                                                                         \
    if ((funcPointer) == NULL) {                                               \
      (funcPointer) = (initializerFunction)();                                 \
      if ((funcPointer) == NULL) {                                             \
        return NULL;                                                           \
      }                                                                        \
    }                                                                          \
  } while (0)

static PyObject *getDeviceProperties(PyObject *self, PyObject *args) {
  int device_id;
  if (!PyArg_ParseTuple(args, "i", &device_id))
    return NULL;
  // Get device handle
  CUdevice device;
  cuDeviceGet(&device, device_id);

  // create a struct to hold device properties
  int max_shared_mem;
  int max_num_regs;
  int multiprocessor_count;
  int warp_size;
  int sm_clock_rate;
  int mem_clock_rate;
  int mem_bus_width;
  CUDA_CHECK_AND_RETURN_NULL(cuDeviceGetAttribute(
      &max_shared_mem, CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK_OPTIN,
      device));
  CUDA_CHECK_AND_RETURN_NULL(cuDeviceGetAttribute(
      &max_num_regs, CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK, device));
  CUDA_CHECK_AND_RETURN_NULL(cuDeviceGetAttribute(
      &multiprocessor_count, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, device));
  CUDA_CHECK_AND_RETURN_NULL(
      cuDeviceGetAttribute(&warp_size, CU_DEVICE_ATTRIBUTE_WARP_SIZE, device));
  CUDA_CHECK_AND_RETURN_NULL(cuDeviceGetAttribute(
      &sm_clock_rate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, device));
  CUDA_CHECK_AND_RETURN_NULL(cuDeviceGetAttribute(
      &mem_clock_rate, CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE, device));
  CUDA_CHECK_AND_RETURN_NULL(cuDeviceGetAttribute(
      &mem_bus_width, CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH, device));

  return Py_BuildValue("{s:i, s:i, s:i, s:i, s:i, s:i, s:i}", "max_shared_mem",
                       max_shared_mem, "max_num_regs", max_num_regs,
                       "multiprocessor_count", multiprocessor_count, "warpSize",
                       warp_size, "sm_clock_rate", sm_clock_rate,
                       "mem_clock_rate", mem_clock_rate, "mem_bus_width",
                       mem_bus_width);
}

static PyObject *loadBinary(PyObject *self, PyObject *args) {
  const char *name;
  const char *data;
  Py_ssize_t data_size;
  int shared;
  int device;
  if (!PyArg_ParseTuple(args, "ss#ii", &name, &data, &data_size, &shared,
                        &device)) {
    return NULL;
  }
  CUfunction fun;
  CUmodule mod;
  int32_t n_regs = 0;
  int32_t n_spills = 0;
  // create driver handles
  CUcontext pctx = 0;

  Py_BEGIN_ALLOW_THREADS;
  CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(cuCtxGetCurrent(&pctx));
  if (!pctx) {
    CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(
        cuDevicePrimaryCtxRetain(&pctx, device));
    CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(cuCtxSetCurrent(pctx));
  }

  CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(cuModuleLoadData(&mod, data));
  CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(
      cuModuleGetFunction(&fun, mod, name));
  // get allocated registers and spilled registers from the function
  CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(
      cuFuncGetAttribute(&n_regs, CU_FUNC_ATTRIBUTE_NUM_REGS, fun));
  CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(
      cuFuncGetAttribute(&n_spills, CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES, fun));
  n_spills /= 4;
  // set dynamic shared memory if necessary
  int shared_optin;
  CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(cuDeviceGetAttribute(
      &shared_optin, CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK_OPTIN,
      device));
  if (shared > 49152 && shared_optin > 49152) {
    CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(
        cuFuncSetCacheConfig(fun, CU_FUNC_CACHE_PREFER_SHARED));
    int shared_total, shared_static;
    CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(cuDeviceGetAttribute(
        &shared_total, CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_MULTIPROCESSOR,
        device));
    CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(cuFuncGetAttribute(
        &shared_static, CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES, fun));
    CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(
        cuFuncSetAttribute(fun, CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES,
                           shared_optin - shared_static));
  }
  Py_END_ALLOW_THREADS;

  if (PyErr_Occurred()) {
    return NULL;
  }
  return Py_BuildValue("(KKii)", (uint64_t)mod, (uint64_t)fun, n_regs,
                       n_spills);
}

typedef CUresult (*cuOccupancyMaxActiveClusters_t)(
    int *numClusters, CUfunction func, const CUlaunchConfig *config);

typedef CUresult (*cuTensorMapEncodeTiled_t)(
    CUtensorMap *tensorMap, CUtensorMapDataType tensorDataType,
    cuuint32_t tensorRank, void *globalAddress, const cuuint64_t *globalDim,
    const cuuint64_t *globalStrides, const cuuint32_t *boxDim,
    const cuuint32_t *elementStrides, CUtensorMapInterleave interleave,
    CUtensorMapSwizzle swizzle, CUtensorMapL2promotion l2Promotion,
    CUtensorMapFloatOOBfill oobFill);

#define defineGetFunctionHandle(name, symbolName)                              \
  static symbolName##_t name() {                                               \
    /* Open the shared library */                                              \
    void *libHandle = dlopen("libcuda.so.1", RTLD_LAZY);                       \
    if (!libHandle) {                                                          \
      PyErr_SetString(PyExc_RuntimeError, "Failed to open libcuda.so.1");      \
      return NULL;                                                             \
    }                                                                          \
    /* Clear any existing error */                                             \
    dlerror();                                                                 \
    symbolName##_t funcHandle = (symbolName##_t)dlsym(libHandle, #symbolName); \
    /* Check for errors */                                                     \
    const char *err = dlerror();                                               \
    if (err) {                                                                 \
      PyErr_SetString(PyExc_RuntimeError,                                      \
                      "Failed to retrieve " #symbolName " from libcuda.so.1"); \
      dlclose(libHandle);                                                      \
      return NULL;                                                             \
    }                                                                          \
    return funcHandle;                                                         \
  }

defineGetFunctionHandle(getCuOccupancyMaxActiveClustersHandle,
                        cuOccupancyMaxActiveClusters);

defineGetFunctionHandle(getCuTensorMapEncodeTiledHandle,
                        cuTensorMapEncodeTiled);

static PyObject *occupancyMaxActiveClusters(PyObject *self, PyObject *args) {
  int clusterDimX = -1, clusterDimY = -1, clusterDimZ = -1,
      maxActiveClusters = -1;
  int shared = 0;
  CUfunction func;

  if (!PyArg_ParseTuple(args, "Kiiii", &func, &shared, &clusterDimX,
                        &clusterDimY, &clusterDimZ)) {
    return NULL;
  }

  // Let each SM have one block
  int maxActiveBlocks = 1;
  Py_BEGIN_ALLOW_THREADS;
  CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(cuFuncSetAttribute(
      func, CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, shared));
  Py_END_ALLOW_THREADS;

  CUlaunchAttribute launchAttr[1];
  launchAttr[0].id = CU_LAUNCH_ATTRIBUTE_CLUSTER_DIMENSION;
  launchAttr[0].value.clusterDim.x = clusterDimX;
  launchAttr[0].value.clusterDim.y = clusterDimY;
  launchAttr[0].value.clusterDim.z = clusterDimZ;
  CUlaunchConfig config;
  config.gridDimX = clusterDimX;
  config.gridDimY = maxActiveBlocks * clusterDimY;
  config.gridDimZ = clusterDimZ;
  config.blockDimX = 128;
  config.blockDimY = 1;
  config.blockDimZ = 1;
  config.sharedMemBytes = shared;
  config.hStream = 0;
  config.numAttrs = 1;
  config.attrs = launchAttr;

  static cuOccupancyMaxActiveClusters_t cuOccupancyMaxActiveClusters = NULL;
  INITIALIZE_FUNCTION_POINTER_IF_NULL(cuOccupancyMaxActiveClusters,
                                      getCuOccupancyMaxActiveClustersHandle);

  Py_BEGIN_ALLOW_THREADS;
  CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(cuFuncSetAttribute(
      func, CU_FUNC_ATTRIBUTE_NON_PORTABLE_CLUSTER_SIZE_ALLOWED, 1));
  CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(
      cuOccupancyMaxActiveClusters(&maxActiveClusters, func, &config));
  Py_END_ALLOW_THREADS;
  return PyLong_FromLong(maxActiveClusters);
}

static PyObject *setPrintfFifoSize(PyObject *self, PyObject *args) {
  long size;
  if (!PyArg_ParseTuple(args, "l", &size)) {
    return NULL;
  }
  if (size < 0) {
    PyErr_SetString(PyExc_ValueError, "fifo size must be non-negative");
    return NULL;
  }

  Py_BEGIN_ALLOW_THREADS;

  // Ensure we have an active context.
  CUcontext ctx = NULL;
  CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(cuCtxGetCurrent(&ctx));
  if (!ctx) {
    CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(
        cuDevicePrimaryCtxRetain(&ctx, /*device=*/0));
    CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(cuCtxSetCurrent(ctx));
  }

  // We can't set the fifo size after running a kernel that calls printf.  This
  // is true even if the set() call is a nop and the new size is the same as the
  // old size.
  //
  // This is unfriendly, so check if the old size matches the new size, and skip
  // the set() call if so.
  size_t oldSize = 0;
  CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(
      cuCtxGetLimit(&oldSize, CU_LIMIT_PRINTF_FIFO_SIZE));
  if (oldSize != size) {
    CUDA_CHECK_AND_RETURN_NULL_ALLOW_THREADS(
        cuCtxSetLimit(CU_LIMIT_PRINTF_FIFO_SIZE, size));
  }

  Py_END_ALLOW_THREADS;
  Py_INCREF(Py_None);
  return Py_None;
}

// Simple helper to experiment creating TMA descriptors on the host.
// This is a useful to test TMA operations independently.
static PyObject *fill1DTMADescriptor(PyObject *self, PyObject *args) {
  unsigned long long global_address;
  uint64_t dim;
  uint32_t tensorDim;
  int elementSize;
  unsigned long long desc_address;
  if (!PyArg_ParseTuple(args, "KKiiK", &global_address, &dim, &tensorDim,
                        &elementSize, &desc_address)) {
    return NULL;
  }
  uint64_t dims[1] = {dim};
  uint64_t globalStrides[1] = {dim * elementSize};
  uint32_t boxDim[1] = {tensorDim};
  uint32_t elementStrides[1] = {1};
  CUtensorMapDataType type;
  switch (elementSize) {
  case 1:
    type = CU_TENSOR_MAP_DATA_TYPE_UINT8;
    break;
  case 2:
    type = CU_TENSOR_MAP_DATA_TYPE_UINT16;
    break;
  case 4:
    type = CU_TENSOR_MAP_DATA_TYPE_UINT32;
    break;
  default:
    PyErr_SetString(PyExc_ValueError, "elementSize must be 1, 2, or 4");
    return NULL;
  }
  assert((elementSize * tensorDim) >= 32 && "block size too small.");
  int rank = 1;
  static cuTensorMapEncodeTiled_t cuTensorMapEncodeTiled = NULL;
  INITIALIZE_FUNCTION_POINTER_IF_NULL(cuTensorMapEncodeTiled,
                                      getCuTensorMapEncodeTiledHandle);
  CUDA_CHECK_AND_RETURN_NULL(cuTensorMapEncodeTiled(
      (CUtensorMap *)desc_address, type, rank, (void *)global_address, dims,
      globalStrides, boxDim, elementStrides, CU_TENSOR_MAP_INTERLEAVE_NONE,
      CU_TENSOR_MAP_SWIZZLE_NONE, CU_TENSOR_MAP_L2_PROMOTION_NONE,
      CU_TENSOR_MAP_FLOAT_OOB_FILL_NONE));
  Py_INCREF(Py_None);
  return Py_None;
}

// Simple helper to experiment creating TMA descriptors on the host.
// This is a useful to test TMA operations independently.
static PyObject *fill2DTMADescriptor(PyObject *self, PyObject *args) {
  unsigned long long global_address;
  uint64_t dims[2];
  uint32_t tensorDims[2];
  int elementSize;
  unsigned long long desc_address;
  if (!PyArg_ParseTuple(args, "KKKiiiK", &global_address, &dims[1], &dims[0],
                        &tensorDims[1], &tensorDims[0], &elementSize,
                        &desc_address)) {
    return NULL;
  }
  uint64_t globalStrides[2] = {dims[0] * elementSize,
                               dims[0] * dims[1] * elementSize};
  uint32_t elementStrides[2] = {1, 1};
  CUtensorMapDataType type;
  switch (elementSize) {
  case 1:
    type = CU_TENSOR_MAP_DATA_TYPE_UINT8;
    break;
  case 2:
    type = CU_TENSOR_MAP_DATA_TYPE_UINT16;
    break;
  case 4:
    type = CU_TENSOR_MAP_DATA_TYPE_UINT32;
    break;
  default:
    PyErr_SetString(PyExc_ValueError, "elementSize must be 1, 2, or 4");
  }
  int rank = 2;
  // Swizzling should be picked in codegen but since we need to set it on the
  // descriptor we rely on a convention between this function and codegen.
  CUtensorMapSwizzle swizzle = CU_TENSOR_MAP_SWIZZLE_128B;
  uint32_t contigDimSizeInByte = elementSize * tensorDims[0];
  if (contigDimSizeInByte >= 128) {
    swizzle = CU_TENSOR_MAP_SWIZZLE_128B;
  } else if (contigDimSizeInByte >= 64) {
    swizzle = CU_TENSOR_MAP_SWIZZLE_64B;
  } else if (contigDimSizeInByte >= 32) {
    swizzle = CU_TENSOR_MAP_SWIZZLE_32B;
  } else {
    assert(false && "block size too small.");
  }
  // The bounding box inner dimension must be less than or equal to the swizzle
  // size.
  // https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__TENSOR__MEMORY.html#group__CUDA__TENSOR__MEMORY_1ga7c7d2aaac9e49294304e755e6f341d7
  // We clamp the block size and the codegen will emit multiple copy operations.
  if (contigDimSizeInByte > 128) {
    tensorDims[0] = 128 / elementSize;
  }
  static cuTensorMapEncodeTiled_t cuTensorMapEncodeTiled = NULL;
  INITIALIZE_FUNCTION_POINTER_IF_NULL(cuTensorMapEncodeTiled,
                                      getCuTensorMapEncodeTiledHandle);
  CUDA_CHECK_AND_RETURN_NULL(cuTensorMapEncodeTiled(
      (CUtensorMap *)desc_address, type, rank, (void *)global_address, dims,
      globalStrides, tensorDims, elementStrides, CU_TENSOR_MAP_INTERLEAVE_NONE,
      swizzle, CU_TENSOR_MAP_L2_PROMOTION_L2_128B,
      CU_TENSOR_MAP_FLOAT_OOB_FILL_NONE));
  Py_INCREF(Py_None);
  return Py_None;
}

static PyMethodDef ModuleMethods[] = {
    {"load_binary", loadBinary, METH_VARARGS,
     "Load provided cubin into CUDA driver"},
    {"get_device_properties", getDeviceProperties, METH_VARARGS,
     "Get the properties for a given device"},
    {"cuOccupancyMaxActiveClusters", occupancyMaxActiveClusters, METH_VARARGS,
     "Python interface for cuOccupancyMaxActiveClusters function"},
    {"set_printf_fifo_size", setPrintfFifoSize, METH_VARARGS,
     "Python interface for cuCtxSetLimit(CU_LIMIT_PRINTF_FIFO_SIZE, x), which "
     "controls how many bytes can be streamed from kernels before data starts "
     "being dropped.  This inherits all the limitations of this call; in "
     "particular it's an error to change this value after launching any kernel "
     "that calls printf()."},
    {"fill_1d_tma_descriptor", fill1DTMADescriptor, METH_VARARGS, "doc"},
    {"fill_2d_tma_descriptor", fill2DTMADescriptor, METH_VARARGS, "doc"},

    {NULL, NULL, 0, NULL} // sentinel
};

static struct PyModuleDef ModuleDef = {PyModuleDef_HEAD_INIT, "cuda_utils",
                                       NULL, // documentation
                                       -1,   // size
                                       ModuleMethods};

PyMODINIT_FUNC PyInit_cuda_utils(void) {
  PyObject *m = PyModule_Create(&ModuleDef);
  if (m == NULL) {
    return NULL;
  }

  PyModule_AddFunctions(m, ModuleMethods);

  return m;
}
