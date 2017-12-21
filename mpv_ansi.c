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

#include "mpdm.h"
#include "mpsl.h"
#include "mp.h"


/* global terminal size */
int g_tx, g_ty;


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
    tv.tv_usec = 100000;

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
    mpdm_t v;
    char buffer[32];

    /* magic line: save cursor position, move to stupid position,
       ask for current position and restore cursor position */
    printf("\0337\033[r\033[999;999H\033[6n\0338");
    fflush(stdout);

    ansi_read_string(0, buffer, sizeof(buffer));

    sscanf(buffer, "\033[%d;%dR", h, w);

    v = mpdm_hget_s(MP, L"window");
    mpdm_hset_s(v, L"tx", MPDM_I(*w));
    mpdm_hset_s(v, L"ty", MPDM_I(*h));
}


static void ansi_sigwinch(int s)
/* SIGWINCH signal handler */
{
    /* get new size */
    ansi_get_tty_size(&g_tx, &g_ty);

    /* (re)attach signal */
    signal(SIGWINCH, ansi_sigwinch);
}


static void ansi_gotoxy(int x, int y)
/* positions the cursor */
{
    printf("\033[%d;%dH", y + 1, x + 1);
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


#if 0
int main(int argc, char *argv[])
{
    ansi_raw_tty(1);

    ansi_sigwinch(0);

    printf("%s", "\033[2J");
    printf("\033[%d;%dH", 10, 10);
    printf("%s", "\033[33;41m");
    printf("LALALA");

    char buffer[32];

    for (;;) {
        int n;

        ansi_read_string(0, buffer, sizeof(buffer));

        if (strcmp(buffer, "q") == 0)
            break;

        if (strcmp(buffer, "1") == 0) {
            printf("\0337\033[r\033[999;999H\033[6n\0338");
        }

        printf("{%s}<", buffer);
        for (n = 0; buffer[n]; n++)
            printf("%02X", buffer[n]);
        printf(">\n");
    }

    ansi_raw_tty(0);

    return 0;
}
#endif


#define ctrl(k) ((k) & 31)

static mpdm_t ansi_getkey(mpdm_t args, mpdm_t ctxt)
{
    char str[32];
    wchar_t *f = NULL;
    mpdm_t k = NULL;

    ansi_read_string(0, str, sizeof(str));

    if (strlen(str) == 1) {
        switch (str[0]) {
        case ctrl('q'):
            f = L"ctrl-q";
            break;
        }
    }
    else {
        if (!strcmp(str, "\033[A"))
            f = L"cursor-up";
        else
        if (!strcmp(str, "\033[B"))
            f = L"cursor-down";
        else
        if (!strcmp(str, "\033[C"))
            f = L"cursor-right";
        else
        if (!strcmp(str, "\033[D"))
            f = L"cursor-left";
    }

    if (f != NULL)
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

//            wattrset(cw, nc_attrs[attr]);
            ansi_print_v(s);
        }
    }

    fflush(stdout);

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
/*    last_attr = mpdm_ival(mpdm_aget(a, 0));

    set_attr();
*/
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
