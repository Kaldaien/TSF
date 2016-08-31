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
#ifndef __TSFIX__INPUT_H__
#define __TSFIX__INPUT_H__

#include "command.h"

namespace tsf
{
  namespace InputManager
  {
    void FixAltTab (void);

    void Init     ();
    void Shutdown ();
  }
}

typedef BOOL (WINAPI *ClipCursor_pfn)(
  _In_opt_ const RECT *lpRect
);

typedef BOOL (WINAPI *SetCursorPos_pfn)(
  _In_ int X,
  _In_ int Y
);

extern ClipCursor_pfn   ClipCursor_Original;
extern SetCursorPos_pfn SetCursorPos_Original;

#endif /* __TSFIX__INPUT_H__ */