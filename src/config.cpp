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

#include "config.h"
#include "parameter.h"
#include "ini.h"
#include "log.h"

static
  iSK_INI* 
             dll_ini       = nullptr;
std::wstring TSFIX_VER_STR = L"0.9.11";
tsf_config_s config;

struct {
  tsf::ParameterBool*    allow_background;
  tsf::ParameterInt*     outline_technique;
  tsf::ParameterFloat*   postproc_ratio;
  tsf::ParameterInt*     refresh_rate;

  // D3D9Ex Stuff
  tsf::ParameterBool*    allow_flipex;
  tsf::ParameterInt*     backbuffers;

  tsf::ParameterBool*    anamorphic;
} render;

struct {
  tsf::ParameterBool*    borderless;
  tsf::ParameterFloat*   foreground_fps;
  tsf::ParameterFloat*   background_fps;
  tsf::ParameterBool*    center;
  tsf::ParameterInt*     x_offset;
  tsf::ParameterInt*     y_offset;
  tsf::ParameterBool*    fix_taskbar;
} window;

struct {
  tsf::ParameterBool*    bypass;
  tsf::ParameterFloat*   tolerance;
  //tsf::ParameterInt*     shortest_sleep;
} stutter;

struct {
  tsf::ParameterFloat*   default;
  tsf::ParameterFloat*   battle;
  tsf::ParameterFloat*   city;
  tsf::ParameterFloat*   cutscene;
  tsf::ParameterFloat*   fmv;
  tsf::ParameterFloat*   menu;
  tsf::ParameterFloat*   world;
} framerate;

struct {
  tsf::ParameterBool*    cache;
  tsf::ParameterBool*    dump;
#if 0
  tsf::ParameterStringW* dump_ext;
#endif
  tsf::ParameterBool*    log;
  tsf::ParameterInt*     max_cache_size;
  tsf::ParameterInt*     max_decomp_jobs;
} textures;

struct {
  tsf::ParameterBool*    block_left_alt;
  tsf::ParameterBool*    block_left_ctrl;
  tsf::ParameterBool*    block_windows;
  tsf::ParameterBool*    block_all_keys;
} input;

struct {
  tsf::ParameterStringW* injector;
  tsf::ParameterStringW* version;
} sys;


tsf::ParameterFactory g_ParameterFactory;

typedef const wchar_t* (__stdcall *SK_GetConfigPath_pfn)(void);
static SK_GetConfigPath_pfn SK_GetConfigPath = nullptr;

bool
TSFix_LoadConfig (std::wstring name)
{
  extern HMODULE hInjectorDLL;

  SK_GetConfigPath =
    (SK_GetConfigPath_pfn)
      GetProcAddress (
        hInjectorDLL,
          "SK_GetConfigPath"
      );

  // Load INI File
  wchar_t wszFullName [ MAX_PATH + 2 ] = { L'\0' };

  lstrcatW (wszFullName, SK_GetConfigPath ());
  lstrcatW (wszFullName,       name.c_str ());
  lstrcatW (wszFullName,             L".ini");

  dll_ini = TSF_CreateINI (wszFullName);

  bool empty = dll_ini->get_sections ().empty ();

  //
  // Create Parameters
  //
  render.allow_background =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Allow background rendering")
      );
  render.allow_background->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"AllowBackground" );

  render.outline_technique =
    static_cast <tsf::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"Black Outline Technique")
      );
  render.outline_technique->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"OutlineTechnique" );

  render.postproc_ratio =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Post Processing Scale")
      );
  render.postproc_ratio->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"PostProcessRatio" );

  render.allow_flipex =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Opt-In FlipEx Model (Windows 7+)")
      );
  render.allow_flipex->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"AllowFlipEx" );

  render.backbuffers =
    static_cast <tsf::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"FlipEx Backbuffers")
      );
  render.backbuffers->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"FlipExBuffers" );

  render.refresh_rate =
    static_cast <tsf::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"Refresh Rate Override; 0=Unchanged")
      );
  render.refresh_rate->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"RefreshRate" );

  render.anamorphic =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Anamorphic Aspect Ratio Correction")
      );
  render.anamorphic->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"Anamorphic" );


  window.borderless =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Borderless mode")
      );
  window.borderless->register_to_ini (
    dll_ini,
      L"TSFix.Window",
        L"Borderless" );

  window.foreground_fps =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Foreground Framerate Limit")
      );
  window.foreground_fps->register_to_ini (
    dll_ini,
      L"TSFix.Window",
        L"ForegroundFPS" );

  window.background_fps =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Background Framerate Limit")
      );
  window.background_fps->register_to_ini (
    dll_ini,
      L"TSFix.Window",
        L"BackgroundFPS" );

  window.center =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Center the window")
      );
  window.center->register_to_ini (
    dll_ini,
      L"TSFix.Window",
        L"Center" );

  window.x_offset =
    static_cast <tsf::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"X Offset Coordinate")
      );
  window.x_offset->register_to_ini (
    dll_ini,
      L"TSFix.Window",
        L"XOffset" );

  window.fix_taskbar =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Fix Taskbar Problems When AllowBackground is Enabled")
      );
  window.fix_taskbar->register_to_ini (
    dll_ini,
      L"TSFix.Window",
        L"FixTaskbar" );

  window.y_offset =
    static_cast <tsf::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"Y Offset Coordinate")
      );
  window.y_offset->register_to_ini (
    dll_ini,
      L"TSFix.Window",
        L"YOffset" );


  stutter.bypass =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Bypass Namco's Limiter")
      );
  stutter.bypass->register_to_ini (
    dll_ini,
      L"TSFix.Stutter",
        L"Bypass" );

  stutter.tolerance =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Framerate Limiter Variance")
      );
  stutter.tolerance->register_to_ini (
    dll_ini,
      L"TSFix.Stutter",
        L"Tolerance" );


  framerate.default =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Default Framerate (for any limit set to 0.0)")
      );
  framerate.default->register_to_ini (
    dll_ini,
      L"TSFix.FrameRate",
        L"Default" );

  framerate.battle =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Framerate in Battle")
      );
  framerate.battle->register_to_ini (
    dll_ini,
      L"TSFix.FrameRate",
        L"Battle" );

  framerate.city =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Framerate in Cities")
      );
  framerate.city->register_to_ini (
    dll_ini,
      L"TSFix.FrameRate",
        L"City" );

  framerate.cutscene =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Framerate in Cutscenes")
      );
  framerate.cutscene->register_to_ini (
    dll_ini,
      L"TSFix.FrameRate",
        L"Cutscene" );

  framerate.fmv =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Framerate in Videos")
      );
  framerate.fmv->register_to_ini (
    dll_ini,
      L"TSFix.FrameRate",
        L"FMV" );

  framerate.menu =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Framerate in Menus")
      );
  framerate.menu->register_to_ini (
    dll_ini,
      L"TSFix.FrameRate",
        L"Menu" );

  framerate.world =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Framerate in the Overworld")
      );
  framerate.world->register_to_ini (
    dll_ini,
      L"TSFix.FrameRate",
        L"World" );

  textures.cache =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Cache loaded texture data")
      );
  textures.cache->register_to_ini (
    dll_ini,
      L"TSFix.Textures",
        L"Cache" );

  textures.dump =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Dump loaded texture data")
      );
  textures.dump->register_to_ini (
    dll_ini,
      L"TSFix.Textures",
        L"Dump" );

  textures.max_cache_size =
    static_cast <tsf::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"Maximum Size of Texture Cache")
      );
  textures.max_cache_size->register_to_ini (
    dll_ini,
      L"TSFix.Textures",
        L"MaxCacheInMiB" );

  textures.max_decomp_jobs =
    static_cast <tsf::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"Maximum Number of Textures to Decompress Simultaneously")
      );
  textures.max_decomp_jobs->register_to_ini (
    dll_ini,
      L"TSFix.Textures",
        L"MaxDecompressionJobs" );

  textures.log =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Log (cached) texture load activity")
      );
  textures.log->register_to_ini (
    dll_ini,
      L"TSFix.Textures",
        L"Log" );

#if 0
  textures.cleanup =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Cleanup low-res textures")
      );
  textures.cleanup->register_to_ini (
    dll_ini,
      L"TSFix.Textures",
        L"Cleanup" );
#endif

  input.block_left_alt =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Block Left Alt Key")
      );
  input.block_left_alt->register_to_ini (
    dll_ini,
      L"TSFix.Input",
        L"BlockLeftAlt" );

  input.block_left_ctrl =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Block Left Ctrl Key")
      );
  input.block_left_ctrl->register_to_ini (
    dll_ini,
      L"TSFix.Input",
        L"BlockLeftCtrl" );

  input.block_windows =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Block Windows Key")
      );
  input.block_windows->register_to_ini (
    dll_ini,
      L"TSFix.Input",
        L"BlockWindows" );

  input.block_all_keys =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Block All Keys")
      );
  input.block_all_keys->register_to_ini (
    dll_ini,
      L"TSFix.Input",
        L"BlockAllKeys" );


  sys.version =
    static_cast <tsf::ParameterStringW *>
      (g_ParameterFactory.create_parameter <std::wstring> (
        L"Software Version")
      );
  sys.version->register_to_ini (
    dll_ini,
      L"TSFix.System",
        L"Version" );

  sys.injector =
    static_cast <tsf::ParameterStringW *>
      (g_ParameterFactory.create_parameter <std::wstring> (
        L"Injector DLL")
      );
  sys.injector->register_to_ini (
    dll_ini,
      L"TSFix.System",
        L"Injector" );


  //
  // Load Parameters
  //
  render.allow_background->load  (config.render.allow_background);
  render.outline_technique->load (config.render.outline_technique);
  render.postproc_ratio->load    (config.render.postproc_ratio);

  render.allow_flipex->load      (config.render.allow_flipex);
  render.backbuffers->load       (config.render.backbuffers);

  render.refresh_rate->load      (config.render.refresh_rate);

  render.anamorphic->load        (config.render.anamorphic);


  window.borderless->load        (config.window.borderless);

  window.foreground_fps->load    (config.window.foreground_fps);
  window.background_fps->load    (config.window.background_fps);

  window.center->load            (config.window.center);
  window.x_offset->load          (config.window.x_offset);
  window.y_offset->load          (config.window.y_offset);

  window.fix_taskbar->load       (config.window.fix_taskbar);


  framerate.default->load        (config.framerate.default);
  framerate.battle->load         (config.framerate.battle);
  framerate.city->load           (config.framerate.city);
  framerate.cutscene->load       (config.framerate.cutscene);
  framerate.fmv->load            (config.framerate.fmv);
  framerate.menu->load           (config.framerate.menu);
  framerate.world->load          (config.framerate.world);


  stutter.bypass->load           (config.stutter.bypass);
  stutter.tolerance->load        (config.stutter.tolerance);

  //stutter.shortest_sleep->load (config.stutter.shortest_sleep);


  textures.cache->load           (config.textures.cache);
  textures.dump->load            (config.textures.dump);
  textures.log->load             (config.textures.log);
  textures.max_cache_size->load  (config.textures.max_cache_in_mib);
  textures.max_decomp_jobs->load (config.textures.max_decomp_jobs);


  input.block_left_alt->load     (config.input.block_left_alt);
  input.block_left_ctrl->load    (config.input.block_left_ctrl);
  input.block_windows->load      (config.input.block_windows);
  input.block_all_keys->load     (config.input.block_all_keys);


  sys.version->load              (config.system.version);
  sys.injector->load             (config.system.injector);

  if (empty)
    return false;

  return true;
}

void
TSFix_SaveConfig (std::wstring name, bool close_config) {
  //render.allow_background->store      (config.render.allow_background);

  render.outline_technique->store     (config.render.outline_technique);
  render.postproc_ratio->set_value    (config.render.postproc_ratio);

  render.allow_flipex->store          (config.render.allow_flipex);
  render.backbuffers->store           (config.render.backbuffers);

  render.refresh_rate->store          (config.render.refresh_rate);

  render.anamorphic->store            (config.render.anamorphic);


  window.borderless->store            (config.window.borderless);

  window.foreground_fps->store        (config.window.foreground_fps);
  window.background_fps->store        (config.window.background_fps);

  window.center->store                (config.window.center);
  window.x_offset->store              (config.window.x_offset);
  window.y_offset->store              (config.window.y_offset);

  window.fix_taskbar->store           (config.window.fix_taskbar);


  framerate.default->store            (config.framerate.default);
  framerate.battle->store             (config.framerate.battle);
  framerate.city->store               (config.framerate.city);
  framerate.cutscene->store           (config.framerate.cutscene);
  framerate.fmv->store                (config.framerate.fmv);
  framerate.menu->store               (config.framerate.menu);
  framerate.world->store              (config.framerate.world);


  stutter.bypass->store               (config.stutter.bypass);
  stutter.tolerance->store            (config.stutter.tolerance);

  //stutter.shortest_sleep->store       (config.stutter.shortest_sleep);


  textures.log->store                 (config.textures.log);
  textures.dump->store                (config.textures.dump);

  textures.cache->store               (config.textures.cache);
  textures.max_cache_size->store      (config.textures.max_cache_in_mib);

  textures.max_decomp_jobs->store     (config.textures.max_decomp_jobs);


  input.block_left_alt->store         (config.input.block_left_alt);
  input.block_left_ctrl->store        (config.input.block_left_ctrl);
  input.block_windows->store          (config.input.block_windows);
  input.block_all_keys->store         (config.input.block_all_keys);


  sys.version->store                  (TSFIX_VER_STR);
  sys.injector->store                 (config.system.injector);

  wchar_t wszFullName [ MAX_PATH + 2 ] = { L'\0' };

  lstrcatW (wszFullName, SK_GetConfigPath ());
  lstrcatW (wszFullName,       name.c_str ());
  lstrcatW (wszFullName,             L".ini");

  dll_ini->write (wszFullName);

  if (close_config) {
    if (dll_ini != nullptr) {
      delete dll_ini;
      dll_ini = nullptr;
    }
  }
}