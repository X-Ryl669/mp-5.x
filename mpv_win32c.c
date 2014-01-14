/*

    Minimum Profit - Programmer Text Editor

    Win32 console driver.

    Copyright (C) 1991-2014 Angel Ortega <angel@triptico.com>

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

WORD *win32c_attrs = NULL;
int normal_attr = 0;

/** code **/

static void update_window_size(void)
{
    mpdm_t v;
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(s_out, &csbi);

    bx = 0;
    by = csbi.srWindow.Top;
    tx = (csbi.srWindow.Right - csbi.srWindow.Left) + 1;
    ty = (csbi.srWindow.Bottom - csbi.srWindow.Top) + 1;

    v = mpdm_hget_s(MP, L"window");
    mpdm_hset_s(v, L"tx", MPDM_I(tx));
    mpdm_hset_s(v, L"ty", MPDM_I(ty));
}

static void build_colors(void)
/* builds the colors */
{
    mpdm_t colors;
    mpdm_t color_names;
    mpdm_t l;
    mpdm_t c;
    int n, s;

    /* gets the color definitions and attribute names */
    colors      = mpdm_hget_s(MP, L"colors");
    color_names = mpdm_hget_s(MP, L"color_names");

    l = mpdm_ref(mpdm_keys(colors));
    s = mpdm_size(l);

    /* redim the structures */
    win32c_attrs = realloc(win32c_attrs, sizeof(WORD) * s);

    /* loop the colors */
    for (n = 0; n < s && (c = mpdm_aget(l, n)) != NULL; n++) {
        mpdm_t d = mpdm_hget(colors, c);
        mpdm_t v = mpdm_hget_s(d, L"text");
        int c0, c1;
    	WORD cp = 0;

        /* store the 'normal' attribute */
        if (wcscmp(mpdm_string(c), L"normal") == 0)
            normal_attr = n;

        /* store the attr */
        mpdm_hset_s(d, L"attr", MPDM_I(n));

        /* get color indexes */
        if ((c0 = mpdm_seek(color_names, mpdm_aget(v, 0), 1)) == -1 ||
            (c1 = mpdm_seek(color_names, mpdm_aget(v, 1), 1)) == -1)
            continue;

        /* color names match the color bits; 0 is 'default' */
        if (c0 == 0) c0 = 8;
        if (c1 == 0) c1 = 1;

        c0--;
        c1--;

        /* flags */
        v = mpdm_hget_s(d, L"flags");

        if (mpdm_seek_s(v, L"reverse", 1) != -1) {
            int t = c0;
            c0 = c1;
            c1 = t;
        }
        if (mpdm_seek_s(v, L"bright", 1) != -1)
            cp |= FOREGROUND_INTENSITY;

        if ((c0 & 1)) cp |= FOREGROUND_RED;
        if ((c1 & 1)) cp |= BACKGROUND_RED;
        if ((c0 & 2)) cp |= FOREGROUND_GREEN;
        if ((c1 & 2)) cp |= BACKGROUND_GREEN;
        if ((c0 & 4)) cp |= FOREGROUND_BLUE;
        if ((c1 & 4)) cp |= BACKGROUND_BLUE;

        win32c_attrs[n] = cp;
    }

    mpdm_unref(l);
}


#define ctrl(c) ((c) & 31)

static mpdm_t win32c_getkey(mpdm_t args, mpdm_t ctxt)
/* reads a key and converts to an action */
{
    wchar_t *f = NULL;
    mpdm_t k = NULL;
    DWORD ne;
    INPUT_RECORD ev;
    static wchar_t sc[2];

    WaitForSingleObject(s_in, INFINITE);

    ReadConsoleInputW(s_in, &ev, 1, &ne);

    if (ne) {
        if (ev.EventType == KEY_EVENT) {
            if (ev.Event.KeyEvent.bKeyDown) {
                wchar_t c = ev.Event.KeyEvent.uChar.UnicodeChar;

                if (c) {
                    if (c > 32) {
                        sc[0] = c;
                        sc[1] = L'\0';
                        f = sc;
                    }
                    else {
                        switch (c) {
                        case ctrl(' '):
                            f = L"ctrl-space";
                            break;
                        case ctrl('a'):
                            f = L"ctrl-a";
                            break;
                        case ctrl('b'):
                            f = L"ctrl-b";
                            break;
                        case ctrl('c'):
                            f = L"ctrl-c";
                            break;
                        case ctrl('d'):
                            f = L"ctrl-d";
                            break;
                        case ctrl('e'):
                            f = L"ctrl-e";
                            break;
                        case ctrl('f'):
                            f = L"ctrl-f";
                            break;
                        case ctrl('g'):
                            f = L"ctrl-g";
                            break;
                        case ctrl('h'):         /* same as backspace */
                            f = L"backspace";
                            break;
                        case ctrl('i'):         /* same as tab */
                            f = L"tab";
                            break;
                        case ctrl('j'):
                            f = L"ctrl-j";
                            break;
                        case ctrl('k'):
                            f = L"ctrl-k";
                            break;
                        case ctrl('l'):
                            f = L"ctrl-l";
                            break;
                        case ctrl('m'):            /* same as ENTER */
                            f = L"enter";
                            break;
                        case ctrl('n'):
                            f = L"ctrl-n";
                            break;
                        case ctrl('o'):
                            f = L"ctrl-o";
                            break;
                        case ctrl('p'):
                            f = L"ctrl-p";
                            break;
                        case ctrl('q'):
                            f = L"ctrl-q";
                            break;
                        case ctrl('r'):
                            f = L"ctrl-r";
                            break;
                        case ctrl('s'):
                            f = L"ctrl-s";
                            break;
                        case ctrl('t'):
                            f = L"ctrl-t";
                            break;
                        case ctrl('u'):
                            f = L"ctrl-u";
                            break;
                        case ctrl('v'):
                            f = L"ctrl-v";
                            break;
                        case ctrl('w'):
                            f = L"ctrl-w";
                            break;
                        case ctrl('x'):
                            f = L"ctrl-x";
                            break;
                        case ctrl('y'):
                            f = L"ctrl-y";
                            break;
                        case ctrl('z'):
                            f = L"ctrl-z";
                            break;
                        case ' ':
                            f = L"space";
                            break;
                        case 27:
                            f = L"escape";
                            break;
                        }
                    }
                }
                else {
                    switch (ev.Event.KeyEvent.wVirtualKeyCode) {
                    case VK_UP:
                        f = L"cursor-up";
                        break;
                    case VK_DOWN:
                        f = L"cursor-down";
                        break;
                    case VK_LEFT:
                        f = L"cursor-left";
                        break;
                    case VK_RIGHT:
                        f = L"cursor-right";
                        break;
                    case VK_PRIOR:
                        f = L"page-up";
                        break;
                    case VK_NEXT:
                        f = L"page-down";
                        break;
                    case VK_HOME:
                        f = L"home";
                        break;
                    case VK_END:
                        f = L"end";
                        break;
                    case VK_RETURN:
                        f = L"enter";
                        break;
                    case VK_BACK:
                        f = L"backspace";
                        break;
                    case VK_DELETE:
                        f = L"delete";
                        break;
                    case VK_INSERT:
                        f = L"insert";
                        break;
                    case VK_DIVIDE:
                        f = L"kp-divide";
                        break;
                    case VK_MULTIPLY:
                        f = L"kp-multiply";
                        break;
                    case VK_SUBTRACT:
                        f = L"kp-minus";
                        break;
                    case VK_ADD:
                        f = L"kp-plus";
                        break;
                    case VK_F1:
                        f = L"f1";
                        break;
                    case VK_F2:
                        f = L"f2";
                        break;
                    case VK_F3:
                        f = L"f3";
                        break;
                    case VK_F4:
                        f = L"f4";
                        break;
                    case VK_F5:
                        f = L"f5";
                        break;
                    case VK_F6:
                        f = L"f6";
                        break;
                    case VK_F7:
                        f = L"f7";
                        break;
                    case VK_F8:
                        f = L"f8";
                        break;
                    case VK_F9:
                        f = L"f9";
                        break;
                    case VK_F10:
                        f = L"f10";
                        break;
                    case VK_F11:
                        f = L"f11";
                        break;
                    case VK_F12:
                        f = L"f12";
                        break;
                    }
                }
            }
        }
    }

    if (f != NULL)
        k = MPDM_S(f);

    return k;
}


static void win32c_addwstr(mpdm_t s)
{
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
}


/** TUI **/

static mpdm_t tui_addstr(mpdm_t a, mpdm_t ctxt)
/* TUI: add a string */
{
    win32c_addwstr(mpdm_aget(a, 0));
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
        int n;
        DWORD oc;

        for (n = 0; n < tx - cx; n++)
            WriteConsoleW(s_out, L" ", 1, &oc, 0);

        SetConsoleCursorPosition(s_out, bs);
    }

    return NULL;
}


static mpdm_t tui_attr(mpdm_t a, mpdm_t ctxt)
/* TUI: set attribute for next string */
{
    int attr = mpdm_ival(mpdm_aget(a, 0));

    SetConsoleTextAttribute(s_out, win32c_attrs[attr]);
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

    mpdm_aset(v, MPDM_I(csbi.dwCursorPosition.X - bx), 0);
    mpdm_aset(v, MPDM_I(csbi.dwCursorPosition.Y - by), 1);

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


static mpdm_t win32c_doc_draw(mpdm_t args, mpdm_t ctxt)
/* draws the document part */
{
    mpdm_t d;
    int n, m, c;

    d = mpdm_aget(args, 0);
    d = mpdm_ref(mp_draw(d, 0));

    for (n = 0; n < mpdm_size(d); n++) {
        mpdm_t l = mpdm_aget(d, n);

        COORD bs;

        bs.X = bx;
        bs.Y = by + n;
        SetConsoleCursorPosition(s_out, bs);

        c = 0;
        for (m = 0; m < mpdm_size(l); m++) {
            int attr;
            mpdm_t s;

            /* get the attribute and the string */
            attr = mpdm_ival(mpdm_aget(l, m++));
            s = mpdm_aget(l, m);

            SetConsoleTextAttribute(s_out, win32c_attrs[attr]);
            win32c_addwstr(s);
            c += mpdm_size(s);
        }

        DWORD oc;
        SetConsoleTextAttribute(s_out, win32c_attrs[normal_attr]);
        for (m = c; m < tx; m++)
            WriteConsoleW(s_out, L" ", 1, &oc, 0);
    }

    mpdm_unref(d);

    return NULL;
}


static mpdm_t win32c_drv_shutdown(mpdm_t a)
{
    mpdm_t v;

    if ((v = mpdm_hget_s(MP, L"exit_message")) != NULL) {
        mpdm_write_wcs(stdout, mpdm_string(v));
        printf("\n");
    }

    return NULL;
}


static void register_functions(void)
{
    mpdm_t drv;
    mpdm_t tui;

    drv = mpdm_hget_s(mpdm_root(), L"mp_drv");


//    mpdm_hset_s(drv, L"timer",      MPDM_X(ncursesw_drv_timer));
    mpdm_hset_s(drv, L"shutdown",   MPDM_X(win32c_drv_shutdown));

    /* execute tui */
    tui = mpsl_eval(MPDM_LS(L"load('mp_tui.mpsl');"), NULL, NULL);

    mpdm_hset_s(tui, L"getkey",     MPDM_X(win32c_getkey));
    mpdm_hset_s(tui, L"addstr",     MPDM_X(tui_addstr));
    mpdm_hset_s(tui, L"move",       MPDM_X(tui_move));
    mpdm_hset_s(tui, L"attr",       MPDM_X(tui_attr));
    mpdm_hset_s(tui, L"refresh",    MPDM_X(tui_refresh));
    mpdm_hset_s(tui, L"getxy",      MPDM_X(tui_getxy));
    mpdm_hset_s(tui, L"openpanel",  MPDM_X(tui_openpanel));
    mpdm_hset_s(tui, L"closepanel", MPDM_X(tui_closepanel));
    mpdm_hset_s(tui, L"doc_draw",   MPDM_X(win32c_doc_draw));
}


static mpdm_t win32c_drv_startup(mpdm_t a, mpdm_t ctxt)
{
    s_in    = GetStdHandle(STD_INPUT_HANDLE);
    s_out   = GetStdHandle(STD_OUTPUT_HANDLE);

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    SetConsoleTitleW(L"mp " VERSION);

    SetConsoleMode(s_in, ENABLE_WINDOW_INPUT);
    SetConsoleMode(s_out, ENABLE_PROCESSED_OUTPUT);

    register_functions();
    build_colors();
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
