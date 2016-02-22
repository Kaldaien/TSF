/**
 * This file is part of Tales of Symphonia "Fix".
 *
 * Tales of Symphonia "Fix" is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Tales of Symphonia "Fix" is distributed in the hope that it will be
 * useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tales of Symphonia "Fix".
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/
#include "timing.h"
#include "config.h"
#include "render.h"
#include "hook.h"
#include "log.h"

#include <Windows.h>

void*
TSF_Scan (uint8_t* pattern, size_t len, uint8_t* mask)
{
  uint8_t* base_addr = (uint8_t *)GetModuleHandle (nullptr);

  MEMORY_BASIC_INFORMATION mem_info;
  VirtualQuery (base_addr, &mem_info, sizeof mem_info);

  //
  // VMProtect kills this, so let's do something else...
  //
#ifdef VMPROTECT_IS_NOT_A_FACTOR
  IMAGE_DOS_HEADER* pDOS =
    (IMAGE_DOS_HEADER *)mem_info.AllocationBase;
  IMAGE_NT_HEADERS* pNT  =
    (IMAGE_NT_HEADERS *)((intptr_t)(pDOS + pDOS->e_lfanew));

  uint8_t* end_addr = base_addr + pNT->OptionalHeader.SizeOfImage;
#else
           base_addr = (uint8_t *)mem_info.AllocationBase;
  uint8_t* end_addr  = (uint8_t *)mem_info.BaseAddress + mem_info.RegionSize;

  while (VirtualQuery (end_addr, &mem_info, sizeof mem_info) > 0) {
    if (mem_info.Type != MEM_IMAGE)
      break;

    end_addr = (uint8_t *)mem_info.BaseAddress + mem_info.RegionSize;
  }
#endif

  uint8_t*  begin = (uint8_t *)base_addr;
  uint8_t*  it    = begin;
  int       idx   = 0;

  while (it < end_addr)
  {
    // Bail-out once we walk into an address range that is not resident, because it does not
    //   belong to the original executable.
    if (! VirtualQuery (it, &mem_info, sizeof mem_info))
      break;

    uint8_t* next_rgn = (uint8_t *)mem_info.BaseAddress + mem_info.RegionSize;

    if (mem_info.Type != MEM_IMAGE || mem_info.State != MEM_COMMIT || mem_info.Protect & PAGE_NOACCESS) {
      it    = next_rgn;
      idx   = 0;
      begin = it;
      continue;
    }

    while (it < next_rgn) {
      uint8_t* scan_addr = it;

      bool match = (*scan_addr == pattern [idx]);

      // For portions we do not care about... treat them
      //   as matching.
      if (mask != nullptr && (! mask [idx]))
        match = true;

      if (match) {
        if (++idx == len)
          return (void *)begin;

        ++it;
      }

      else {
        // No match?!
        if (it > end_addr - len)
          break;

        it  = ++begin;
        idx = 0;
      }
    }
  }

  return nullptr;
}

void
TSF_FlushInstructionCache ( LPCVOID base_addr,
                            size_t  code_size )
{
  FlushInstructionCache ( GetCurrentProcess (),
                            base_addr,
                              code_size );
}

void
TSF_InjectMachineCode ( LPVOID   base_addr,
                         uint8_t* new_code,
                         size_t   code_size,
                         DWORD    permissions,
                         uint8_t* old_code = nullptr )
{
  DWORD dwOld;

  if (VirtualProtect (base_addr, code_size, permissions, &dwOld))
  {
    if (old_code != nullptr)
      memcpy (old_code, base_addr, code_size);

    memcpy (base_addr, new_code, code_size);

    VirtualProtect (base_addr, code_size, dwOld, &dwOld);

    TSF_FlushInstructionCache (base_addr, code_size);
  }
}


QueryPerformanceCounter_pfn QueryPerformanceCounter_Original = nullptr;
Sleep_pfn                   Sleep_Original                   = nullptr;

int           last_sleep     =   1;
LARGE_INTEGER last_perfCount = { 0 };

void
WINAPI
Sleep_Detour (DWORD dwMilliseconds)
{
  if (GetCurrentThreadId () == tsf::RenderFix::dwRenderThreadID) {
    last_sleep = dwMilliseconds;
    //return;
  }

  //if (config.framerate.yield_processor && dwMilliseconds == 0)
    //YieldProcessor ();

  if ((! config.stutter.fix)                                     ||
       GetCurrentThreadId () != tsf::RenderFix::dwRenderThreadID ||
      dwMilliseconds         >= config.stutter.shortest_sleep) {
    Sleep_Original (dwMilliseconds);
  } else {
#if 0
    LARGE_INTEGER time;
    LARGE_INTEGER freq;
    QueryPerformanceCounter_Original (&time);
    QueryPerformanceFrequency        (&freq);

    LARGE_INTEGER end;
    end.QuadPart = time.QuadPart + (double)dwMilliseconds * 0.001 * freq.QuadPart;

    while (time.QuadPart < end.QuadPart) {
      QueryPerformanceCounter_Original (&time);
    }
#endif
  }
}

BOOL
WINAPI
QueryPerformanceCounter_Detour (
  _Out_ LARGE_INTEGER *lpPerformanceCount
){
  DWORD dwThreadId = GetCurrentThreadId               ();
  BOOL  ret        = QueryPerformanceCounter_Original (lpPerformanceCount);

  if (last_sleep != 0 || (! config.stutter.fix) ||
     (dwThreadId != tsf::RenderFix::dwRenderThreadID)) {

    if (dwThreadId == tsf::RenderFix::dwRenderThreadID)
      memcpy (&last_perfCount, lpPerformanceCount, sizeof (LARGE_INTEGER));

    return ret;

  } else if (dwThreadId == tsf::RenderFix::dwRenderThreadID) {
    const float fudge_factor = config.stutter.fudge_factor;

    last_sleep = -1;

    // Mess with the numbers slightly to prevent hiccups
    lpPerformanceCount->QuadPart +=
      (lpPerformanceCount->QuadPart - last_perfCount.QuadPart) * 
                        fudge_factor/* * freq.QuadPart*/;
    memcpy (&last_perfCount, lpPerformanceCount, sizeof (LARGE_INTEGER));

    return ret;
  }

  if (dwThreadId == tsf::RenderFix::dwRenderThreadID)
    memcpy (&last_perfCount, lpPerformanceCount, sizeof (LARGE_INTEGER));

  return ret;
}

// Bugger off!
void
NamcoLimiter_Detour (DWORD dwUnknown0, DWORD dwUnknown1, DWORD dwUnknwnown2)
{
#if 0
  static HMODULE hModD3D9 = GetModuleHandle (config.system.injector.c_str ());
  static bool*   trigger_limiter =
    (bool *)GetProcAddress (hModD3D9, "__TSFix_need_wait");

  *trigger_limiter = true;
#endif

  return;
}

typedef LONG NTSTATUS;


typedef NTSTATUS (NTAPI *NtQueryTimerResolution_pfn)
(
  OUT PULONG              MinimumResolution,
  OUT PULONG              MaximumResolution,
  OUT PULONG              CurrentResolution
);

typedef NTSTATUS (NTAPI *NtSetTimerResolution_pfn)
(
  IN  ULONG               DesiredResolution,
  IN  BOOLEAN             SetResolution,
  OUT PULONG              CurrentResolution
);

HMODULE                    NtDll                  = 0;

NtQueryTimerResolution_pfn NtQueryTimerResolution = nullptr;
NtSetTimerResolution_pfn   NtSetTimerResolution   = nullptr;

void
tsf::TimingFix::Init (void)
{
  if (NtDll == 0) {
    NtDll = LoadLibrary (L"ntdll.dll");

    NtQueryTimerResolution =
      (NtQueryTimerResolution_pfn)
        GetProcAddress (NtDll, "NtQueryTimerResolution");

    NtSetTimerResolution =
      (NtSetTimerResolution_pfn)
        GetProcAddress (NtDll, "NtSetTimerResolution");

    if (NtQueryTimerResolution != nullptr &&
        NtSetTimerResolution   != nullptr) {
      ULONG min, max, cur;
      NtQueryTimerResolution (&min, &max, &cur);
      //NtSetTimerResolution   (max, TRUE,  &cur);
      dll_log.Log ( L" [  Timing  ] Kernel resolution: %f ms",
                      (float)(cur * 100)/1000000.0f );
    }
  }
 
  static uint8_t limiter_prolog [] = {
    0x55,
    0x8B, 0xEC,
    0x83, 0xE4, 0xF8,
    0x83, 0xEC, 0x14,
    0x80, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0x53,
    0x56,
    0x57,
    0x0F, 0x84, 0xFF, 0xFF, 0xFF, 0xFF,
    0x83, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF, 0x02,
    0x0F, 0x82, 0xFF, 0xFF, 0xFF, 0xFF
  };

  /*
  55                    - push ebp
  8B EC                 - mov ebp,esp
  83 E4 F8              - and esp,-08 { 248 }
  83 EC 14              - sub esp,14 { 00000014 }
  80 3D -------- 00     - cmp byte ptr [sec*.tmp+1838212],00 { [1] }
  53                    - push ebx
  56                    - push esi
  57                    - push edi
  0F84 --------         - je sec*.tmp+1C9203
  83 3D -------- 02     - cmp dword ptr [sec*.tmp+4A40C4],02 { [00000002] }
  0F82 XXYYZZWW         - jb sec*.tmp+1C92B1
  */

  static uint8_t limiter_prolog_mask [] = {
    0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00
  };

  uint8_t *pLimiterFunc = nullptr;

  if (config.stutter.bypass) {
    pLimiterFunc = 
      (uint8_t *)TSF_Scan ( limiter_prolog,
                              sizeof (limiter_prolog),
                                limiter_prolog_mask );
  }

  //
  // Install Namco Framerate Limiter Bypass
  //
  if (pLimiterFunc != nullptr) {
    dll_log.Log ( L" [StutterFix] Scanned Namco Framerate Bug (\"Limiter\"): "
                  L"%ph",
                    pLimiterFunc );

    // We will never be calling the original function, so ... who cares?
    LPVOID lpvDontCare;
    TSFix_CreateFuncHook ( L"NamcoLimiter", pLimiterFunc,
                           NamcoLimiter_Detour,
                           &lpvDontCare );
    TSFix_EnableHook     (pLimiterFunc);


    HMODULE hModKernel32 = GetModuleHandle (L"kernel32.dll");

    QueryPerformanceCounter_Original =
      (QueryPerformanceCounter_pfn)
        GetProcAddress (hModKernel32, "QueryPerformanceCounter");

    Sleep_Original =
      (Sleep_pfn)
        GetProcAddress (hModKernel32, "Sleep");
  }

  //
  // Install Stutter Fix
  //
  else {
    // Because Bypass Was Unsuccessful
    if (config.stutter.bypass) {
      dll_log.Log ( L" >> Unable to locate Namco's Framerate Bug (\"Limiter\")"
                    L"resorting to \"Stutter Fix\"..." );
      config.stutter.fix = true;
    }

    TSFix_CreateDLLHook ( L"d3d9.dll", "QueryPerformanceCounter_Detour",
                          QueryPerformanceCounter_Detour, 
               (LPVOID *)&QueryPerformanceCounter_Original );

    TSFix_CreateDLLHook ( L"kernel32.dll", "Sleep",
                          Sleep_Detour, 
               (LPVOID *)&Sleep_Original );
  }
}

void
tsf::TimingFix::Shutdown (void)
{
  FreeLibrary (NtDll);

  // If we really cared about any of this, we could restore the original
  //   limiter branch instruction here... but it shouldn't matter
}