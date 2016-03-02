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
           base_addr = (uint8_t *)mem_info.BaseAddress;//AllocationBase;
  uint8_t* end_addr  = (uint8_t *)mem_info.BaseAddress + mem_info.RegionSize;

  if (base_addr != (uint8_t *)0x400000) {
    dll_log.Log ( L"[ Sig Scan ] Expected module base addr. 40000h, but got: %ph",
                    base_addr );
  }

  int pages = 0;

// Scan up to 32 MiB worth of data
#define PAGE_WALK_LIMIT (base_addr) + (1 << 26)

  //
  // For practical purposes, let's just assume that all valid games have less than 32 MiB of
  //   committed executable image data.
  //
  while (VirtualQuery (end_addr, &mem_info, sizeof mem_info) && end_addr < PAGE_WALK_LIMIT) {
    if (mem_info.Protect & PAGE_NOACCESS || (! (mem_info.Type & MEM_IMAGE)))
      break;

    pages += VirtualQuery (end_addr, &mem_info, sizeof mem_info);

    end_addr = (uint8_t *)mem_info.BaseAddress + mem_info.RegionSize;
  }

  if (end_addr > PAGE_WALK_LIMIT) {
    dll_log.Log ( L"[ Sig Scan ] Module page walk resulted in end addr. out-of-range: %ph",
                    end_addr );
    dll_log.Log ( L"[ Sig Scan ]  >> Restricting to %ph",
                    PAGE_WALK_LIMIT );
    end_addr = PAGE_WALK_LIMIT;
  }

  dll_log.Log ( L"[ Sig Scan ] Module image consists of %lu pages, from %ph to %ph",
                  pages,
                    base_addr,
                      end_addr );
#endif

  uint8_t*  begin = (uint8_t *)base_addr;
  uint8_t*  it    = begin;
  int       idx   = 0;

  while (it < end_addr)
  {
    VirtualQuery (it, &mem_info, sizeof mem_info);

    // Bail-out once we walk into an address range that is not resident, because
    //   it does not belong to the original executable.
    if (mem_info.RegionSize == 0)
      break;

    uint8_t* next_rgn =
     (uint8_t *)mem_info.BaseAddress + mem_info.RegionSize;

    if ( (! (mem_info.Type    & MEM_IMAGE))  ||
         (! (mem_info.State   & MEM_COMMIT)) ||
             mem_info.Protect & PAGE_NOACCESS ) {
      it    = next_rgn;
      idx   = 0;
      begin = it;
      continue;
    }

    // Do not search past the end of the module image!
    if (next_rgn >= end_addr)
      break;

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

BOOL
WINAPI
QueryPerformanceCounter_Detour (
  _Out_ LARGE_INTEGER *lpPerformanceCount
){
  return QueryPerformanceCounter_Original (lpPerformanceCount);
}

typedef BOOL (WINAPI *CreateTimerQueueTimer_pfn)
(
  _Out_    PHANDLE             phNewTimer,
  _In_opt_ HANDLE              TimerQueue,
  _In_     WAITORTIMERCALLBACK Callback,
  _In_opt_ PVOID               Parameter,
  _In_     DWORD               DueTime,
  _In_     DWORD               Period,
  _In_     ULONG               Flags
);

CreateTimerQueueTimer_pfn CreateTimerQueueTimer_Original = nullptr;

BOOL
WINAPI
CreateTimerQueueTimer_Override (
  _Out_    PHANDLE             phNewTimer,
  _In_opt_ HANDLE              TimerQueue,
  _In_     WAITORTIMERCALLBACK Callback,
  _In_opt_ PVOID               Parameter,
  _In_     DWORD               DueTime,
  _In_     DWORD               Period,
  _In_     ULONG               Flags
)
{
  dll_log.Log ( L"[  60 FPS  ][!]CreateTimerQueueTimer (... %lu ms, %lu ms, ...)",
                  DueTime, Period );

  return CreateTimerQueueTimer_Original (phNewTimer, TimerQueue, Callback, Parameter, 4, 4, Flags);
}


struct namco_rate_control_s {
  bool   Half               = false;
  bool   Hyper              = false;
  LPVOID NamcoTimerFunc     = nullptr;
  LPVOID NamcoTimerAddr     = (void *)0x5CBF20;

  LPVOID NamcoTimerTickInst = (void *)0x5CBF18;
  LPVOID NamcoTimerTickOrig = nullptr;
} game_speed;

DWORD* dwFrameCount  = (DWORD *)0x008A6050;
DWORD* dwFrameCount2 = (DWORD *)0x008A6058;

void
__declspec (naked)
NamcoTick_ (void)
{
  __asm {
    pushad
  }

  if (game_speed.Half) {
    static int frame = 0;
    //if (frame++ % 2) {
      *dwFrameCount   = *dwFrameCount  + 1;
      *dwFrameCount2  = *dwFrameCount2 + 1;
    //}
  }

  __asm {
    popad
    jmp game_speed.NamcoTimerTickOrig
  }
}

void
__cdecl
NamcoTimer_Imp (void)
{
  __asm {
    call game_speed.NamcoTimerFunc
  }
}

void
__cdecl
NamcoUnknown_Detour (void)
{
  if (! game_speed.Hyper) {
    if (! game_speed.Half) {
      NamcoTimer_Imp ();
    }
    else {
      NamcoTimer_Imp ();
      //NamcoTimer_Imp ();
    }
  }
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
  TSFix_CreateDLLHook ( L"kernel32.dll", "CreateTimerQueueTimer",
                          CreateTimerQueueTimer_Override,
               (LPVOID *)&CreateTimerQueueTimer_Original );

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
      dll_log.Log ( L"[  Timing  ] Kernel resolution.: %f ms",
                      (float)(cur * 100)/1000000.0f );
      NtSetTimerResolution   (max, TRUE,  &cur);
      dll_log.Log ( L"[  Timing  ] New resolution....: %f ms",
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
    dll_log.Log ( L"[StutterFix] Scanned Namco Framerate Bug (\"Limiter\"): "
                  L"%ph",
                    pLimiterFunc );

    // We will never be calling the original function, so ... who cares?
    LPVOID lpvDontCare;
    TSFix_CreateFuncHook ( L"NamcoLimiter", pLimiterFunc,
                           NamcoLimiter_Detour,
                           &lpvDontCare );
    TSFix_EnableHook     (pLimiterFunc);

#if 0
    TSFix_CreateFuncHook ( L"NamcoUnknown",
                          game_speed.NamcoTimerAddr,
                          NamcoUnknown_Detour,
                         &game_speed.NamcoTimerFunc );
    TSFix_EnableHook     (game_speed.NamcoTimerAddr);
#endif

#if 0
    TSFix_CreateFuncHook ( L"NamcoFrameCounter",
                          game_speed.NamcoTimerTickInst,
                          NamcoTick_,
                         &game_speed.NamcoTimerTickOrig );
    TSFix_EnableHook     (game_speed.NamcoTimerTickInst);
#endif

    HMODULE hModKernel32 = GetModuleHandle (L"kernel32.dll");

    QueryPerformanceCounter_Original =
      (QueryPerformanceCounter_pfn)
        GetProcAddress (hModKernel32, "QueryPerformanceCounter");
  }

  //
  // Install Stutter Fix
  //
  else {
    TSFix_CreateDLLHook ( L"d3d9.dll", "QueryPerformanceCounter_Detour",
                          QueryPerformanceCounter_Detour, 
               (LPVOID *)&QueryPerformanceCounter_Original );

  }

  eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor        ();

  pCommandProc->AddVariable ("Timing.RouteSixty", new eTB_VarStub <bool>(&game_speed.Half));
  pCommandProc->AddVariable ("Timing.HyperSpeed", new eTB_VarStub <bool>(&game_speed.Hyper));
}

void
tsf::TimingFix::Shutdown (void)
{
  FreeLibrary (NtDll);

  // If we really cared about any of this, we could restore the original
  //   limiter branch instruction here... but it shouldn't matter
}