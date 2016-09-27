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
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>

#include <cstdint>
#include <cstring>

#include "log.h"

void
TSFix_PatchZelosAchievement (void)
{
#if 0
  extern void* TSF_Scan (uint8_t* pattern, size_t len, uint8_t* mask);

  char* str = 
    (char *)TSF_Scan ( (uint8_t *)"TROPHY_ID_ZELOSZ_TITLE_COMPLET",
                         strlen ("TROPHY_ID_ZELOSZ_TITLE_COMPLET")+1,
                           nullptr );

  if (str != nullptr) {
    dll_log->Log (L"[  Achiev  ] Fixing Harem Master... addr=%ph", str);
    strcpy (str, "TROPHY_ID_ZELOSZ_TITLE_COMPLETE");
  } else {
    dll_log->Log (L"[  Achiev  ] Could not locate Harem Master achievement bug", str);
  }
#endif
}