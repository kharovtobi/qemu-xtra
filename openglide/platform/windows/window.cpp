//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*   Windows specific functions for handling display window
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//*               Linux version by Simon White
//**************************************************************
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if !defined C_USE_SDL && defined WIN32

#include <windows.h>
#include <io.h>
#include <math.h>

#include "GlOgl.h"

#include "platform/window.h"
#include <GL/glext.h>
#include <GL/wgl.h>

static HDC   hDC;
static HGLRC hRC;
static HWND  hWND;
static struct
{
    FxU16 red[ 256 ];
    FxU16 green[ 256 ];
    FxU16 blue[ 256 ];
} old_ramp;

static BOOL ramp_stored  = false;
static BOOL mode_changed = false;
static int tainted_cursor;

#define DPRINTF(fmt, ...)
static LONG WINAPI GlideWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {
	case WM_MOUSEACTIVATE:
	    return MA_NOACTIVATEANDEAT;
        case WM_ACTIVATE:
        case WM_ACTIVATEAPP:
	case WM_NCLBUTTONDOWN:
	    return 0;
	default:
	    break;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static HWND CreateGlideWindow(const char *title, int w, int h, int show)
{
    HWND 	hWnd;
    WNDCLASS 	wc;
    HINSTANCE   hInstance = GetModuleHandle(0);

    memset(&wc, 0, sizeof(WNDCLASS));
    wc.hInstance = hInstance;
    wc.style	= CS_OWNDC;
    wc.lpfnWndProc	= (WNDPROC)GlideWndProc;
    wc.lpszClassName = title;

    if (!RegisterClass(&wc)) {
        DPRINTF("RegisterClass() faled, Error %08lx", GetLastError());
        return NULL;
    }
    
    RECT rect;
    rect.top = 0; rect.left = 0;
    rect.right = w; rect.bottom = h;
    AdjustWindowRectEx(&rect, WS_CAPTION, FALSE, 0);
    rect.right  -= rect.left;
    rect.bottom -= rect.top;
    hWnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_NOACTIVATE,
	    title, title,
	    WS_CAPTION | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
	    CW_USEDEFAULT, CW_USEDEFAULT, rect.right, rect.bottom,
	    NULL, NULL, hInstance, NULL);
    if (show) {
        GetClientRect(hWnd, &rect);
        DPRINTF("    window %lux%lu", rect.right, rect.bottom);
        ShowCursor(FALSE);
        ShowWindow(hWnd, SW_SHOW);
    }

    return hWnd;
}

static int *iattribs_fb(const int do_msaa)
{
    static int ia[] = {
        WGL_DRAW_TO_WINDOW_ARB, 1,
        WGL_SUPPORT_OPENGL_ARB, 1,
        WGL_DOUBLE_BUFFER_ARB, 1,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_COLOR_BITS_ARB, 32,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_ALPHA_BITS_ARB, 8,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_SAMPLE_BUFFERS_ARB, 0,
        WGL_SAMPLES_ARB, 0,
        0,0,
    };
    for (int i = 0; ia[i]; i+=2) {
        switch(ia[i]) {
            case WGL_SAMPLE_BUFFERS_ARB:
                ia[i+1] = (do_msaa)? 1:0;
                break;
            case WGL_SAMPLES_ARB:
                ia[i+1] = (do_msaa)? do_msaa:0;
                break;
            default:
                break;
        }
    }
    return ia;
}

bool InitialiseOpenGLWindow(FxU wnd, int x, int y, int width, int height)
{
    PIXELFORMATDESCRIPTOR   pfd;
    int                     PixFormat;
    HWND                    hwnd = (HWND) wnd;

    if( hwnd == NULL )
    {
        hwnd = GetActiveWindow();
    }

    if ( hwnd == NULL )
    {
        hwnd = CreateGlideWindow("GlideWnd", width, height, 1);
        if ( hwnd == NULL ) {
            MessageBox( NULL, "Failed to create window", "Error", MB_OK );
            exit( 1 );
        }
    }

    mode_changed = false;

#if (SIZEOF_INT_P == 4)
    DISPLAY_DEVICE dd = { .cb = sizeof(DISPLAY_DEVICE) };
    const char vidstr[] = "QEMU Bochs";
    if (EnumDisplayDevices(NULL, 0, &dd, 0) &&
        !memcmp(dd.DeviceString, vidstr, strlen(vidstr))) {
        CURSORINFO ci = { .cbSize = sizeof(CURSORINFO) };
        if (GetCursorInfo(&ci) && ci.flags == CURSOR_SHOWING) {
            tainted_cursor = 1;
            while (ShowCursor(FALSE) >= 0)
                tainted_cursor++;
        }
        UserConfig.QEmu = UserConfig.InitFullScreen = true;
    }
#endif

    if ( UserConfig.InitFullScreen )
    {
        if (!UserConfig.QEmu) {
            SetWindowLong( hwnd, 
                           GWL_STYLE, 
                           WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
            MoveWindow( hwnd, 0, 0, width, height, false );
            mode_changed = SetScreenMode( width, height );
        }
        else {
            RECT rect;
            GetWindowRect(GetDesktopWindow(), &rect);
            if (rect.bottom > OpenGL.WindowHeight) {
                float r = (1.f * height) / width,
                      win_r = (1.f * rect.bottom) / rect.right;
                if (r == win_r) {
                    OpenGL.WindowWidth = rect.right;
                    OpenGL.WindowHeight = rect.bottom;
                    OpenGL.WindowOffset = 0;
                }
                else {
                    OpenGL.WindowWidth = rect.bottom / r;
                    OpenGL.WindowHeight = rect.bottom;
                    OpenGL.WindowOffset = (rect.right - OpenGL.WindowWidth) >> 1;
                }
                UserConfig.Resolution = OpenGL.WindowWidth;
            }
        }
    }
    else
    {
       if (!UserConfig.QEmu) {
           RECT rect, radj;
           int x_adj, y_adj;
           GetWindowRect(hwnd, &rect);
           GetClientRect(hwnd, &radj);
           x_adj = (rect.right - rect.left - radj.right) >> 1;
           y_adj = rect.bottom - rect.top - radj.bottom - x_adj;
           rect.right = rect.left + width;
           rect.bottom = rect.top + height;

           AdjustWindowRectEx( &rect, 
                               GetWindowLong( hwnd, GWL_STYLE ),
                               GetMenu( hwnd ) != NULL,
                               GetWindowLong( hwnd, GWL_EXSTYLE ) );
           MoveWindow( hwnd, 
                       x + rect.left + x_adj, y + rect.top + y_adj, 
                       x + ( rect.right - rect.left ),
                       y + ( rect.bottom - rect.top ),
                       true );
       }
    }

    hWND = hwnd;

    hDC = GetDC( hwnd );

    ZeroMemory( &pfd, sizeof( pfd ) );
    pfd.nSize        = sizeof( pfd );
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;
    pfd.cDepthBits   = 24;
    pfd.cAlphaBits   = 8;
    pfd.cStencilBits = 8;

    HWND tmpWnd = CreateGlideWindow("dummy", 320, 200, 0);
    HDC tmpDC = GetDC(tmpWnd);
    SetPixelFormat(tmpDC, ChoosePixelFormat(tmpDC, &pfd), &pfd);
    HGLRC tmpGL = wglCreateContext(tmpDC);
    wglMakeCurrent(tmpDC, tmpGL);

    BOOL (WINAPI *p_wglGetPixelFormatAttribivARB)(HDC, int, int, UINT, const int *, int *) =
        (BOOL (WINAPI *)(HDC, int, int, UINT, const int *, int *))wglGetProcAddress("wglGetPixelFormatAttribivARB");
    BOOL (WINAPI *p_wglChoosePixelFormatARB)(HDC, const int *, const float *, UINT, int *, UINT *) =
        (BOOL (WINAPI *)(HDC, const int *, const float *, UINT, int *, UINT *))wglGetProcAddress("wglChoosePixelFormatARB");

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(tmpGL);
    ReleaseDC(tmpWnd, tmpDC);
    DestroyWindow(tmpWnd);
    UnregisterClass("dummy", GetModuleHandle(0));

    if ( !( PixFormat = GetPixelFormat(hDC))) {
        if (p_wglChoosePixelFormatARB) {
            static const float fa[] = {0, 0};
            int *ia = iattribs_fb(UserConfig.SamplesMSAA);
            int pi[64]; UINT nFmts = 0;
            BOOL status = p_wglChoosePixelFormatARB(hDC, ia, fa, 64, pi, &nFmts);
            if (UserConfig.SamplesMSAA && !nFmts) {
                ia = iattribs_fb(0);
                status = p_wglChoosePixelFormatARB(hDC, ia, fa, 64, pi, &nFmts);
            }
            PixFormat = (status && nFmts)? pi[0]:0;
        }

        if (!PixFormat) {
            fprintf(stderr, "Warn: Fallback to legacy OpenGL context creation\n");
            if ( !( PixFormat = ChoosePixelFormat( hDC, &pfd ) ) )
            {
                MessageBox( NULL, "ChoosePixelFormat() failed:  "
                            "Cannot find a suitable pixel format.", "Error", MB_OK );
                exit( 1 );
            }
        }

        // the window must have WS_CLIPCHILDREN and WS_CLIPSIBLINGS for this call to
        // work correctly, so we SHOULD set this attributes, not doing that yet
        if ( !SetPixelFormat( hDC, PixFormat, &pfd ) )
        {
            MessageBox( NULL, "SetPixelFormat() failed:  "
                        "Cannot set format specified.", "Error", MB_OK );
            exit( 1 );
        }
    }
    else
        SetPixelFormat( hDC, PixFormat, NULL );

    DescribePixelFormat( hDC, PixFormat, sizeof( PIXELFORMATDESCRIPTOR ), &pfd );
    GlideMsg( "ColorBits	= %d\n", pfd.cColorBits );
    GlideMsg( "DepthBits	= %d\n", pfd.cDepthBits );

    if (p_wglGetPixelFormatAttribivARB) {
        static const int iattr[] = {
            WGL_AUX_BUFFERS_ARB,
            WGL_SAMPLE_BUFFERS_ARB,
            WGL_SAMPLES_ARB,
            WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB,
        };
        int cattr[4];
        p_wglGetPixelFormatAttribivARB(hDC, PixFormat, 0, 4, iattr, cattr);
        cattr[3] = (cattr[3] && UserConfig.FramebufferSRGB)? 1:0;
        UserConfig.FramebufferSRGB = cattr[3] != 0;
        fprintf(stderr, "Info: PixFmt 0x%02x nAux %d nSamples %d %d %s\n", PixFormat,
            cattr[0], cattr[1], cattr[2], (cattr[3])? "sRGB":"");
    }

    if ( pfd.cDepthBits > 16 )
    {
        UserConfig.PrecisionFix = false;
    }

    hRC = wglCreateContext( hDC );
    wglMakeCurrent( hDC, hRC );

    if (UserConfig.FramebufferSRGB)
        glEnable(GL_FRAMEBUFFER_SRGB);

    // ramp_stored = GetDeviceGammaRamp( pDC, &old_ramp );
    for (int i = 0; i < 0x100; i++) {
        old_ramp.red[i]   = (FxU16)(((i << 8) | i) & 0xFFFFU);
        old_ramp.green[i] = (FxU16)(((i << 8) | i) & 0xFFFFU);
        old_ramp.blue[i]  = (FxU16)(((i << 8) | i) & 0xFFFFU);
    }
    ramp_stored = true;

    return true;
}

void FinaliseOpenGLWindow(void)
{
    if ( ramp_stored )
        SetGammaTable(&old_ramp);
    if ( UserConfig.FramebufferSRGB )
        glDisable(GL_FRAMEBUFFER_SRGB);
    SetSwapInterval(-1);

    wglMakeCurrent( NULL, NULL );
    wglDeleteContext( hRC );
    ReleaseDC( hWND, hDC );

    if( mode_changed )
    {
        ResetScreenMode( );
    }
    for (; tainted_cursor; tainted_cursor--)
        ShowCursor(TRUE);
}

void SetGamma(float value)
{
    struct
    {
        WORD red[256];
        WORD green[256];
        WORD blue[256];
    } ramp;

    for ( int i = 0; i < 256; i++ )
    {
        WORD v = (WORD)( 0xffff * pow( i / 255.0, 1.0 / value ) );

        ramp.red[ i ] = ramp.green[ i ] = ramp.blue[ i ] = ( v & 0xff00 );
    }

    SetGammaTable(&ramp);
}

void RestoreGamma()
{
}

void SetGammaTable(void *ptbl)
{
    HDC pDC = GetDC(NULL);
    BOOL (WINAPI *SetGammaExt)(HDC, LPVOID) = (BOOL (WINAPI *)(HDC, LPVOID))
        wglGetProcAddress("wglSetDeviceGammaRamp3DFX");
    if (SetGammaExt)
        SetGammaExt( pDC, ptbl );
    else if (!UserConfig.FramebufferSRGB)
    SetDeviceGammaRamp( pDC, ptbl );
    ReleaseDC( NULL, pDC );
}

void GetGammaTable(void *ptbl)
{
    HDC pDC = GetDC(NULL);
    BOOL (WINAPI *GetGammaExt)(HDC, LPVOID) = (BOOL (WINAPI *)(HDC, LPVOID))
        wglGetProcAddress("wglGetDeviceGammaRamp3DFX");
    if (GetGammaExt)
        GetGammaExt( pDC, ptbl );
    else
    GetDeviceGammaRamp( pDC, ptbl );
    ReleaseDC( NULL, pDC );
}

bool SetScreenMode(int &xsize, int &ysize)
{
    HDC     hdc;
    FxU32   bits_per_pixel;
    bool    found;
    DEVMODE DevMode;

    hdc = GetDC( hWND );
    bits_per_pixel = GetDeviceCaps( hdc, BITSPIXEL );
    ReleaseDC( hWND, hdc );
    
    found = false;
    DevMode.dmSize = sizeof( DEVMODE );
    
    for ( int i = 0; 
          !found && EnumDisplaySettings( NULL, i, &DevMode ) != false; 
          i++ )
    {
        if ( ( DevMode.dmPelsWidth == (FxU32)xsize ) && 
             ( DevMode.dmPelsHeight == (FxU32)ysize ) && 
             ( DevMode.dmBitsPerPel == bits_per_pixel ) )
        {
            DevMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
            found = true;
        }
    }
    
    return ( found && ChangeDisplaySettings( &DevMode, 0 ) == DISP_CHANGE_SUCCESSFUL );
}

void ResetScreenMode()
{
    ChangeDisplaySettings( NULL, 0 );
}

void SetSwapInterval(const int i)
{
    static int last_i = -1;
    BOOL (WINAPI *SwapIntervalEXT)(int) = (BOOL (WINAPI *)(int))
        wglGetProcAddress("wglSwapIntervalEXT");
    if (SwapIntervalEXT && (last_i != i)) {
        last_i = i;
        if (i >= 0)
            SwapIntervalEXT(i);
    }
}

void SwapBuffers()
{
    SwapBuffers(hDC);
}

#endif // !C_USE_SDL && WIN32
