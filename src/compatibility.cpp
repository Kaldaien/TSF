#include <Windows.h>

#include "hook.h"
#include "log.h"

typedef HMODULE (WINAPI *LoadLibraryA_t)(LPCSTR  lpFileName);
typedef HMODULE (WINAPI *LoadLibraryW_t)(LPCWSTR lpFileName);

LoadLibraryA_t LoadLibraryA_Original = nullptr;
LoadLibraryW_t LoadLibraryW_Original = nullptr;

extern HMODULE hModSelf;

HMODULE
WINAPI
LoadLibraryA_Detour (LPCSTR lpFileName)
{
  if (lpFileName == nullptr)
    return NULL;

  HMODULE hModEarly = GetModuleHandleA (lpFileName);

  if (strstr (lpFileName, "ltc_help32" ) ||
      strstr (lpFileName, "ltc_game32")) {
    dll_log.Log (L"[Black List] Preventing Raptr's overlay, evil little thing must die!");
    return NULL;
  }

  if (strstr (lpFileName, "PlayClaw")) {
    dll_log.Log (L"[Black List] Incompatible software: PlayClaw disabled");
    return NULL;
  }

  if (strstr (lpFileName, "fraps")) {
    dll_log.Log (L"[Black List] FRAPS is not compatible with this softare");
    return NULL;
  }

  HMODULE hMod = LoadLibraryA_Original (lpFileName);

  if (hModEarly != hMod)
    dll_log.Log (L"[DLL Loader] Game loaded '%hs'", lpFileName);

  return hMod;
}

HMODULE
WINAPI
LoadLibraryW_Detour (LPCWSTR lpFileName)
{
  if (lpFileName == nullptr)
    return NULL;

  HMODULE hModEarly = GetModuleHandleW (lpFileName);

  if (wcsstr (lpFileName, L"ltc_help32") ||
      wcsstr (lpFileName, L"ltc_game32")) {
    dll_log.Log (L"[Black List] Preventing Raptr's overlay, evil little thing must die!");
    return NULL;
  }

  if (wcsstr (lpFileName, L"PlayClaw")) {
    dll_log.Log (L"[Black List] Incompatible software: PlayClaw disabled");
    return NULL;
  }

  if (wcsstr (lpFileName, L"fraps")) {
    dll_log.Log (L"[Black List] FRAPS is not compatible with this softare");
    return NULL;
  }

  HMODULE hMod = LoadLibraryW_Original (lpFileName);

  if (hModEarly != hMod)
    dll_log.Log (L"[DLL Loader] Game loaded '%s'", lpFileName);

  return hMod;
}

void
TSF_InitCompatBlacklist (void)
{
  TSFix_CreateDLLHook ( L"kernel32.dll", "LoadLibraryA",
                        LoadLibraryA_Detour,
              (LPVOID*)&LoadLibraryA_Original );

  TSFix_CreateDLLHook ( L"kernel32.dll", "LoadLibraryW",
                        LoadLibraryW_Detour,
              (LPVOID*)&LoadLibraryW_Original );

}