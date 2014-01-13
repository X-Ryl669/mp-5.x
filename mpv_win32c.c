/*

    Minimum Profit - Programmer Text Editor

    Win32 console driver.

    Copyright (C) 1991-2012 Angel Ortega <angel@triptico.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

    http://www.triptico.com

*/

#include "config.h"

#ifdef CONFOPT_WIN32

#include <stdio.h>
#include <windows.h>
#include <tchar.h>

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

HANDLE s_in;
HANDLE s_out;

int bx;
int by;
int tx;
int ty;


/** code **/

static void update_window_size(void)
{
    mpdm_t v;
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(s_out, &csbi);

    bx = 0;
    by = csbi.srWindow.Top;
    tx = csbi.srWindow.Right - csbi.srWindow.Left;
    ty = csbi.srWindow.Bottom - csbi.srWindow.Top;

    v = mpdm_hget_s(MP, L"window");
    mpdm_hset_s(v, L"tx", MPDM_I(tx));
    mpdm_hset_s(v, L"ty", MPDM_I(ty));
}


/** TUI **/

static mpdm_t tui_addstr(mpdm_t a, mpdm_t ctxt)
/* TUI: add a string */
{
    mpdm_t s = mpdm_aget(a, 0);
    wchar_t *str = mpdm_string(s);
    int c = mpdm_size(s);
    DWORD oc;

    if (!WriteConsoleW(s_out, str, c, &oc, 0)) {
        int n;

        /* error writing: strip non-ASCII characters */
        str = wcsdup(str);

        for (n = 0; str[n]; n++) {
            if (str[n] >= 127)
                str[n] = L'?';
        }

        WriteConsoleW(s_out, str, c, &oc, 0);

        free(str);
    }

    return NULL;
}


static mpdm_t tui_move(mpdm_t a, mpdm_t ctxt)
/* TUI: move to a screen position */
{
    int cx = mpdm_ival(mpdm_aget(a, 0));
    int cy = mpdm_ival(mpdm_aget(a, 1));

    COORD bs;

    bs.X = bx + cx;
    bs.Y = by + cy;
    SetConsoleCursorPosition(s_out, bs);

    /* if third argument is not NULL, clear line */
    if (mpdm_aget(a, 2) != NULL) {
//        wclrtoeol(cw);
    }

    return NULL;
}


static mpdm_t tui_attr(mpdm_t a, mpdm_t ctxt)
/* TUI: set attribute for next string */
{
//    last_attr = mpdm_ival(mpdm_aget(a, 0));

//    set_attr();

    return NULL;
}


static mpdm_t tui_refresh(mpdm_t a, mpdm_t ctxt)
/* TUI: refresh the screen */
{
    return NULL;
}


static mpdm_t tui_getxy(mpdm_t a, mpdm_t ctxt)
/* TUI: returns the x and y cursor position */
{
    mpdm_t v;

    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(s_out, &csbi);

    v = MPDM_A(2);
    mpdm_ref(v);

    mpdm_aset(v, MPDM_I(csbi.dwCursorPosition.X), 0);
    mpdm_aset(v, MPDM_I(csbi.dwCursorPosition.Y), 1);

    mpdm_unrefnd(v);

    return v;
}


static mpdm_t tui_openpanel(mpdm_t a, mpdm_t ctxt)
/* opens a panel (creates new window) */
{
    return NULL;
}


static mpdm_t tui_closepanel(mpdm_t a, mpdm_t ctxt)
/* closes a panel (deletes last window) */
{
    return NULL;
}


static void register_functions(void)
{
    mpdm_t drv;
    mpdm_t tui;

    drv = mpdm_hget_s(mpdm_root(), L"mp_drv");

/*
    mpdm_hset_s(drv, L"timer",      MPDM_X(ncursesw_drv_timer));
    mpdm_hset_s(drv, L"shutdown",   MPDM_X(ncursesw_drv_shutdown));
*/
    /* execute tui */
    tui = mpsl_eval(MPDM_LS(L"load('mp_tui.mpsl');"), NULL, NULL);

//    mpdm_hset_s(tui, L"getkey",     MPDM_X(nc_getkey));
    mpdm_hset_s(tui, L"addstr",     MPDM_X(tui_addstr));
    mpdm_hset_s(tui, L"move",       MPDM_X(tui_move));
    mpdm_hset_s(tui, L"attr",       MPDM_X(tui_attr));
    mpdm_hset_s(tui, L"refresh",    MPDM_X(tui_refresh));
    mpdm_hset_s(tui, L"getxy",      MPDM_X(tui_getxy));
    mpdm_hset_s(tui, L"openpanel",  MPDM_X(tui_openpanel));
    mpdm_hset_s(tui, L"closepanel", MPDM_X(tui_closepanel));
//    mpdm_hset_s(tui, L"doc_draw",   MPDM_X(nc_doc_draw));
}


static mpdm_t win32c_drv_startup(mpdm_t a, mpdm_t ctxt)
{
    s_in    = GetStdHandle(STD_INPUT_HANDLE);
    s_out   = GetStdHandle(STD_OUTPUT_HANDLE);

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    SetConsoleTitleW(L"mp " VERSION);

    register_functions();

    update_window_size();

    return NULL;
}


int win32_drv_detect(int *argc, char ***argv)
{
    mpdm_t drv;
    int n;

    for (n = 0; n < *argc; n++) {
        if (strcmp(argv[0][n], "-h") == 0)
            return 0;
    }

    drv = mpdm_hset_s(mpdm_root(), L"mp_drv", MPDM_H(0));

    mpdm_hset_s(drv, L"id",         MPDM_LS(L"win32c"));
    mpdm_hset_s(drv, L"startup",    MPDM_X(win32c_drv_startup));

    return 1;
}

#endif                          /* CONFOPT_WIN32 */
