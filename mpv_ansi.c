/*

    Minimum Profit - Programmer Text Editor

    Raw ANSI terminal driver.

    Copyright (C) 1991-2017 Angel Ortega <angel@triptico.com> et al.

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

    http://triptico.com

*/

#include "config.h"

#ifdef CONFOPT_ANSI

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

#include "mpdm.h"
#include "mpsl.h"
#include "mp.h"


static int *ansi_attrs = NULL;


/** code **/


static void ansi_raw_tty(int start)
/* sets/unsets stdin in raw mode */
{
    static struct termios so;

    if (start) {
        struct termios o;

        /* save previous fd state */
        tcgetattr(0, &so);

        /* set raw */
        tcgetattr(0, &o);
        cfmakeraw(&o);
        tcsetattr(0, TCSANOW, &o);
    }
    else
        /* restore previously saved tty state */
        tcsetattr(0, TCSANOW, &so);
}


static int ansi_something_waiting(int fd)
/* returns yes if there is something waiting on fd */
{
    fd_set ids;
    struct timeval tv;

    /* reset */
    FD_ZERO(&ids);

    /* add fd to set */
    FD_SET(fd, &ids);

    tv.tv_sec  = 0;
    tv.tv_usec = 10000;

    return select(1, &ids, NULL, NULL, &tv) > 0;
}


int ansi_read_string(int fd, char *buf, size_t max)
/* reads an ansi string, waiting in the first char */
{
    int n = 0;

    /* first char blocks, rest of possible chars not */
    do {
        char c;

        if (n == max)
            n = -2;
        else
        if (read(fd, &c, sizeof(c)) == -1)
            n = -1;
        else {
            buf[n++] = c;
            buf[n]   = '\0';
        }

    } while (n >= 0 && ansi_something_waiting(fd));

    return n;
}


static void ansi_get_tty_size(int *w, int *h)
/* asks the tty for its size */
{
    char buffer[32];

    /* magic line: save cursor position, move to stupid position,
       ask for current position and restore cursor position */
    printf("\0337\033[r\033[999;999H\033[6n\0338");
    fflush(stdout);

    ansi_read_string(0, buffer, sizeof(buffer));

    sscanf(buffer, "\033[%d;%dR", h, w);
}


static void ansi_sigwinch(int s)
/* SIGWINCH signal handler */
{
    int tx, ty;
    mpdm_t v;

    /* get new size */
    ansi_get_tty_size(&tx, &ty);

    v = mpdm_hget_s(MP, L"window");
    mpdm_hset_s(v, L"tx", MPDM_I(tx + 1));
    mpdm_hset_s(v, L"ty", MPDM_I(ty));

    /* (re)attach signal */
    signal(SIGWINCH, ansi_sigwinch);
}


static void ansi_gotoxy(int x, int y)
/* positions the cursor */
{
    printf("\033[%d;%dH", y + 1, x);
}


static void ansi_clrscr(void)
/* clears the screen */
{
    printf("\033[2J");
}


static void ansi_print_v(mpdm_t v)
/* prints an mpdm_t */
{
    char *ptr;

    ptr = mpdm_wcstombs(mpdm_string(v), NULL);
    printf("%s", ptr);
    free(ptr);
}


static void ansi_set_attr(int a)
{
    int cf, c0, c1;

    cf = ansi_attrs[a] & 0xff;
    c0 = ((ansi_attrs[a] & 0xff00) >> 8);
    c1 = ((ansi_attrs[a] & 0xff0000) >> 16);

    printf("\033[0;%s%s%s%d;%dm",
        cf & 0x1 ? "7;" : "",
        cf & 0x2 ? "1;" : "",
        cf & 0x4 ? "4;" : "",
        c0 + 30,
        c1 + 40
    );
}


static void build_colors(void)
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
    ansi_attrs = realloc(ansi_attrs, sizeof(int) * s);

    /* loop the colors */
    for (n = 0; n < s && (c = mpdm_aget(l, n)) != NULL; n++) {
        mpdm_t d = mpdm_hget(colors, c);
        mpdm_t v = mpdm_hget_s(d, L"text");
        int cp, c0, c1;

        /* store the attr */
        mpdm_hset_s(d, L"attr", MPDM_I(n));

        /* get color indexes */
        if ((c0 = mpdm_seek(color_names, mpdm_aget(v, 0), 1)) == -1 ||
            (c1 = mpdm_seek(color_names, mpdm_aget(v, 1), 1)) == -1)
            continue;

        if ((--c0) == -1) c0 = 9;
        if ((--c1) == -1) c1 = 9;

        cp = (c1 << 16) | (c0 << 8);

        /* flags */
        v = mpdm_hget_s(d, L"flags");
        if (mpdm_seek_s(v, L"reverse", 1) != -1)
            cp |= 0x01;
        if (mpdm_seek_s(v, L"bright", 1) != -1)
            cp |= 0x02;
        if (mpdm_seek_s(v, L"underline", 1) != -1)
            cp |= 0x04;

        ansi_attrs[n] = cp;
    }
}


struct _str_to_code {
    char *ansi_str;
    wchar_t *code;
} str_to_code[] = {
    { "\033[A",     L"cursor-up" },
    { "\033[B",     L"cursor-down" },
    { "\033[C",     L"cursor-right" },
    { "\033[D",     L"cursor-left" },
    { "\033[5~",    L"page-up" },
    { "\033[6~",    L"page-down" },
    { "\033[H",     L"home" },
    { "\033[F",     L"end" },
    { "\033OP",     L"f1" },
    { "\033OQ",     L"f2" },
    { "\033OR",     L"f3" },
    { "\033OS",     L"f4" },
    { "\033[15~",   L"f5" },
    { "\033[17~",   L"f6" },
    { "\033[18~",   L"f7" },
    { "\033[19~",   L"f8" },
    { "\033[20~",   L"f9" },
    { "\033[21~",   L"f10" },
    { "\033[1;2A",  L"_shift-cursor-up" },
    { "\033[1;2B",  L"_shift-cursor-down" },
    { "\033[1;2C",  L"_shift-cursor-right" },
    { "\033[1;2D",  L"_shift-cursor-left" },
    { "\033[1;5A",  L"ctrl-cursor-up" },
    { "\033[1;5B",  L"ctrl-cursor-down" },
    { "\033[1;5C",  L"ctrl-cursor-right" },
    { "\033[1;5D",  L"ctrl-cursor-left" },
    { "\033[1;5H",  L"ctrl-home" },
    { "\033[1;5F",  L"ctrl-end" },
    { "\033[1;3A",  L"alt-cursor-up" },
    { "\033[1;3B",  L"alt-cursor-down" },
    { "\033[1;3C",  L"alt-cursor-right" },
    { "\033[1;3D",  L"alt-cursor-left" },
    { "\033[1;3H",  L"alt-home" },
    { "\033[1;3F",  L"alt-end" },
    { "\033[3~",    L"delete" },
    { "\033[2~",    L"insert" },
    { "\033[Z",     L"shift-tab" },
    { "\033\r",     L"alt-enter" },
    { NULL,         NULL }
};

#define ctrl(k) ((k) & 31)

static mpdm_t ansi_getkey(mpdm_t args, mpdm_t ctxt)
{
    char str[32];
    wchar_t wstr[2];
    wchar_t *f = NULL;
    mpdm_t k = NULL;

    ansi_read_string(0, str, sizeof(str));

    /* only one char? it's an ASCII or ctrl character */
    if (str[1] == '\0') {
        if (str[0] == '\r')
            f = L"enter";
        else
        if (str[0] == '\t')
            f = L"tab";
        else
        if (str[0] == '\033')
            f = L"escape";
        else
        if (str[0] == '\b' || str[0] == '\177')
            f = L"backspace";
        else
        if (str[0] >= ctrl('a') && str[0] <= ctrl('z')) {
            char tmp[32];

            sprintf(tmp, "ctrl-%c", str[0] + 'a' - ctrl('a'));
            k = MPDM_MBS(tmp);
        }
        else
        if (str[0] == ctrl(' '))
            f = L"ctrl-space";
        else
            k = MPDM_MBS(str);
    }

    /* still nothing? search the table of keys */
    if (k == NULL && f == NULL) {
        int n;

        for (n = 0; str_to_code[n].code != NULL; n++) {
            if (strcmp(str_to_code[n].ansi_str, str) == 0) {
                f = str_to_code[n].code;
                break;
            }
        }

        /* if a found key starts with _shift-, set shift_pressed flag */
        if (f && wcsncmp(f, L"_shift-", 7) == 0) {
            mpdm_hset_s(MP, L"shift_pressed", MPDM_I(1));
            f += 7;
        }
    }

    /* still nothing? try if the string converts to an wchar_t */
    if (f == NULL) {
        if (mbstowcs(wstr, str, 2) == 1)
            f = wstr;
    }

    /* if something, create a value */
    if (k == NULL && f != NULL)
        k = MPDM_S(f);

    return k;
}


static mpdm_t ansi_doc_draw(mpdm_t args, mpdm_t ctxt)
{
    mpdm_t d;
    int n, m;

    ansi_clrscr();

    d = mpdm_aget(args, 0);
    d = mpdm_ref(mp_draw(d, 0));

    for (n = 0; n < mpdm_size(d); n++) {
        mpdm_t l = mpdm_aget(d, n);

        ansi_gotoxy(0, n);

        for (m = 0; m < mpdm_size(l); m++) {
            int attr;
            mpdm_t s;

            /* get the attribute and the string */
            attr = mpdm_ival(mpdm_aget(l, m++));
            s = mpdm_aget(l, m);

            ansi_set_attr(attr);
            ansi_print_v(s);
        }
    }

    mpdm_unref(d);

    return NULL;
}


static mpdm_t ansi_drv_timer(mpdm_t a, mpdm_t ctxt)
{
    return NULL;
}


static mpdm_t ansi_drv_shutdown(mpdm_t a, mpdm_t ctxt)
{
    mpdm_t v;

    ansi_raw_tty(0);

    mp_load_save_state("w");

    ansi_clrscr();

    if ((v = mpdm_hget_s(MP, L"exit_message")) != NULL) {
        mpdm_write_wcs(stdout, mpdm_string(v));
        printf("\n");
    }

    return NULL;
}


static mpdm_t ansi_drv_suspend(mpdm_t a, mpdm_t ctxt)
{
    ansi_raw_tty(0);

    printf("\nType 'fg' to return to Minimum Profit");
    fflush(stdout);

    /* Trigger suspending this process */
    kill(getpid(), SIGSTOP);

    ansi_raw_tty(1);

    return NULL;
}


/** TUI **/

static mpdm_t ansi_tui_addstr(mpdm_t a, mpdm_t ctxt)
/* TUI: add a string */
{
    ansi_print_v(mpdm_aget(a, 0));

    return NULL;
}


static mpdm_t ansi_tui_move(mpdm_t a, mpdm_t ctxt)
/* TUI: move to a screen position */
{
    ansi_gotoxy(mpdm_ival(mpdm_aget(a, 0)), mpdm_ival(mpdm_aget(a, 1)));

    /* if third argument is not NULL, clear line */
    if (mpdm_aget(a, 2) != NULL)
        printf("\033[K");

    return NULL;
}


static mpdm_t ansi_tui_attr(mpdm_t a, mpdm_t ctxt)
/* TUI: set attribute for next string */
{
    ansi_set_attr(mpdm_ival(mpdm_aget(a, 0)));

    return NULL;
}


static mpdm_t ansi_tui_refresh(mpdm_t a, mpdm_t ctxt)
/* TUI: refresh the screen */
{
    fflush(stdout);
    return NULL;
}


static mpdm_t ansi_tui_getxy(mpdm_t a, mpdm_t ctxt)
/* TUI: returns the x and y cursor position */
{
    mpdm_t v;
    int x, y;
    char buffer[32];

    printf("\033[6n");
    fflush(stdout);

    ansi_read_string(0, buffer, sizeof(buffer));
    sscanf(buffer, "\033[%d;%dR", &y, &x);

    v = MPDM_A(2);
    mpdm_ref(v);

    mpdm_aset(v, MPDM_I(x), 0);
    mpdm_aset(v, MPDM_I(y), 1);

    mpdm_unrefnd(v);

    return v;
}


static void register_functions(void)
{
    mpdm_t drv;
    mpdm_t tui;

    drv = mpdm_hget_s(mpdm_root(), L"mp_drv");

    mpdm_hset_s(drv, L"timer",      MPDM_X(ansi_drv_timer));
    mpdm_hset_s(drv, L"shutdown",   MPDM_X(ansi_drv_shutdown));
    mpdm_hset_s(drv, L"suspend",    MPDM_X(ansi_drv_suspend));

    /* execute tui */
    tui = mpsl_eval(MPDM_LS(L"load('mp_tui.mpsl');"), NULL, NULL);

    mpdm_hset_s(tui, L"getkey",     MPDM_X(ansi_getkey));
    mpdm_hset_s(tui, L"addstr",     MPDM_X(ansi_tui_addstr));
    mpdm_hset_s(tui, L"move",       MPDM_X(ansi_tui_move));
    mpdm_hset_s(tui, L"attr",       MPDM_X(ansi_tui_attr));
    mpdm_hset_s(tui, L"refresh",    MPDM_X(ansi_tui_refresh));
    mpdm_hset_s(tui, L"getxy",      MPDM_X(ansi_tui_getxy));
    mpdm_hset_s(tui, L"doc_draw",   MPDM_X(ansi_doc_draw));
}


static mpdm_t ansi_drv_startup(mpdm_t a)
{
    register_functions();

    ansi_raw_tty(1);
    ansi_sigwinch(0);

    build_colors();

    mp_load_save_state("r");

    return NULL;
}


int ansi_drv_detect(int *argc, char ***argv)
{
    mpdm_t drv;

    drv = mpdm_hset_s(mpdm_root(), L"mp_drv", MPDM_H(0));

    mpdm_hset_s(drv, L"id",         MPDM_LS(L"ansi"));
    mpdm_hset_s(drv, L"startup",    MPDM_X(ansi_drv_startup));

    return 1;
}


#endif /* CONFOPT_ANSI */
