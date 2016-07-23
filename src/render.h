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
#ifndef __TSFIX__RENDER_H__
#define __TSFIX__RENDER_H__

#include "command.h"

struct IDirect3DDevice9;
struct IDirect3DSurface9;

#include <Windows.h>

#include <d3d9.h>

namespace tsf
{
  namespace RenderFix
  {
    struct tracer_s {
      bool log   = false;
      int  count = 0;
    } extern tracer;

    struct tsf_draw_states_s {
      bool         has_aniso      = false; // Has he game even once set anisotropy?!
      int          max_aniso      = 4;
      bool         has_msaa       = false;
      bool         use_msaa       = true;  // Allow MSAA toggle via console
                                           //  without changing the swapchain.
      D3DVIEWPORT9 vp             = { 0 };
      bool         outlines       = false; // Drawing outlines?
      bool         postprocessing = false;
      bool         fullscreen     = false;

      DWORD        srcblend       = 0;
      DWORD        dstblend       = 0;
      DWORD        srcalpha       = 0;     // Separate Alpha Blend Eq: Src
      DWORD        dstalpha       = 0;     // Separate Alpha Blend Eq: Dst
      bool         alpha_test     = false; // Test Alpha?
      DWORD        alpha_ref      = 0;     // Value to test.
      bool         zwrite         = false; // Depth Mask

      int          last_vs_vec4   = 0; // Number of vectors in the last call to
                                       //   set vertex shader constant...

      std::pair <IDirect3DTexture9*, IDirect3DTexture9*>
                   blur_proxy; // If non-null, then bypass the blur
                               //   filter via texture indirection.

      int          draws          = 0; // Number of draw calls
      int          frames         = 0;
    } extern draw_state;


    void Init     ();
    void Shutdown ();

    class CommandProcessor : public eTB_iVariableListener {
    public:
      CommandProcessor (void);

      virtual bool OnVarChange (eTB_Variable* var, void* val = NULL);

      static CommandProcessor* getInstance (void)
      {
        if (pCommProc == NULL)
          pCommProc = new CommandProcessor ();

        return pCommProc;
      }

    protected:

    private:
      static CommandProcessor* pCommProc;
    };

    extern uint32_t           width;
    extern uint32_t           height;
    extern bool               fullscreen;

    extern IDirect3DDevice9*  pDevice;
    extern HWND               hWndDevice;

    extern uint32_t           dwRenderThreadID;

    extern HMODULE            d3dx9_43_dll;
    extern HMODULE            user32_dll;
  }
}

typedef HRESULT (STDMETHODCALLTYPE *BeginScene_pfn)(
  IDirect3DDevice9 *This
);

typedef HRESULT (STDMETHODCALLTYPE *EndScene_pfn)(
  IDirect3DDevice9 *This
);

typedef HRESULT (STDMETHODCALLTYPE *SetScissorRect_pfn)
(
        IDirect3DDevice9 *This,
  const RECT             *pRect
);

typedef HRESULT (STDMETHODCALLTYPE *StretchRect_pfn)
  (      IDirect3DDevice9    *This,
         IDirect3DSurface9   *pSourceSurface,
   const RECT                *pSourceRect,
         IDirect3DSurface9   *pDestSurface,
   const RECT                *pDestRect,
         D3DTEXTUREFILTERTYPE Filter
  );

typedef HRESULT (STDMETHODCALLTYPE *SetViewport_pfn)
(
        IDirect3DDevice9* This,
  CONST D3DVIEWPORT9*     pViewport);



typedef HRESULT (STDMETHODCALLTYPE *SetRenderState_pfn)
(
  IDirect3DDevice9*  This,
  D3DRENDERSTATETYPE State,
  DWORD              Value
);

typedef HRESULT (STDMETHODCALLTYPE *SetVertexShaderConstantF_pfn)
(
        IDirect3DDevice9 *This,
        UINT              StartRegister,
  CONST float            *pConstantData,
        UINT              Vector4fCount
);

typedef HRESULT (STDMETHODCALLTYPE *DrawPrimitive_pfn)
(
  IDirect3DDevice9 *This,
  D3DPRIMITIVETYPE  PrimitiveType,
  UINT              StartVertex,
  UINT              PrimitiveCount
);

typedef HRESULT (STDMETHODCALLTYPE *DrawIndexedPrimitive_pfn)
(
  IDirect3DDevice9 *This,
  D3DPRIMITIVETYPE  Type,
  INT               BaseVertexIndex,
  UINT              MinVertexIndex,
  UINT              NumVertices,
  UINT              startIndex,
  UINT              primCount
);

typedef HRESULT (STDMETHODCALLTYPE *DrawPrimitiveUP_pfn)
(
  IDirect3DDevice9 *This,
  D3DPRIMITIVETYPE  PrimitiveType,
  UINT              PrimitiveCount,
  const void       *pVertexStreamZeroData,
  UINT              VertexStreamZeroStride
);

typedef HRESULT (STDMETHODCALLTYPE *DrawIndexedPrimitiveUP_pfn)
(
  IDirect3DDevice9 *This,
  D3DPRIMITIVETYPE  PrimitiveType,
  UINT              MinVertexIndex,
  UINT              NumVertices,
  UINT              PrimitiveCount,
  const void       *pIndexData,
  D3DFORMAT         IndexDataFormat,
  const void       *pVertexStreamZeroData,
  UINT              VertexStreamZeroStride
);

typedef void (STDMETHODCALLTYPE *SK_BeginBufferSwap_pfn)(void);

typedef HRESULT (STDMETHODCALLTYPE *SK_EndBufferSwap_pfn)
(
  HRESULT   hr,
  IUnknown *device
);

typedef D3DPRESENT_PARAMETERS* (__stdcall *SK_SetPresentParamsD3D9_pfn)
(
  IDirect3DDevice9      *pDevice,
  D3DPRESENT_PARAMETERS *pParams
);

typedef HRESULT (STDMETHODCALLTYPE *SetSamplerState_pfn)
(
  IDirect3DDevice9    *This,
  DWORD                Sampler,
  D3DSAMPLERSTATETYPE  Type,
  DWORD                Value
);


//===========================================================================
//
// 16 bit floating point numbers
//
//===========================================================================

#define D3DX_16F_DIG          3                // # of decimal digits of precision
#define D3DX_16F_EPSILON      4.8875809e-4f    // smallest such that 1.0 + epsilon != 1.0
#define D3DX_16F_MANT_DIG     11               // # of bits in mantissa
#define D3DX_16F_MAX          6.550400e+004    // max value
#define D3DX_16F_MAX_10_EXP   4                // max decimal exponent
#define D3DX_16F_MAX_EXP      15               // max binary exponent
#define D3DX_16F_MIN          6.1035156e-5f    // min positive value
#define D3DX_16F_MIN_10_EXP   (-4)             // min decimal exponent
#define D3DX_16F_MIN_EXP      (-14)            // min binary exponent
#define D3DX_16F_RADIX        2                // exponent radix
#define D3DX_16F_ROUNDS       1                // addition rounding: near


typedef struct D3DXFLOAT16
{
public:
    D3DXFLOAT16() {};
    D3DXFLOAT16( FLOAT );
    D3DXFLOAT16( CONST D3DXFLOAT16& );

    // casting
    operator FLOAT ();

    // binary operators
    BOOL operator == ( CONST D3DXFLOAT16& ) const;
    BOOL operator != ( CONST D3DXFLOAT16& ) const;

protected:
    WORD value;
} D3DXFLOAT16, *LPD3DXFLOAT16;


//--------------------------
// 3D Vector
//--------------------------
typedef struct D3DXVECTOR3 : public D3DVECTOR
{
public:
    D3DXVECTOR3() {};
    D3DXVECTOR3( CONST FLOAT * );
    D3DXVECTOR3( CONST D3DVECTOR& );
    D3DXVECTOR3( CONST D3DXFLOAT16 * );
    D3DXVECTOR3( FLOAT x, FLOAT y, FLOAT z );

    // casting
    operator FLOAT* ();
    operator CONST FLOAT* () const;

    // assignment operators
    D3DXVECTOR3& operator += ( CONST D3DXVECTOR3& );
    D3DXVECTOR3& operator -= ( CONST D3DXVECTOR3& );
    D3DXVECTOR3& operator *= ( FLOAT );
    D3DXVECTOR3& operator /= ( FLOAT );

    // unary operators
    D3DXVECTOR3 operator + () const;
    D3DXVECTOR3 operator - () const;

    // binary operators
    D3DXVECTOR3 operator + ( CONST D3DXVECTOR3& ) const;
    D3DXVECTOR3 operator - ( CONST D3DXVECTOR3& ) const;
    D3DXVECTOR3 operator * ( FLOAT ) const;
    D3DXVECTOR3 operator / ( FLOAT ) const;

    friend D3DXVECTOR3 operator * ( FLOAT, CONST struct D3DXVECTOR3& );

    BOOL operator == ( CONST D3DXVECTOR3& ) const;
    BOOL operator != ( CONST D3DXVECTOR3& ) const;

} D3DXVECTOR3, *LPD3DXVECTOR3;

//===========================================================================
//
// Matrices
//
//===========================================================================
typedef struct D3DXMATRIX : public D3DMATRIX
{
public:
    D3DXMATRIX() {};
    D3DXMATRIX( CONST FLOAT * );
    D3DXMATRIX( CONST D3DMATRIX& );
    D3DXMATRIX( CONST D3DXFLOAT16 * );
    D3DXMATRIX( FLOAT _11, FLOAT _12, FLOAT _13, FLOAT _14,
                FLOAT _21, FLOAT _22, FLOAT _23, FLOAT _24,
                FLOAT _31, FLOAT _32, FLOAT _33, FLOAT _34,
                FLOAT _41, FLOAT _42, FLOAT _43, FLOAT _44 );


    // access grants
    FLOAT& operator () ( UINT Row, UINT Col );
    FLOAT  operator () ( UINT Row, UINT Col ) const;

    // casting operators
    operator FLOAT* ();
    operator CONST FLOAT* () const;

    // assignment operators
    D3DXMATRIX& operator *= ( CONST D3DXMATRIX& );
    D3DXMATRIX& operator += ( CONST D3DXMATRIX& );
    D3DXMATRIX& operator -= ( CONST D3DXMATRIX& );
    D3DXMATRIX& operator *= ( FLOAT );
    D3DXMATRIX& operator /= ( FLOAT );

    // unary operators
    D3DXMATRIX operator + () const;
    D3DXMATRIX operator - () const;

    // binary operators
    D3DXMATRIX operator * ( CONST D3DXMATRIX& ) const;
    D3DXMATRIX operator + ( CONST D3DXMATRIX& ) const;
    D3DXMATRIX operator - ( CONST D3DXMATRIX& ) const;
    D3DXMATRIX operator * ( FLOAT ) const;
    D3DXMATRIX operator / ( FLOAT ) const;

    friend D3DXMATRIX operator * ( FLOAT, CONST D3DXMATRIX& );

    BOOL operator == ( CONST D3DXMATRIX& ) const;
    BOOL operator != ( CONST D3DXMATRIX& ) const;

} D3DXMATRIX, *LPD3DXMATRIX;



//////////////////////////////////////////////////////////////////////////////
// D3DXSPRITE flags:
// -----------------
// D3DXSPRITE_DONOTSAVESTATE
//   Specifies device state is not to be saved and restored in Begin/End.
// D3DXSPRITE_DONOTMODIFY_RENDERSTATE
//   Specifies device render state is not to be changed in Begin.  The device
//   is assumed to be in a valid state to draw vertices containing POSITION0, 
//   TEXCOORD0, and COLOR0 data.
// D3DXSPRITE_OBJECTSPACE
//   The WORLD, VIEW, and PROJECTION transforms are NOT modified.  The 
//   transforms currently set to the device are used to transform the sprites 
//   when the batch is drawn (at Flush or End).  If this is not specified, 
//   WORLD, VIEW, and PROJECTION transforms are modified so that sprites are 
//   drawn in screenspace coordinates.
// D3DXSPRITE_BILLBOARD
//   Rotates each sprite about its center so that it is facing the viewer.
// D3DXSPRITE_ALPHABLEND
//   Enables ALPHABLEND(SRCALPHA, INVSRCALPHA) and ALPHATEST(alpha > 0).
//   ID3DXFont expects this to be set when drawing text.
// D3DXSPRITE_SORT_TEXTURE
//   Sprites are sorted by texture prior to drawing.  This is recommended when
//   drawing non-overlapping sprites of uniform depth.  For example, drawing
//   screen-aligned text with ID3DXFont.
// D3DXSPRITE_SORT_DEPTH_FRONTTOBACK
//   Sprites are sorted by depth front-to-back prior to drawing.  This is 
//   recommended when drawing opaque sprites of varying depths.
// D3DXSPRITE_SORT_DEPTH_BACKTOFRONT
//   Sprites are sorted by depth back-to-front prior to drawing.  This is 
//   recommended when drawing transparent sprites of varying depths.
// D3DXSPRITE_DO_NOT_ADDREF_TEXTURE
//   Disables calling AddRef() on every draw, and Release() on Flush() for
//   better performance.
//////////////////////////////////////////////////////////////////////////////

#define D3DXSPRITE_DONOTSAVESTATE               (1 << 0)
#define D3DXSPRITE_DONOTMODIFY_RENDERSTATE      (1 << 1)
#define D3DXSPRITE_OBJECTSPACE                  (1 << 2)
#define D3DXSPRITE_BILLBOARD                    (1 << 3)
#define D3DXSPRITE_ALPHABLEND                   (1 << 4)
#define D3DXSPRITE_SORT_TEXTURE                 (1 << 5)
#define D3DXSPRITE_SORT_DEPTH_FRONTTOBACK       (1 << 6)
#define D3DXSPRITE_SORT_DEPTH_BACKTOFRONT       (1 << 7)
#define D3DXSPRITE_DO_NOT_ADDREF_TEXTURE        (1 << 8)


//////////////////////////////////////////////////////////////////////////////
// ID3DXSprite:
// ------------
// This object intends to provide an easy way to drawing sprites using D3D.
//
// Begin - 
//    Prepares device for drawing sprites.
//
// Draw -
//    Draws a sprite.  Before transformation, the sprite is the size of 
//    SrcRect, with its top-left corner specified by Position.  The color 
//    and alpha channels are modulated by Color.
//
// Flush -
//    Forces all batched sprites to submitted to the device.
//
// End - 
//    Restores device state to how it was when Begin was called.
//
// OnLostDevice, OnResetDevice -
//    Call OnLostDevice() on this object before calling Reset() on the
//    device, so that this object can release any stateblocks and video
//    memory resources.  After Reset(), the call OnResetDevice().
//////////////////////////////////////////////////////////////////////////////

typedef interface ID3DXSprite ID3DXSprite;
typedef interface ID3DXSprite *LPD3DXSPRITE;


// {BA0B762D-7D28-43ec-B9DC-2F84443B0614}
DEFINE_GUID(IID_ID3DXSprite, 
0xba0b762d, 0x7d28, 0x43ec, 0xb9, 0xdc, 0x2f, 0x84, 0x44, 0x3b, 0x6, 0x14);


#undef INTERFACE
#define INTERFACE ID3DXSprite

DECLARE_INTERFACE_(ID3DXSprite, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DXSprite
    STDMETHOD(GetDevice)(THIS_ LPDIRECT3DDEVICE9* ppDevice) PURE;

    STDMETHOD(GetTransform)(THIS_ D3DXMATRIX *pTransform) PURE;
    STDMETHOD(SetTransform)(THIS_ CONST D3DXMATRIX *pTransform) PURE;

    STDMETHOD(SetWorldViewRH)(THIS_ CONST D3DXMATRIX *pWorld, CONST D3DXMATRIX *pView) PURE;
    STDMETHOD(SetWorldViewLH)(THIS_ CONST D3DXMATRIX *pWorld, CONST D3DXMATRIX *pView) PURE;

    STDMETHOD(Begin)(THIS_ DWORD Flags) PURE;
    STDMETHOD(Draw)(THIS_ LPDIRECT3DTEXTURE9 pTexture, CONST RECT *pSrcRect, CONST D3DXVECTOR3 *pCenter, CONST D3DXVECTOR3 *pPosition, D3DCOLOR Color) PURE;
    STDMETHOD(Flush)(THIS) PURE;
    STDMETHOD(End)(THIS) PURE;

    STDMETHOD(OnLostDevice)(THIS) PURE;
    STDMETHOD(OnResetDevice)(THIS) PURE;
};


extern "C" {

HRESULT WINAPI 
    D3DXCreateSprite( 
        LPDIRECT3DDEVICE9   pDevice, 
        LPD3DXSPRITE*       ppSprite);

}

//////////////////////////////////////////////////////////////////////////////
// ID3DXFont:
// ----------
// Font objects contain the textures and resources needed to render a specific 
// font on a specific device.
//
// GetGlyphData -
//    Returns glyph cache data, for a given glyph.
//
// PreloadCharacters/PreloadGlyphs/PreloadText -
//    Preloads glyphs into the glyph cache textures.
//
// DrawText -
//    Draws formatted text on a D3D device.  Some parameters are 
//    surprisingly similar to those of GDI's DrawText function.  See GDI 
//    documentation for a detailed description of these parameters.
//    If pSprite is NULL, an internal sprite object will be used.
//
// OnLostDevice, OnResetDevice -
//    Call OnLostDevice() on this object before calling Reset() on the
//    device, so that this object can release any stateblocks and video
//    memory resources.  After Reset(), the call OnResetDevice().
//////////////////////////////////////////////////////////////////////////////

typedef struct _D3DXFONT_DESCA
{
    INT Height;
    UINT Width;
    UINT Weight;
    UINT MipLevels;
    BOOL Italic;
    BYTE CharSet;
    BYTE OutputPrecision;
    BYTE Quality;
    BYTE PitchAndFamily;
    CHAR FaceName[LF_FACESIZE];

} D3DXFONT_DESCA, *LPD3DXFONT_DESCA;

typedef struct _D3DXFONT_DESCW
{
    INT Height;
    UINT Width;
    UINT Weight;
    UINT MipLevels;
    BOOL Italic;
    BYTE CharSet;
    BYTE OutputPrecision;
    BYTE Quality;
    BYTE PitchAndFamily;
    WCHAR FaceName[LF_FACESIZE];

} D3DXFONT_DESCW, *LPD3DXFONT_DESCW;

#ifdef UNICODE
typedef D3DXFONT_DESCW D3DXFONT_DESC;
typedef LPD3DXFONT_DESCW LPD3DXFONT_DESC;
#else
typedef D3DXFONT_DESCA D3DXFONT_DESC;
typedef LPD3DXFONT_DESCA LPD3DXFONT_DESC;
#endif


typedef interface ID3DXFont ID3DXFont;
typedef interface ID3DXFont *LPD3DXFONT;


// {D79DBB70-5F21-4d36-BBC2-FF525C213CDC}
DEFINE_GUID(IID_ID3DXFont, 
0xd79dbb70, 0x5f21, 0x4d36, 0xbb, 0xc2, 0xff, 0x52, 0x5c, 0x21, 0x3c, 0xdc);


#undef INTERFACE
#define INTERFACE ID3DXFont

DECLARE_INTERFACE_(ID3DXFont, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DXFont
    STDMETHOD(GetDevice)(THIS_ LPDIRECT3DDEVICE9 *ppDevice) PURE;
    STDMETHOD(GetDescA)(THIS_ D3DXFONT_DESCA *pDesc) PURE;
    STDMETHOD(GetDescW)(THIS_ D3DXFONT_DESCW *pDesc) PURE;
    STDMETHOD_(BOOL, GetTextMetricsA)(THIS_ TEXTMETRICA *pTextMetrics) PURE;
    STDMETHOD_(BOOL, GetTextMetricsW)(THIS_ TEXTMETRICW *pTextMetrics) PURE;

    STDMETHOD_(HDC, GetDC)(THIS) PURE;
    STDMETHOD(GetGlyphData)(THIS_ UINT Glyph, LPDIRECT3DTEXTURE9 *ppTexture, RECT *pBlackBox, POINT *pCellInc) PURE;

    STDMETHOD(PreloadCharacters)(THIS_ UINT First, UINT Last) PURE;
    STDMETHOD(PreloadGlyphs)(THIS_ UINT First, UINT Last) PURE;
    STDMETHOD(PreloadTextA)(THIS_ LPCSTR pString, INT Count) PURE;
    STDMETHOD(PreloadTextW)(THIS_ LPCWSTR pString, INT Count) PURE;

    STDMETHOD_(INT, DrawTextA)(THIS_ LPD3DXSPRITE pSprite, LPCSTR pString, INT Count, LPRECT pRect, DWORD Format, D3DCOLOR Color) PURE;
    STDMETHOD_(INT, DrawTextW)(THIS_ LPD3DXSPRITE pSprite, LPCWSTR pString, INT Count, LPRECT pRect, DWORD Format, D3DCOLOR Color) PURE;

    STDMETHOD(OnLostDevice)(THIS) PURE;
    STDMETHOD(OnResetDevice)(THIS) PURE;

#ifdef UNICODE
    HRESULT GetDesc(D3DXFONT_DESCW *pDesc) { return GetDescW(pDesc); }
    HRESULT PreloadText(LPCWSTR pString, INT Count) { return PreloadTextW(pString, Count); }
#else
    HRESULT GetDesc(D3DXFONT_DESCA *pDesc) { return GetDescA(pDesc); }
    HRESULT PreloadText(LPCSTR pString, INT Count) { return PreloadTextA(pString, Count); }
#endif
};

#ifndef GetTextMetrics
#ifdef UNICODE
#define GetTextMetrics GetTextMetricsW
#else
#define GetTextMetrics GetTextMetricsA
#endif
#endif

#ifndef DrawText
#ifdef UNICODE
#define DrawText DrawTextW
#else
#define DrawText DrawTextA
#endif
#endif

extern "C" {

typedef HRESULT (WINAPI *D3DXCreateFontW_pfn)(
        LPDIRECT3DDEVICE9       pDevice,
        INT                     Height,
        UINT                    Width,
        UINT                    Weight,
        UINT                    MipLevels,
        BOOL                    Italic,
        DWORD                   CharSet,
        DWORD                   OutputPrecision,
        DWORD                   Quality,
        DWORD                   PitchAndFamily,
        LPCWSTR                 pFaceName,
        LPD3DXFONT*             ppFont
);

extern D3DXCreateFontW_pfn D3DXCreateFontW;

typedef HRESULT (WINAPI *D3DXCreateFontIndirectW_pfn)(
        LPDIRECT3DDEVICE9       pDevice,
        CONST D3DXFONT_DESCW*   pDesc,
        LPD3DXFONT*             ppFont
);

extern D3DXCreateFontIndirectW_pfn D3DXCreateFontIndirectW;

#define D3DXCreateFontIndirect D3DXCreateFontIndirectW
#define D3DXCreateFont D3DXCreateFontW
}

namespace tsf
{
  namespace RenderFix
  {
    extern ID3DXFont* pFont;
  }
}

#endif /* __TSFIX__RENDER_H__ */