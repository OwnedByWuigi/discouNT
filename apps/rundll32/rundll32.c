/*
 * PURPOSE: Load a DLL and run an entry point with the specified parameters
 *
 * Copyright 2002 Alberto Massari
 * Copyright 2001-2003 Aric Stewart for CodeWeavers
 * Copyright 2003 Mike McCormack for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */

/*
 *
 *  rundll32 dllname,entrypoint [arguments]
 *
 *  Documentation for this utility found on KB Q164787
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Exclude rarely-used stuff from Windows headers */
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(rundll32);


/* The native loader uses the declared WINAPI calling convention directly. */
static void call_entry_point( void *func, HWND hwnd, HINSTANCE inst, void *cmdline, int show )
{
    void (WINAPI *entry_point)( HWND hwnd, HINSTANCE inst, void *cmdline, int show ) = func;
    entry_point( hwnd, inst, cmdline, show );
}

/*
 * Control_RunDLL needs to have a window. So lets make us a very simple window class.
 */
static ATOM register_class(void)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEXW);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = DefWindowProcW;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = NULL;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = L"class_rundll32";
    wcex.hIconSm        = NULL;

    return RegisterClassExW(&wcex);
}

static void *get_entry_point32( HMODULE module, LPCWSTR entry, BOOL *unicode )
{
    void *ret;

    /* determine if the entry point is an ordinal */
    if (entry[0] == '#')
    {
        INT_PTR ordinal = wcstol( entry + 1, NULL, 10 );
        if (ordinal <= 0)
            return NULL;

        *unicode = TRUE;
        ret = GetProcAddress( module, (LPCSTR)ordinal );
    }
    else
    {
        DWORD len = WideCharToMultiByte( CP_ACP, 0, entry, -1, NULL, 0, NULL, NULL );
        char *entryA = malloc( len + 1 );

        if (!entryA)
            return NULL;

        WideCharToMultiByte( CP_ACP, 0, entry, -1, entryA, len, NULL, NULL );

        /* first try the W version */
        *unicode = TRUE;
        strcat( entryA, "W" );
        if (!(ret = GetProcAddress( module, entryA )))
        {
            /* now the A version */
            *unicode = FALSE;
            entryA[strlen(entryA)-1] = 'A';
            if (!(ret = GetProcAddress( module, entryA )))
            {
                /* now the version without suffix */
                entryA[strlen(entryA)-1] = 0;
                ret = GetProcAddress( module, entryA );
            }
        }
        free( entryA );
    }
    return ret;
}

static WCHAR *parse_arguments( WCHAR *cmdline, WCHAR **dll, WCHAR **entrypoint )
{
    WCHAR *p;

    if (cmdline[0] == '"')
        p = wcschr( ++cmdline, '"' );
    else
        p = wcspbrk( cmdline, L" ,\t" );

    if (!p) return NULL;
    *p++ = 0;
    *dll = cmdline;
    /* skip spaces and commas */
    p += wcsspn( p, L" ,\t" );
    if (!*p) return NULL;
    *entrypoint = p;
    /* find end of entry point string */
    p += wcscspn( p, L" \t" );
    if (*p) *p++ = 0;
    p += wcsspn( p, L" \t" );
    return p;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE hOldInstance, LPWSTR szCmdLine, int nCmdShow)
{
    HWND hWnd;
    LPWSTR szDllName, szEntryPoint, args;
    void *entry_point = NULL;
    BOOL unicode = FALSE;
    HMODULE hDll;
    STARTUPINFOW info;

    /* Initialize the rundll32 class */
    register_class();
    hWnd = CreateWindowW(L"class_rundll32", L"rundll32", WS_OVERLAPPEDWINDOW|WS_VISIBLE,
          CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, NULL, NULL);

    /* Get the dll name and API EntryPoint */
    args = parse_arguments( wcsdup(szCmdLine), &szDllName, &szEntryPoint );
    if (!args) return 0;

    TRACE( "dll %s entry %s cmdline %s\n", wine_dbgstr_w(szDllName),
           wine_dbgstr_w(szEntryPoint), wine_dbgstr_w(args) );

    /* Load the library */
    hDll=LoadLibraryW(szDllName);
    if (hDll) entry_point = get_entry_point32( hDll, szEntryPoint, &unicode );

    if (!entry_point)
    {
        /* Windows has a MessageBox here... */
        WINE_ERR( "Unable to find the entry point %s in %s\n",
                  wine_dbgstr_w(szEntryPoint), wine_dbgstr_w(szDllName) );
        return 0;
    }

    GetStartupInfoW( &info );
    if (!(info.dwFlags & STARTF_USESHOWWINDOW)) info.wShowWindow = SW_SHOWDEFAULT;

    if (unicode)
    {
        WINE_TRACE( "Calling %s (%p,%p,%s,%d)\n", wine_dbgstr_w(szEntryPoint),
                    hWnd, instance, wine_dbgstr_w(args), info.wShowWindow );

        call_entry_point( entry_point, hWnd, instance, args, info.wShowWindow );
    }
    else
    {
        DWORD len = WideCharToMultiByte( CP_ACP, 0, args, -1, NULL, 0, NULL, NULL );
        char *cmdline = malloc( len );

        if (!cmdline)
            return 0;

        WideCharToMultiByte( CP_ACP, 0, args, -1, cmdline, len, NULL, NULL );

        WINE_TRACE( "Calling %s (%p,%p,%s,%d)\n", wine_dbgstr_w(szEntryPoint),
                    hWnd, instance, wine_dbgstr_a(cmdline), info.wShowWindow );

        call_entry_point( entry_point, hWnd, instance, cmdline, info.wShowWindow );
    }

    return 0; /* rundll32 always returns 0! */
}
