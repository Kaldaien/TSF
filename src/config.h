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
#ifndef __TSFIX__CONFIG_H__
#define __TSFIX__CONFIG_H__

#include <Windows.h>
#include <string>

extern std::wstring TSFIX_VER_STR;

enum outline_techniques {
  OUTLINE_NAMCO    = 0x00,
  OUTLINE_DURANTE  = 0x01,
  OUTLINE_KALDAIEN = 0x02
};

struct tsf_config_s
{
  struct {
    bool     allow_background  = true;
    int      outline_technique = OUTLINE_KALDAIEN;
    float    postproc_ratio    = 0.5f;
    bool     durante_scissor   = false;
    int      refresh_rate      = 0;

    // D3D9Ex Stuff, since we can...
    bool     allow_flipex      = true;
    int      backbuffers       = 4;

    bool     anamorphic        = false;
  } render;

  struct {
    bool     borderless        = true;
    float    foreground_fps    = 30.0f; // 0.0 = Unlimited
    float    background_fps    = 30.0f;
    bool     center            = true;
    int      x_offset          = 0;
    int      y_offset          = 0;
    bool     fix_taskbar       = false;
  } window;

  struct {
    bool     bypass           = true;
    float    tolerance        = 0.333333f; // 33%
    int      shortest_sleep   = 1;         // 1 ms
  } stutter;

  struct {
    float    default          = 30.0f; // Anything set to 0.0f will assume this
    float    battle           = 0.0f;
    float    city             = 0.0f;
    float    cutscene         = 0.0f;
    float    fmv              = 60.0f;
    float    menu             = 60.0f;
    float    world            = 60.0f;
  } framerate;

  struct {
    bool     cache            = true;
    bool     dump             = false;
    bool     log              = false;
    int      max_cache_in_mib = 1024;
    int      max_decomp_jobs  = 16;
  } textures;

  struct {
    int  num_frames = 0;
    bool shaders    = false;
    bool ui         = false; // General-purpose UI stuff
    bool hud        = false;
    bool menus      = false;
    bool minimap    = false;
  } trace;

  struct {
    bool block_left_alt  = false;
    bool block_left_ctrl = false;
    bool block_windows   = true;
    bool block_all_keys  = false;
    bool fix_alt_tab     = true;
  } input;

  struct {
    std::wstring
            version            = TSFIX_VER_STR;
    std::wstring
            injector           = L"d3d9.dll";
  } system;
};

extern tsf_config_s config;

bool TSFix_LoadConfig (std::wstring name         = L"tsfix");
void TSFix_SaveConfig (std::wstring name         = L"tsfix",
                       bool         close_config = false);

#endif /* __TSFIX__CONFIG_H__ */