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

#include <Windows.h>

namespace tsf {
  namespace TimingFix {
    void Init     (void);
    void Shutdown (void);
  }
}

typedef BOOL (WINAPI *QueryPerformanceCounter_pfn)(
  _Out_ LARGE_INTEGER *lpPerformanceCount
);

extern QueryPerformanceCounter_pfn QueryPerformanceCounter_Original;

typedef BOOL (WINAPI *QueryPerformanceFrequency_pfn)(
  _Out_ LARGE_INTEGER *lpPerformanceCount
);

extern QueryPerformanceFrequency_pfn QueryPerformanceFrequency_Original;