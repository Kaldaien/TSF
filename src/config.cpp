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
  tsf::INI::File* 
             dll_ini       = nullptr;
std::wstring TSFIX_VER_STR = L"0.6.2";
tsf_config_s config;

struct {
  tsf::ParameterBool*    allow_background;
  tsf::ParameterBool*    borderless;
  tsf::ParameterFloat*   foreground_fps;
  tsf::ParameterFloat*   background_fps;
  tsf::ParameterInt*     outline_technique;
  tsf::ParameterFloat*   postproc_ratio;
  tsf::ParameterInt*     msaa_samples;
  tsf::ParameterInt*     msaa_quality;
  tsf::ParameterBool*    background_msaa;
  tsf::ParameterBool*    remove_blur;
} render;

struct {
  tsf::ParameterBool*    fix;
  tsf::ParameterBool*    bypass;
  tsf::ParameterFloat*   fudge_factor;
  tsf::ParameterFloat*   tolerance;
  tsf::ParameterInt*     shortest_sleep;
} stutter;

struct {
  tsf::ParameterInt*     max_anisotropy;
  tsf::ParameterBool*    cache;
  tsf::ParameterBool*    dump;
#if 0
  tsf::ParameterStringW* dump_ext;
#endif
  tsf::ParameterBool*    optimize_ui;
  tsf::ParameterBool*    log;
  tsf::ParameterBool*    uncompressed;
  tsf::ParameterBool*    full_mipmaps;
#if 0
  tsf::ParameterInt*     mipmap_count;
#endif
  tsf::ParameterBool*    cleanup;
} textures;

struct {
  tsf::ParameterBool*    block_left_alt;
  tsf::ParameterBool*    block_left_ctrl;
  tsf::ParameterBool*    block_windows;
  tsf::ParameterBool*    pause_touch;
  tsf::ParameterBool*    disable_touch;
} input;

struct {
  tsf::ParameterStringW* injector;
  tsf::ParameterStringW* version;
} sys;


tsf::ParameterFactory g_ParameterFactory;


bool
TSFix_LoadConfig (std::wstring name) {
  // Load INI File
  std::wstring full_name = name + L".ini";  
  dll_ini = new tsf::INI::File ((wchar_t *)full_name.c_str ());

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

  render.borderless =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Borderless mode")
      );
  render.borderless->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"BorderlessWindow" );

  render.foreground_fps =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Foreground Framerate Limit")
      );
  render.foreground_fps->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"ForegroundFPS" );

  render.background_fps =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Background Framerate Limit")
      );
  render.background_fps->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"BackgroundFPS" );

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

  render.msaa_samples =
    static_cast <tsf::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"MSAA Sample Count")
      );
  render.msaa_samples->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"MSAA_Samples" );

  render.msaa_quality =
    static_cast <tsf::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"MSAA Quality")
      );
  render.msaa_quality->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"MSAA_Quality" );

  render.background_msaa =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"MSAA Disable in BG")
      );
  render.background_msaa->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"BackgroundMSAA" );

  render.remove_blur =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Remove Blur")
      );
  render.remove_blur->register_to_ini (
    dll_ini,
      L"TSFix.Render",
        L"RemoveBlur" );



  stutter.fix =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Fix Stutter")
      );
  stutter.fix->register_to_ini (
    dll_ini,
      L"TSFix.Stutter",
        L"Fix" );

  stutter.bypass =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Bypass Namco's Limiter")
      );
  stutter.bypass->register_to_ini (
    dll_ini,
      L"TSFix.Stutter",
        L"Bypass" );

  stutter.fudge_factor =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Cosmological Constant")
      );
  stutter.fudge_factor->register_to_ini (
    dll_ini,
      L"TSFix.Stutter",
        L"FudgeFactor" );

  stutter.tolerance =
    static_cast <tsf::ParameterFloat *>
      (g_ParameterFactory.create_parameter <float> (
        L"Framerate Limiter Variance")
      );
  stutter.tolerance->register_to_ini (
    dll_ini,
      L"TSFix.Stutter",
        L"Tolerance" );

  stutter.shortest_sleep =
    static_cast <tsf::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"Shortest allowable sleep")
      );
  stutter.shortest_sleep->register_to_ini (
    dll_ini,
      L"TSFix.Stutter",
        L"ShortestSleep" );


  textures.max_anisotropy =
    static_cast <tsf::ParameterInt *>
      (g_ParameterFactory.create_parameter <int> (
        L"Maximum Anisotropy")
      );
  textures.max_anisotropy->register_to_ini (
    dll_ini,
      L"TSFix.Textures",
        L"MaxAnisotropy" );

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

  textures.optimize_ui =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Optimize UI textures")
      );
  textures.optimize_ui->register_to_ini (
    dll_ini,
      L"TSFix.Textures",
        L"OptimizeUI" );

  textures.log =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Log (cached) texture load activity")
      );
  textures.log->register_to_ini (
    dll_ini,
      L"TSFix.Textures",
        L"Log" );

  textures.uncompressed =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Do not compress texture data")
      );
  textures.uncompressed->register_to_ini (
    dll_ini,
      L"TSFix.Textures",
        L"Uncompressed" );

  textures.full_mipmaps =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Generate missing mipmaps")
      );
  textures.full_mipmaps->register_to_ini (
    dll_ini,
      L"TSFix.Textures",
        L"FullMipmaps" );

  textures.cleanup =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Cleanup low-res textures")
      );
  textures.cleanup->register_to_ini (
    dll_ini,
      L"TSFix.Textures",
        L"Cleanup" );


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

  input.disable_touch =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Disable Touch Input Services")
      );
  input.disable_touch->register_to_ini (
    dll_ini,
      L"TSFix.Input",
        L"DisableTouch" );

  input.pause_touch =
    static_cast <tsf::ParameterBool *>
      (g_ParameterFactory.create_parameter <bool> (
        L"Pause Touch Input Services")
      );
  input.pause_touch->register_to_ini (
    dll_ini,
      L"TSFix.Input",
        L"PauseTouch" );


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
  if (render.allow_background->load ())
    config.render.allow_background = render.allow_background->get_value ();

  if (render.borderless->load ())
    config.render.borderless = render.borderless->get_value ();

  if (render.foreground_fps->load ())
    config.render.foreground_fps = render.foreground_fps->get_value ();

  if (render.background_fps->load ())
    config.render.background_fps = render.background_fps->get_value ();

  if (render.outline_technique->load ())
    config.render.outline_technique = render.outline_technique->get_value ();

  if (render.postproc_ratio->load ())
    config.render.postproc_ratio = render.postproc_ratio->get_value ();

  if (render.msaa_samples->load ())
    config.render.msaa_samples = render.msaa_samples->get_value ();

  if (render.msaa_quality->load ())
    config.render.msaa_quality = render.msaa_quality->get_value ();

  if (render.background_msaa->load ())
    config.render.disable_bg_msaa = (! render.background_msaa->get_value ());

  if (render.remove_blur->load ())
    config.render.remove_blur = render.remove_blur->get_value ();


  if (stutter.fix->load ())
    config.stutter.fix = stutter.fix->get_value ();

  if (stutter.bypass->load ())
    config.stutter.bypass = stutter.bypass->get_value ();

  if (stutter.fudge_factor->load ())
    config.stutter.fudge_factor = stutter.fudge_factor->get_value ();

  if (stutter.tolerance->load ())
    config.stutter.tolerance = stutter.tolerance->get_value ();

  if (stutter.shortest_sleep->load ())
    config.stutter.shortest_sleep = stutter.shortest_sleep->get_value ();


  if (textures.max_anisotropy->load ())
    config.textures.max_anisotropy = textures.max_anisotropy->get_value ();

  if (textures.cache->load ())
    config.textures.cache = textures.cache->get_value ();

  if (textures.dump->load ())
    config.textures.dump = textures.dump->get_value ();

  if (textures.full_mipmaps->load ())
    config.textures.full_mipmaps = textures.full_mipmaps->get_value ();

  if (textures.log->load ())
    config.textures.log = textures.log->get_value ();

  if (textures.optimize_ui->load ())
    config.textures.optimize_ui = textures.optimize_ui->get_value ();

  if (textures.uncompressed->load ())
    config.textures.uncompressed = textures.uncompressed->get_value ();

  if (textures.cleanup->load ())
    config.textures.cleanup = textures.cleanup->get_value ();

  // When this option is set, it is essential to force 16x AF on
  if (config.textures.full_mipmaps) {
    config.textures.max_anisotropy = 16;
  }


  if (input.block_left_alt->load ())
    config.input.block_left_alt = input.block_left_alt->get_value ();

  if (input.block_left_ctrl->load ())
    config.input.block_left_ctrl = input.block_left_ctrl->get_value ();

  if (input.block_windows->load ())
    config.input.block_windows = input.block_windows->get_value ();

  if (input.disable_touch->load ())
    config.input.disable_touch = input.disable_touch->get_value ();

  if (input.pause_touch->load ())
    config.input.pause_touch = input.pause_touch->get_value ();


  if (sys.version->load ())
    config.system.version = sys.version->get_value ();

  if (sys.injector->load ())
    config.system.injector = sys.injector->get_value ();

  if (empty)
    return false;

  return true;
}

void
TSFix_SaveConfig (std::wstring name, bool close_config) {
  //render.allow_background->set_value  (config.render.allow_background);
  //render.allow_background->store      ();

  render.borderless->set_value        (config.render.borderless);
  render.borderless->store            ();

  render.foreground_fps->set_value    (config.render.foreground_fps);
  render.foreground_fps->store        ();

  render.background_fps->set_value    (config.render.background_fps);
  render.background_fps->store        ();

  render.outline_technique->set_value (config.render.outline_technique);
  render.outline_technique->store     ();

  render.postproc_ratio->set_value    (config.render.postproc_ratio);
  render.postproc_ratio->store        ();

  render.msaa_samples->set_value      (config.render.msaa_samples);
  render.msaa_samples->store          ();

  render.msaa_quality->set_value      (config.render.msaa_quality);
  render.msaa_quality->store          ();

  render.remove_blur->set_value       (config.render.remove_blur);
  render.remove_blur->store           ();


  stutter.fix->set_value              (config.stutter.fix);
  stutter.fix->store                  ();

  stutter.bypass->set_value           (config.stutter.bypass);
  stutter.bypass->store               ();

  stutter.fudge_factor->set_value     (config.stutter.fudge_factor);
  stutter.fudge_factor->store         ();

  stutter.tolerance->set_value        (config.stutter.tolerance);
  stutter.tolerance->store            ();

  stutter.shortest_sleep->set_value   (config.stutter.shortest_sleep);
  stutter.shortest_sleep->store       ();



  //
  // This gets set dynamically depending on certain settings, don't
  //   save this...
  //
  //textures.max_anisotropy->set_value (config.textures.max_anisotropy);
  //textures.max_anisotropy->store     ();

  textures.cache->set_value          (config.textures.cache);
  textures.cache->store              ();

  textures.dump->set_value           (config.textures.dump);
  textures.dump->store               ();

  textures.log->set_value            (config.textures.log);
  textures.log->store                ();

  textures.optimize_ui->set_value    (config.textures.optimize_ui);
  textures.optimize_ui->store        ();

  textures.uncompressed->set_value   (config.textures.uncompressed);
  textures.uncompressed->store       ();

  textures.full_mipmaps->set_value   (config.textures.full_mipmaps);
  textures.full_mipmaps->store       ();

  textures.cleanup->set_value        (config.textures.cleanup);
  textures.cleanup->store            ();


  input.block_left_alt->set_value  (config.input.block_left_alt);
  input.block_left_alt->store      ();

  input.block_left_ctrl->set_value (config.input.block_left_ctrl);
  input.block_left_ctrl->store     ();

  input.block_windows->set_value   (config.input.block_windows);
  input.block_windows->store       ();

  input.disable_touch->set_value   (config.input.disable_touch);
  input.disable_touch->store       ();

  input.pause_touch->set_value     (config.input.pause_touch);
  input.pause_touch->store         ();


  sys.version->set_value  (TSFIX_VER_STR);
  sys.version->store      ();

  sys.injector->set_value (config.system.injector);
  sys.injector->store     ();

  dll_ini->write (name + L".ini");

  if (close_config) {
    if (dll_ini != nullptr) {
      delete dll_ini;
      dll_ini = nullptr;
    }
  }
}