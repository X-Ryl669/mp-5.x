/*

    Minimum Profit - Programmer Text Editor

    Copyright (C) 1991-2019 Angel Ortega <angel@triptico.com>

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

#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

/** data **/

/* exit requested? */
int mp_exit_requested = 0;

/** private data for drawing syntax-highlighted text **/

struct drw_1_info {
    mpdm_t txt;                 /* the document */
    mpdm_t syntax;              /* syntax highlight information */
    mpdm_t colors;              /* color definitions (for attributes) */
    mpdm_t word_color_func;     /* word color function (just for detection) */
    mpdm_t last_search;         /* last search regex */
    int normal_attr;            /* normal attr */
    int cursor_attr;            /* cursor attr */
    int n_lines;                /* number of processed lines */
    int p_lines;                /* number of prereaded lines */
    int t_lines;                /* total lines in document */
    int vx;                     /* first visible column */
    int vy;                     /* first visible line */
    int tx;                     /* horizontal window size */
    int ty;                     /* vertical window size */
    int tab_size;               /* tabulator size */
    int mod;                    /* modify count */
    int preread_lines;          /* lines to pre-read (for synhi blocks) */
    int mark_eol;               /* mark end of lines */
    int redraw;                 /* redraw trigger */
    int vwrap;                  /* visual wrap value */
    int xoffset;                /* # of columns reserved for line numbers */
};

struct drw_1_info drw_1;
struct drw_1_info drw_1_o;

static struct {
    int x;                      /* cursor x */
    int y;                      /* cursor y */
    int *offsets;               /* offsets of lines */
    char *attrs;                /* attributes */
    int visible;                /* offset to the first visible character */
    int cursor;                 /* offset to cursor */
    wchar_t *ptr;               /* pointer to joined data */
    int size;                   /* size of joined data */
    int matchparen_offset;      /* offset to matched paren */
    int matchparen_o_attr;      /* original attribute there */
    int cursor_o_attr;          /* original attribute under cursor */
    mpdm_t v;                   /* the data */
    mpdm_t old;                 /* the previously generated array */
    int mark_offset;            /* offset to the marked block */
    int mark_size;              /* size of mark_o_attr */
    char *mark_o_attr;          /* saved attributes for the mark */

    wchar_t *cmap;
    int *amap;
    int *vx2x;
    int *vy2y;
    int mx;
    int my;
} drw_2;

/** code **/

#define MP_REAL_TAB_SIZE(x) (drw_1.tab_size - ((x) % drw_1.tab_size))

static int drw_wcwidth(int x, wchar_t c)
/* returns the wcwidth of c, or the tab spaces for
   the x column if it's a tab */
{
    int r;

    switch (c) {
    case L'\n':
        r = 1;
        break;

    case L'\t':
        r = MP_REAL_TAB_SIZE(x);
        break;

    default:
        r = mpdm_wcwidth(c);
        break;
    }

    return r < 0 ? 1 : r;
}


int drw_vx2x(mpdm_t str, int vx)
/* returns the character in str that is on column vx */
{
    const wchar_t *ptr = str->data;
    int n, x;

    for (n = x = 0; n < vx && ptr[x] != L'\0'; x++)
        n += drw_wcwidth(n, ptr[x]);

    return x;
}


int drw_x2vx(mpdm_t str, int x)
/* returns the column where the character at offset x seems to be */
{
    const wchar_t *ptr = str->data;
    int n, vx;

    for (n = vx = 0; n < x && ptr[n] != L'\0'; n++)
        vx += drw_wcwidth(vx, ptr[n]);

    return vx;
}


static int drw_line_offset(int l)
/* returns the offset into v for line number l */
{
    return drw_2.offsets[l - drw_1.vy + drw_1.p_lines];
}


static int drw_adjust_y(int y, int *vy, int ty)
/* adjusts the visual y position */
{
    int t = *vy;

    if (*vy < 0)
        *vy = 0;

    /* is y above the first visible line? */
    if (y < *vy)
        *vy = y;

    /* is y below the last visible line? */
    if (y > *vy + (ty - 2))
        *vy = y - (ty - 2);

    return t != *vy;
}


static int drw_adjust_x(int x, int y, int *vx, int tx, wchar_t * ptr)
/* adjust the visual x position */
{
    int n, m;
    int t = *vx;

    /* calculate the column for the cursor position */
    for (n = m = 0; n < x; n++, ptr++)
        m += drw_wcwidth(m, *ptr);

    /* if new cursor column is nearer the leftmost column, set */
    if (m < *vx)
        *vx = m;

    /* if new cursor column is further the rightmost column, set */
    if (m > *vx + (tx - 1))
        *vx = m - (tx - 1);

    return t != *vx;
}


static int drw_get_attr(wchar_t * color_name)
/* returns the attribute number for a color */
{
    mpdm_t v;
    int attr = 0;

    if ((v = mpdm_hget_s(drw_1.colors, color_name)) != NULL)
        attr = mpdm_ival(mpdm_hget_s(v, L"attr"));

    return attr;
}


static int drw_prepare(mpdm_t doc)
/* prepares the document for screen drawing */
{
    mpdm_t window   = mpdm_hget_s(MP, L"window");
    mpdm_t config   = mpdm_hget_s(MP, L"config");
    mpdm_t txt      = mpdm_hget_s(doc, L"txt");
    mpdm_t lines    = mpdm_hget_s(txt, L"lines");
    int x           = mpdm_ival(mpdm_hget_s(txt, L"x"));
    int y           = mpdm_ival(mpdm_hget_s(txt, L"y"));
    int n;

    drw_1.vx            = mpdm_ival(mpdm_hget_s(txt, L"vx"));
    drw_1.vy            = mpdm_ival(mpdm_hget_s(txt, L"vy"));
    drw_1.tx            = mpdm_ival(mpdm_hget_s(window, L"tx"));
    drw_1.ty            = mpdm_ival(mpdm_hget_s(window, L"ty"));
    drw_1.tab_size      = mpdm_ival(mpdm_hget_s(config, L"tab_size"));
    drw_1.mod           = mpdm_ival(mpdm_hget_s(txt, L"mod"));
    drw_1.preread_lines = mpdm_ival(mpdm_hget_s(config, L"preread_lines"));
    drw_1.mark_eol      = mpdm_ival(mpdm_hget_s(config, L"mark_eol"));
    drw_1.t_lines       = mpdm_size(lines);

    /* adjust the visual y coordinate */
    if (drw_adjust_y(y, &drw_1.vy, drw_1.ty))
        mpdm_hset_s(txt, L"vy", MPDM_I(drw_1.vy));

    /* adjust the visual x coordinate */
    if (drw_adjust_x
        (x, y, &drw_1.vx, drw_1.tx, mpdm_string(mpdm_aget(lines, y))))
        mpdm_hset_s(txt, L"vx", MPDM_I(drw_1.vx));

    /* get the maximum prereadable lines */
    drw_1.p_lines =
        drw_1.vy > drw_1.preread_lines ? drw_1.preread_lines : drw_1.vy;

    /* maximum lines */
    drw_1.n_lines = drw_1.ty + drw_1.p_lines;

    /* get the mp.colors structure and the most used attributes */
    drw_1.colors = mpdm_hget_s(MP, L"colors");
    drw_1.normal_attr = drw_get_attr(L"normal");
    drw_1.cursor_attr = drw_get_attr(L"cursor");

    /* store the syntax highlight structure */
    drw_1.syntax = mpdm_hget_s(doc, L"syntax");

    drw_1.word_color_func = mpdm_hget_s(MP, L"word_color_func");

    mpdm_store(&drw_1.txt, txt);

    drw_2.x = x;
    drw_2.y = y;

    /* last search regex */
    drw_1.last_search = mpdm_hget_s(MP, L"last_search");

    /* redraw trigger */
    drw_1.redraw = mpdm_ival(mpdm_hget_s(MP, L"redraw_counter"));

    /* visual wrap */
    drw_1.vwrap = mpdm_ival(mpdm_hget_s(config, L"visual_wrap"));

    /* calculate number of lines reserved for line numbers */
    n = mpdm_ival(mpdm_hget_s(config, L"show_line_numbers"));

    if (n) {
        char tmp[32];
        sprintf(tmp, " %d ", mpdm_size(lines));
        drw_1.xoffset = strlen(tmp);
    }
    else
        drw_1.xoffset = 0;

    mpdm_hset_s(MP, L"xoffset", MPDM_I(drw_1.xoffset));

    /* compare drw_1 with drw_1_o; if they are the same,
       no more expensive calculations on drw_2 are needed */
    if (memcmp(&drw_1, &drw_1_o, sizeof(drw_1)) == 0)
        return 0;

    /* different; store now */
    memcpy(&drw_1_o, &drw_1, sizeof(drw_1_o));

    /* alloc space for line offsets */
    drw_2.offsets = realloc(drw_2.offsets, drw_1.n_lines * sizeof(int));

    drw_2.ptr = NULL;
    drw_2.size = 0;

    /* add first line */
    drw_2.ptr = mpdm_pokev(drw_2.ptr, &drw_2.size,
                           mpdm_aget(lines, drw_1.vy - drw_1.p_lines));

    /* first line start at 0 */
    drw_2.offsets[0] = 0;

    /* add the following lines and store their offsets */
    for (n = 1; n < drw_1.n_lines; n++) {
        /* add the separator */
        drw_2.ptr =
            mpdm_poke(drw_2.ptr, &drw_2.size, L"\n", 1, sizeof(wchar_t));

        /* this line starts here */
        drw_2.offsets[n] = drw_2.size;

        /* now add it */
        drw_2.ptr = mpdm_pokev(drw_2.ptr, &drw_2.size,
                               mpdm_aget(lines,
                                         n + drw_1.vy - drw_1.p_lines));
    }

    drw_2.ptr = mpdm_poke(drw_2.ptr, &drw_2.size, L"", 1, sizeof(wchar_t));
    drw_2.size--;

    /* now create a value */
    mpdm_store(&drw_2.v, MPDM_ENS(drw_2.ptr, drw_2.size));

    /* alloc and init space for the attributes */
    drw_2.attrs = realloc(drw_2.attrs, drw_2.size + 1);
    memset(drw_2.attrs, drw_1.normal_attr, drw_2.size + 1);

    drw_2.visible = drw_line_offset(drw_1.vy);

    /* (re)create the maps */
    n = (drw_1.tx + 1) * drw_1.ty;
    drw_2.cmap  = realloc(drw_2.cmap, n * sizeof(wchar_t));
    drw_2.amap  = realloc(drw_2.amap, n * sizeof(int));
    drw_2.vx2x  = realloc(drw_2.vx2x, n * sizeof(int));
    drw_2.vy2y  = realloc(drw_2.vy2y, n * sizeof(int));

    drw_2.mx = drw_2.my = -1;

    return 1;
}


static int drw_fill_attr(int attr, int offset, int size)
/* fill an attribute */
{
    if (attr != -1)
        memset(drw_2.attrs + offset, attr, size);

    return offset + size;
}


static int drw_fill_attr_regex(int attr)
/* fills with an attribute the last regex match */
{
    return drw_fill_attr(attr, mpdm_regex_offset, mpdm_regex_size);
}


static void drw_words(void)
/* fills the attributes for separate words */
{
    mpdm_t r, t;
    int o = drw_2.visible;
    mpdm_t word_color = NULL;
    mpdm_t word_color_func = NULL;

    /* take the hash of word colors, if any */
    if ((word_color = mpdm_hget_s(MP, L"word_color")) == NULL)
        return;

    /* get the regex for words */
    if ((r = mpdm_hget_s(MP, L"word_regex")) == NULL)
        return;

    /* get the word color function */
    word_color_func = mpdm_hget_s(MP, L"word_color_func");

    while ((t = mpdm_regex(drw_2.v, r, o)) != NULL) {
        int attr = -1;
        mpdm_t v;

        mpdm_ref(t);

        if ((v = mpdm_hget(word_color, t)) != NULL)
            attr = mpdm_ival(v);
        else if (word_color_func != NULL)
            attr = mpdm_ival(mpdm_exec_1(word_color_func, t, NULL));

        o = drw_fill_attr_regex(attr);

        mpdm_unref(t);
    }
}


static void drw_multiline_regex(mpdm_t a, int attr)
/* sets the attribute to all matching (possibly multiline) regexes */
{
    int n;

    mpdm_ref(a);

    for (n = 0; n < mpdm_size(a); n++) {
        mpdm_t r = mpdm_aget(a, n);
        int o = 0;

        /* if the regex is an array, it's a pair of
           'match from this' / 'match until this' */
        if (r->flags & MPDM_MULTIPLE) {
            mpdm_t rs = mpdm_aget(r, 0);
            mpdm_t re = mpdm_aget(r, 1);

            while (!mpdm_is_null(mpdm_regex(drw_2.v, rs, o))) {
                int s;

                /* fill the matched part */
                o = drw_fill_attr_regex(attr);

                /* try to match the end */
                if (!mpdm_is_null(mpdm_regex(drw_2.v, re, o))) {
                    /* found; fill the attribute
                       to the end of the match */
                    s = mpdm_regex_size + (mpdm_regex_offset - o);
                }
                else {
                    /* not found; fill to the end
                       of the document */
                    s = drw_2.size - o;
                }

                /* fill to there */
                o = drw_fill_attr(attr, o, s);
            }
        }
        else {
            /* it's a scalar */

            if (*mpdm_string(r) == L'%') {
                /* it's a sscanf() expression */
                mpdm_t v;

                while ((v = mpdm_ref(mpdm_sscanf(drw_2.v, r, o)))
                       && mpdm_size(v) == 2) {
                    int i = mpdm_ival(mpdm_aget(v, 0));
                    int s = mpdm_ival(mpdm_aget(v, 1)) - i;

                    o = drw_fill_attr(attr, i, s);

                    mpdm_unref(v);
                }
            }
            else {
                /* it's a regex */
                /* while the regex matches, fill attributes */
                while (!mpdm_is_null(mpdm_regex(drw_2.v, r, o)) && mpdm_regex_size)
                    o = drw_fill_attr_regex(attr);
            }
        }
    }

    mpdm_unref(a);
}


static void drw_blocks(void)
/* fill attributes for multiline blocks */
{
    mpdm_t defs;
    int n;

    /* no definitions? return */
    if (drw_1.syntax == NULL
        || (defs = mpdm_hget_s(drw_1.syntax, L"defs")) == NULL)
        return;

    for (n = 0; n < mpdm_size(defs); n += 2) {
        mpdm_t attr;
        mpdm_t list;

        /* get the attribute */
        attr = mpdm_aget(defs, n);
        attr = mpdm_hget(drw_1.colors, attr);
        attr = mpdm_hget_s(attr, L"attr");

        /* get the list for this word color */
        list = mpdm_aget(defs, n + 1);

        drw_multiline_regex(list, mpdm_ival(attr));
    }
}


static void drw_selection(void)
/* draws the selected block, if any */
{
    mpdm_t mark;
    int bx, by, ex, ey, vertical;
    int so, eo;
    int mby, mey;
    int line_offset, next_line_offset;
    int y;
    int len;
    int attr;

    /* no mark? return */
    if ((mark = mpdm_hget_s(drw_1.txt, L"mark")) == NULL)
        return;

    bx = mpdm_ival(mpdm_hget_s(mark, L"bx"));
    by = mpdm_ival(mpdm_hget_s(mark, L"by"));
    ex = mpdm_ival(mpdm_hget_s(mark, L"ex"));
    ey = mpdm_ival(mpdm_hget_s(mark, L"ey"));
    vertical = mpdm_ival(mpdm_hget_s(mark, L"vertical"));

    /* if block is not visible, return */
    if (ey < drw_1.vy || by >= drw_1.vy + drw_1.ty)
        return;

    so = by < drw_1.vy ? drw_2.visible : drw_line_offset(by) + bx;
    eo = ey >= drw_1.vy + drw_1.ty ? drw_2.size : drw_line_offset(ey) + ex;

    /* alloc space and save the attributes being destroyed */
    drw_2.mark_offset = so;
    drw_2.mark_size = eo - so + 1;
    drw_2.mark_o_attr = malloc(eo - so + 1);
    memcpy(drw_2.mark_o_attr, &drw_2.attrs[so], eo - so + 1);

    if (vertical == 0) {
        /* normal selection */
        drw_fill_attr(drw_get_attr(L"selection"), so, eo - so);
    }
    else {
        /* vertical selection */
        mby = by < drw_1.vy ? drw_1.vy : by;
        mey = ey >= drw_1.vy + drw_1.ty ? drw_1.vy + drw_1.ty : ey;
        line_offset = drw_line_offset(mby);
        attr = drw_get_attr(L"selection");
        for (y = mby; y <= mey; y++) {
            next_line_offset = drw_line_offset(y + 1);
            len = next_line_offset - line_offset - 1;
            so = bx > len ? -1 : bx;
            eo = ex > len ? len : ex;

            if (so >= 0 && eo >= so)
                drw_fill_attr(attr, line_offset + so, eo - so + 1);

            line_offset = next_line_offset;
        }
    }
}


static void drw_search_hit(void)
/* colorize the search hit, if any */
{
    if (drw_1.last_search != NULL) {
        mpdm_t l = MPDM_A(0);

        mpdm_aset(l, drw_1.last_search, 0);
        drw_multiline_regex(l, drw_get_attr(L"search"));
    }
}


static void drw_cursor(void)
/* fill the attribute for the cursor */
{
    /* calculate the cursor offset */
    drw_2.cursor = drw_line_offset(drw_2.y) + drw_2.x;

    drw_2.cursor_o_attr = drw_2.attrs[drw_2.cursor];
    drw_fill_attr(drw_1.cursor_attr, drw_2.cursor, 1);
}


static void drw_matching_paren(void)
/* highlights the matching paren */
{
    int o = drw_2.cursor;
    int i = 0;
    wchar_t c;

    /* by default, no offset has been found */
    drw_2.matchparen_offset = -1;

    /* find the opposite and the increment (direction) */
    switch (drw_2.ptr[o]) {
    case L'(':
        c = L')';
        i = 1;
        break;
    case L'{':
        c = L'}';
        i = 1;
        break;
    case L'[':
        c = L']';
        i = 1;
        break;
    case L')':
        c = L'(';
        i = -1;
        break;
    case L'}':
        c = L'{';
        i = -1;
        break;
    case L']':
        c = L'[';
        i = -1;
        break;
    }

    /* if a direction is set, do the searching */
    if (i) {
        wchar_t s = drw_2.ptr[o];
        int m = 0;
        int l = i == -1 ? drw_2.visible - 1 : drw_2.size;

        while (o != l) {
            if (drw_2.ptr[o] == s) {
                /* found the same */
                m++;
            }
            else if (drw_2.ptr[o] == c) {
                /* found the opposite */
                if (--m == 0) {
                    /* found! fill and exit */
                    drw_2.matchparen_offset = o;
                    drw_2.matchparen_o_attr = drw_2.attrs[o];
                    drw_fill_attr(drw_get_attr(L"matching"), o, 1);
                    break;
                }
            }

            o += i;
        }
    }
}


static mpdm_t drw_push_pair(mpdm_t l, int i, int a, wchar_t * tmp)
/* pushes a pair of attribute / string into l */
{
    /* create the array, if doesn't exist yet */
    if (l == NULL)
        l = MPDM_A(0);

    /* finish the string */
    tmp[i] = L'\0';

    /* special magic: if the attribute is the
       one of the cursor and the string is more than
       one character, create two strings; the
       cursor is over a tab */
    if (a == drw_1.cursor_attr && i > 1) {
        mpdm_push(l, MPDM_I(a));
        mpdm_push(l, MPDM_NS(tmp, 1));

        /* the rest of the string has the original attr */
        a = drw_2.cursor_o_attr;

        /* one char less */
        tmp[i - 1] = L'\0';
    }

    /* store the attribute and the string */
    mpdm_push(l, MPDM_I(a));
    mpdm_push(l, MPDM_S(tmp));

    return l;
}


static wchar_t drw_char(wchar_t c)
{
    if (drw_1.mark_eol) {
        if (c == L'\t')
            c = L'\xb7';
        else
        if (c == L'\n' || c == L'\0')
            c = L'\xb6';
    }
    else {
        if (c == L'\t' || c == L'\n' || c == L'\0')
            c = L' ';
    }

    /* replace form feed with visual representation and
       the rest of control codes with the Unicode replace char */
    if (c == L'\f')
        c = L'\x21a1';
    else
    if (c < L' ')
        c = L'\xfffd';

    return c;
}


static void drw_map_1(int mx, int my, wchar_t c, int a, int x, int y)
{
    int o = mx + my * (drw_1.tx + 1);

    drw_2.cmap[o] = c;
    drw_2.amap[o] = a;
    drw_2.vx2x[o] = x;
    drw_2.vy2y[o] = y;

    if (a == drw_1.cursor_attr) {
        drw_2.mx = mx;
        drw_2.my = my;
    }
}


static void vpos2pos(int mx, int my, int *x, int *y)
{
    if (my < 0) {
        /* above top margin: pick previous line */
        *x = mx;
        *y = drw_1.vy - 2;
    }
    else
    if (my > drw_1.ty - 1) {
        /* below bottom margin: pick next line */
        *x = mx;
        *y = drw_1.vy + drw_1.ty;
    }
    else {
        /* in range: pick from the map */
        int o = mx + my * (drw_1.tx + 1);

        *x = drw_2.vx2x[o];
        *y = drw_2.vy2y[o];
    }
}


static void drw_remap_basic_vwrap(void)
{
    int i = drw_2.offsets[drw_1.p_lines];
    int mx, my;
    int x, y;

    x = 0;
    y = drw_1.vy;

    for (my = 0; my < drw_1.ty; my++) {
        wchar_t c;
        mx = 0;

        do {
            c = drw_2.ptr[i];
            int t = drw_wcwidth(mx, c);
            int n;

            if (c == '\0')
                break;

            for (n = 0; n < t && mx < drw_1.tx; n++)
                drw_map_1(mx++, my, drw_char(c), drw_2.attrs[i], x, y);

            i++;

            if (c == '\n')
                break;

            x++;
        } while (mx < drw_1.tx);

        while (mx < drw_1.tx)
            drw_map_1(mx++, my, '.', -1, x, y);

        if (c == '\n') {
            x = 0;
            y++;
        }
    }
}


static void drw_remap_truncate(void)
{
    int i = drw_2.offsets[drw_1.p_lines];
    int mx, my;
    int x, y;

    x = 0;
    y = drw_1.vy;

    for (my = 0; my < drw_1.ty; my++) {
        wchar_t c;
        int ax = 0;
        mx = 0;

        do {
            c = drw_2.ptr[i];
            int t = drw_wcwidth(mx, c);
            int n;

            if (c == '\0')
                break;

            for (n = 0; n < t && mx < drw_1.tx; n++) {
                if (ax >= drw_1.vx)
                    drw_map_1(mx++, my, drw_char(c), drw_2.attrs[i], x, y);
                ax++;
            }

            i++;

            if (c == '\n')
                break;

            x++;

        } while (mx < drw_1.tx);

        while (mx < drw_1.tx)
            drw_map_1(mx++, my, '.', -1, x, y);

        while (c != '\n' && c != '\0')
            c = drw_2.ptr[i++];

        if (c == '\n') {
            x = 0;
            y++;
        }
    }
}


static mpdm_t drw_remap_to_array(void)
{
    mpdm_t r = MPDM_A(0);
    wchar_t *line = malloc((drw_1.tx + 1) * sizeof(wchar_t));
    int my;
    mpdm_t fmt = NULL;

    if (drw_1.xoffset) {
        fmt = mpdm_ref(mpdm_strcat(MPDM_LS(L" %"), mpdm_strcat_s(MPDM_I(drw_1.xoffset - 2), L"d ")));
    }

    for (my = 0; my < drw_1.ty; my++) {
        mpdm_t l = MPDM_A(0);
        int o = my * (drw_1.tx + 1);
        int mx = 0;

        if (drw_1.xoffset && drw_1.vy + my < drw_1.t_lines) {
            mpdm_push(l, MPDM_I(drw_1.normal_attr));
            mpdm_push(l, mpdm_fmt(fmt, MPDM_I(drw_1.vy + 1 + my)));
        }

        while (mx < drw_1.tx && drw_2.amap[o] != -1) {
            int i = 0;
            int a = drw_2.amap[o];

            while (a == drw_2.amap[o] && mx++ < drw_1.tx)
                line[i++] = drw_2.cmap[o++];

            line[i] = L'\0';

            l = drw_push_pair(l, i, a, line);
        }

        mpdm_push(r, l);
    }

    free(line);

    mpdm_unref(fmt);

    return r;
}


static mpdm_t drw_optimize_array(mpdm_t a, int optimize)
/* optimizes the array, NULLifying all lines that are the same as the last time */
{
    mpdm_t o = drw_2.old;
    mpdm_t r = a;

    mpdm_ref(a);

    if (optimize && o != NULL) {
        int n = 0;

        /* creates a copy */
        r = mpdm_clone(a);

        mpdm_ref(r);

        /* compare each array */
        while (n < mpdm_size(o) && n < mpdm_size(r)) {
            /* if both lines are equal, optimize out */
            if (mpdm_cmp(mpdm_aget(o, n), mpdm_aget(r, n)) == 0)
                mpdm_aset(r, NULL, n);

            n++;
        }

        mpdm_unrefnd(r);
    }

    mpdm_store(&drw_2.old, a);

    mpdm_unref(a);

    return r;
}


static void drw_restore_attrs(void)
/* restored the patched attrs */
{
    /* matching paren, if any */
    if (drw_2.matchparen_offset != -1)
        drw_fill_attr(drw_2.matchparen_o_attr, drw_2.matchparen_offset, 1);

    /* cursor */
    drw_fill_attr(drw_2.cursor_o_attr, drw_2.cursor, 1);

    /* marked block, if any */
    if (drw_2.mark_o_attr != NULL) {
        memcpy(&drw_2.attrs[drw_2.mark_offset], drw_2.mark_o_attr,
               drw_2.mark_size);

        free(drw_2.mark_o_attr);
        drw_2.mark_o_attr = NULL;
    }
}


static mpdm_t drw_draw(mpdm_t doc, int optimize)
/* main document drawing function: takes a document and returns an array of
   arrays of attribute / string pairs */
{
    mpdm_t r = NULL, w;

    if (drw_prepare(doc)) {
        /* colorize separate words */
        drw_words();

        /* colorize multiline blocks */
        drw_blocks();
    }

    /* now set the marked block (if any) */
    drw_selection();

    /* colorize the search hit */
    drw_search_hit();

    /* the cursor */
    drw_cursor();

    /* highlight the matching paren */
    drw_matching_paren();

    /* convert to an array of string / atribute pairs */
    if (drw_1.vwrap == 1)
        drw_remap_basic_vwrap();
    else
        drw_remap_truncate();

    r = drw_remap_to_array();

    /* optimize */
    r = drw_optimize_array(r, optimize);

    /* restore the patched attrs */
    drw_restore_attrs();

    w = mpdm_hget_s(MP, L"window");
    mpdm_hset_s(w, L"mx", MPDM_I(drw_2.mx));
    mpdm_hset_s(w, L"my", MPDM_I(drw_2.my));

    return r;
}


/** interface **/

mpdm_t mp_draw(mpdm_t doc, int optimize)
/* main generic drawing function for drivers */
{
    mpdm_t f, r = NULL;
    static mpdm_t d = NULL;

    if (doc != d) {
        optimize = 0;
        mpdm_store(&d, doc);
    }

    if ((f = mpdm_hget_s(doc, L"render")) != NULL) {
        /* create a context to contain the object itself
           (i.e. call as a method) */
        mpdm_t ctxt = MPDM_A(0);

        mpdm_push(ctxt, doc);
        r = mpdm_exec_2(f, doc, MPDM_I(optimize), ctxt);
    }

    return r;
}


#define THR_SPEED_STEP	10
#define THR_MAX_SPEED	7

int mp_keypress_throttle(int keydown)
/* processes key acceleration and throttle */
{
#ifdef USE_THROTTLE
    static int keydowns = 0;
    static int seq = 0;
    int redraw = 0;

    if (keydown) {
        int speed;

        /* as keydowns accumulate, speed increases, which is the number
           of cycles the redraw will be skipped (up to a maximum) */
        if ((speed = 1 + (++keydowns / THR_SPEED_STEP)) > THR_MAX_SPEED)
            speed = THR_MAX_SPEED;

        if (++seq % speed == 0)
            redraw = 1;
    }
    else {
        if (keydowns > 1)
            redraw = 1;

        keydowns = 0;
    }

    return redraw;

#else /* USE_THROTTLE */
    return 1;
#endif /* USE_THROTTLE */
}


mpdm_t mp_active(void)
/* interface to mp.active() */
{
    return mpdm_exec(mpdm_hget_s(MP, L"active"), NULL, NULL);
}


void mp_process_action(mpdm_t action)
/* interface to mp.process_action() */
{
    mpdm_void(mpdm_exec_1
              (mpdm_hget_s(MP, L"process_action"), action, NULL));
}


void mp_process_event(mpdm_t keycode)
/* interface to mp.process_event() */
{
    mpdm_void(mpdm_exec_1
              (mpdm_hget_s(MP, L"process_event"), keycode, NULL));
}


void mp_set_y(mpdm_t doc, int y)
/* interface to mp.set_y() */
{
    mpdm_void(mpdm_exec_2
              (mpdm_hget_s(doc, L"set_y"), doc, MPDM_I(y), NULL));
}


mpdm_t mp_build_status_line(void)
/* interface to mp.build_status_line() */
{
    return mpdm_exec(mpdm_hget_s(MP, L"build_status_line"), NULL, NULL);
}


mpdm_t mp_get_history(mpdm_t key)
/* interface to mp.get_history() */
{
    return mpdm_exec_1(mpdm_hget_s(MP, L"get_history"), key, NULL);
}


mpdm_t mp_get_doc_names(void)
/* interface to mp.get_doc_names() */
{
    return mpdm_exec(mpdm_hget_s(MP, L"get_doc_names"), NULL, NULL);
}


mpdm_t mp_menu_label(mpdm_t action)
/* interface to mp.menu_label() */
{
    return mpdm_exec_1(mpdm_hget_s(MP, L"menu_label"), action, NULL);
}


mpdm_t mp_c_exit(mpdm_t args, mpdm_t ctxt)
/* exit the editor (set mp_exit_requested) */
{
    mp_exit_requested = 1;

    return NULL;
}


static mpdm_t mp_c_exit_requested(mpdm_t args, mpdm_t ctxt)
/* returns the value of the mp_exit_requested variable */
{
    return MPDM_I(mp_exit_requested);
}


mpdm_t mp_c_render(mpdm_t args, mpdm_t ctxt)
{
    return drw_draw(mpdm_aget(args, 0), mpdm_ival(mpdm_aget(args, 1)));
}


mpdm_t mp_c_vx2x(mpdm_t args, mpdm_t ctxt)
/* interface to drw_vx2x() */
{
    return
        MPDM_I(drw_vx2x
               (mpdm_aget(args, 0), mpdm_ival(mpdm_aget(args, 1))));
}


mpdm_t mp_c_x2vx(mpdm_t args, mpdm_t ctxt)
/* interface to drw_x2vx() */
{
    return
        MPDM_I(drw_x2vx
               (mpdm_aget(args, 0), mpdm_ival(mpdm_aget(args, 1))));
}


mpdm_t mp_c_vpos2pos(mpdm_t args, mpdm_t ctxt)
{
    mpdm_t r = MPDM_A(2);
    int x = mpdm_ival(mpdm_aget(args, 0));
    int y = mpdm_ival(mpdm_aget(args, 1));

    vpos2pos(x, y, &x, &y);

    mpdm_aset(r, MPDM_I(x), 0);
    mpdm_aset(r, MPDM_I(y), 1);

    return r;
}


mpdm_t mp_c_plain_load(mpdm_t args, mpdm_t ctxt)
/* loads a plain file into an array (highly optimized one) */
{
    mpdm_t f = mpdm_aget(args, 0);
    mpdm_t a;
    mpdm_t v;
    int chomped = 1;
    int eol = 1;

    a = MPDM_A(0);

    /* clean last seen EOL */
    mpdm_hset_s(MP, L"last_seen_eol", NULL);

    /* NOTE: this code rewrites a value, which is *illegal*,
       to avoid generating too much residual values */
    while ((v = mpdm_read(f)) != NULL) {
        wchar_t *ptr = (wchar_t *) v->data;
        int size = v->size;

        /* chomp */
        if (size && ptr[size - 1] == L'\n') {
            if (--size && ptr[size - 1] == L'\r') {
                --size;
                eol = 2;
            }

            ptr[size] = L'\0';
            v->size = size;
        }
        else
            chomped = 0;

        mpdm_push(a, v);
    }

    /* if last line was chomped, add a last, empty one */
    if (chomped)
        mpdm_push(a, MPDM_LS(L""));

    /* store the last seen EOL */
    mpdm_hset_s(MP, L"last_seen_eol", MPDM_LS(eol == 2 ? L"\r\n" : L"\n"));

    return a;
}


mpdm_t mp_c_search_hex(mpdm_t args, mpdm_t ctxt)
/* search the hex string str in the file */
{
    mpdm_t fd = mpdm_aget(args, 0);
    mpdm_t str = mpdm_aget(args, 1);
    FILE *f = mpdm_get_filehandle(fd);
    wchar_t *s = mpdm_string(str);
    int n = 0;
    unsigned char *ptr;
    off_t o;
    int found = 0;

    /* parse str into a binary buffer */
    ptr = malloc(wcslen(s) + 1);
    while (s[0] && s[1]) {
        char tmp[3];
        int c;

        tmp[0] = (char)s[0];
        tmp[1] = (char)s[1];
        sscanf(tmp, "%02x", &c);

        ptr[n++] = (unsigned char) c;
        s += 2;
    }
    ptr[n] = 0;

    /* start searching */
    o = ftell(f);
    while (!found && !feof(f)) {
        int c;

        fseek(f, o, 0);
        n = 0;

        while (!found && (c = fgetc(f)) != EOF) {
            if (c == ptr[n]) {
                n++;

                if (ptr[n] == '\0')
                    found = 1;
            }
            else {
                o++;
                break;
            }
        }
    }

    if (found)
        fseek(f, o, 0);

    free(ptr);

    return MPDM_I(found);
}


#ifndef CONFOPT_EXTERNAL_TAR

extern const char _binary_mp_tar_start;
extern const char _binary_mp_tar_end;
extern const char binary_mp_tar_start;
extern const char binary_mp_tar_end;

static mpdm_t find_in_embedded_tar(mpdm_t args, mpdm_t ctxt)
/* searches for embedded MPSL code */
{
    mpdm_t f;
    mpdm_t r = NULL;

    f = mpdm_ref(MPDM_2MBS(mpdm_string(mpdm_aget(args, 0))));
    r = mpsl_find_in_embedded_tar((const char *)f->data,

#ifdef CONFOPT_EMBED_NOUNDER
        &binary_mp_tar_start, &binary_mp_tar_end);
#else
        &_binary_mp_tar_start, &_binary_mp_tar_end);
#endif

    mpdm_unref(f);

    return r;
}

#endif

void mp_startup(int argc, char *argv[])
{
    mpdm_t INC;
    char *ptr;
    mpdm_t mp_c;

    mpdm_startup();
    mpdm_hset_s(mpdm_root(), L"APPID", MPDM_MBS(CONFOPT_APPNAME));

    mpsl_startup();

    /* reset the structures */
    memset(&drw_1, '\0', sizeof(drw_1));
    memset(&drw_1_o, '\0', sizeof(drw_1_o));

    /* new mp_c namespace (C interface) */
    mp_c = mpdm_hset_s(mpdm_root(), L"mp_c", MPDM_H(0));

    /* version */
    mpdm_hset_s(mp_c, L"VERSION",           MPDM_S(L"" VERSION));
    mpdm_hset_s(mp_c, L"x2vx",              MPDM_X(mp_c_x2vx));
    mpdm_hset_s(mp_c, L"vx2x",              MPDM_X(mp_c_vx2x));
    mpdm_hset_s(mp_c, L"vpos2pos",          MPDM_X(mp_c_vpos2pos));
    mpdm_hset_s(mp_c, L"exit",              MPDM_X(mp_c_exit));
    mpdm_hset_s(mp_c, L"plain_load",        MPDM_X(mp_c_plain_load));
    mpdm_hset_s(mp_c, L"exit_requested",    MPDM_X(mp_c_exit_requested));
    mpdm_hset_s(mp_c, L"render",            MPDM_X(mp_c_render));
    mpdm_hset_s(mp_c, L"search_hex",        MPDM_X(mp_c_search_hex));

    /* creates the INC (executable path) array */
    INC = mpdm_hset_s(mpdm_root(), L"INC", MPDM_A(0));

    /* if the MP_LIBRARY_PATH environment variable is set,
       put it before anything else */
    if ((ptr = getenv("MP_LIBRARY_PATH")) != NULL)
        mpdm_push(INC, MPDM_MBS(ptr));

#ifdef CONFOPT_EXTERNAL_TAR
    /* add code library as externally installed tar */
    mpdm_push(INC, mpdm_strcat_s(
        mpdm_hget_s(mpdm_root(), L"APPDIR"), L"/mp-dist.tar"));

#else /* CONFOPT_EXTERNAL_TAR */

    /* add code library as embedded tar */
    mpdm_push(INC, MPDM_X(find_in_embedded_tar));

#endif /* CONFOPT_EXTERNAL_TAR */

    if (!TRY_DRIVERS()) {
        printf("No usable driver found; exiting.\n");
        exit(1);
    }

    mpsl_argv(argc, argv);
}


void mp_mpsl(void)
{
    mpdm_t e;

    mpsl_eval(MPDM_LS(L"load('mp_core.mpsl');"), NULL, NULL);

    if ((e = mpdm_hget_s(mpdm_root(), L"ERROR")) != NULL) {
        mpdm_write_wcs(stdout, mpdm_string(e));
        printf("\n");
    }
}


void mp_shutdown(void)
{
    /* unref pending values */
    mpdm_unref(drw_1.txt);
    mpdm_unref(drw_2.v);
    mpdm_unref(drw_2.old);

#ifdef DEBUG_CLEANUP
    mpdm_unref(mpdm_root());
#endif

    mpsl_shutdown();
}


int main(int argc, char *argv[])
{
    mp_startup(argc, argv);

    mp_mpsl();

    mp_shutdown();

    return 0;
}
