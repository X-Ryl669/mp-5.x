/*

    Minimum Profit - Programmer Text Editor

    Qt4 (and Qt5) driver.

    Copyright (C) 2009/2019 Angel Ortega <angel@triptico.com> et al.

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

/* override auto-generated definition in config.h */
extern "C" int qt4_drv_detect(int *argc, char ***argv);
extern "C" int qt5_drv_detect(int *argc, char ***argv);

#include "config.h"

#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include "mpdm.h"
#include "mpsl.h"
#include "mp.h"
#include "mp.xpm"

#ifdef CONFOPT_QT5
#include <QtWidgets>
#else
#include <QtGui>
#endif

/** data **/

class MPWindow : public QMainWindow
{
public:
    MPWindow(QWidget * parent = 0);
    bool queryExit(void);
    void closeEvent(QCloseEvent *event);
    bool event(QEvent * event);
};

/* global data */
QApplication *app;
MPWindow *window;
QMenuBar *menubar;

#define MENU_CLASS QMenu

#include "mpv_qk_common.cpp"

/** MPWindow methods **/

MPWindow::MPWindow(QWidget * parent) : QMainWindow(parent)
{
    QVBoxLayout *vb;
    QHBoxLayout *hb;
    int height;

    setWindowTitle("mp " VERSION);

    menubar = this->menuBar();
    qk_build_menu();

    /* pick an optimal height for the menu & tabs */
    height = menubar->sizeHint().height();

    /* top area */
    hb = new QHBoxLayout();
    hb->setContentsMargins(0, 0, 0, 0);

    hb->addWidget(menubar);
    QWidget *ta = new QWidget();
    ta->setLayout(hb);
    ta->setMaximumHeight(height);

    /* main area */
    hb = new QHBoxLayout();
    hb->setContentsMargins(0, 0, 0, 0);

    area = new MPArea();

    hb->addWidget(area);
    hb->addWidget(area->scrollbar);
    QWidget *cc = new QWidget();
    cc->setLayout(hb);

    /* the full container */
    vb = new QVBoxLayout();

    vb->addWidget(ta);
    vb->addWidget(area->file_tabs);
    vb->addWidget(cc);

    QWidget *mc = new QWidget();
    mc->setLayout(vb);

    this->statusBar()->addWidget(area->statusbar);

    setCentralWidget(mc);

    connect(area->scrollbar, SIGNAL(valueChanged(int)),
            area, SLOT(from_scrollbar(int)));

    connect(area->file_tabs, SIGNAL(currentChanged(int)),
            area, SLOT(from_filetabs(int)));

    connect(menubar, SIGNAL(triggered(QAction *)),
            area, SLOT(from_menu(QAction *)));

    this->setWindowIcon(QIcon(QPixmap(mp_xpm)));

    mpdm_t st = mp_load_save_state("r");
    if ((st = mpdm_hget_s(st, L"window")) == NULL) {
        st = mpdm_hset_s(mpdm_hget_s(MP, L"state"), L"window", MPDM_H(0));
        mpdm_hset_s(st, L"x", MPDM_I(20));
        mpdm_hset_s(st, L"y", MPDM_I(20));
        mpdm_hset_s(st, L"w", MPDM_I(600));
        mpdm_hset_s(st, L"h", MPDM_I(400));
    }

    move(QPoint(mpdm_ival(mpdm_hget_s(st, L"x")), mpdm_ival(mpdm_hget_s(st, L"y"))));
    resize(QSize(mpdm_ival(mpdm_hget_s(st, L"w")), mpdm_ival(mpdm_hget_s(st, L"h"))));
}


static void save_settings(MPWindow *w)
{
    mpdm_t v;

    v = mpdm_hget_s(MP, L"state");
    v = mpdm_hset_s(v, L"window", MPDM_H(0));
    mpdm_hset_s(v, L"x", MPDM_I(w->pos().x()));
    mpdm_hset_s(v, L"y", MPDM_I(w->pos().y()));
    mpdm_hset_s(v, L"w", MPDM_I(w->size().width()));
    mpdm_hset_s(v, L"h", MPDM_I(w->size().height()));

    mp_load_save_state("w");
}


bool MPWindow::queryExit(void)
{
    mp_process_event(MPDM_LS(L"close-window"));

    save_settings(this);

    return mp_exit_requested ? true : false;
}


void MPWindow::closeEvent(QCloseEvent *event)
{
    mp_process_event(MPDM_LS(L"close-window"));

    if (!mp_exit_requested)
        event->ignore();
}


bool MPWindow::event(QEvent *event)
{
    /* do the processing */
    bool r = QWidget::event(event);

    if (mp_exit_requested) {
        save_settings(this);
        qt4_drv_shutdown(NULL, NULL);
        QApplication::exit(0);
    }

    return r;
}


/** driver functions **/

static mpdm_t qt4_drv_alert(mpdm_t a, mpdm_t ctxt)
{
    /* 1# arg: prompt */
    QMessageBox::information(window, "mp " VERSION,
                             v_to_qstring(mpdm_aget(a, 0)));

    return NULL;
}


static mpdm_t qt4_drv_confirm(mpdm_t a, mpdm_t ctxt)
{
    int r;

    /* 1# arg: prompt */
    r = QMessageBox::question(window, "mp" VERSION,
                              v_to_qstring(mpdm_aget(a, 0)),
                              QMessageBox::Yes | QMessageBox::
                              No | QMessageBox::Cancel);

    switch (r) {
    case QMessageBox::Yes:
        r = 1;
        break;

    case QMessageBox::No:
        r = 2;
        break;

    case QMessageBox::Cancel:
        r = 0;
        break;
    }

    return MPDM_I(r);
}


static mpdm_t qt4_drv_openfile(mpdm_t a, mpdm_t ctxt)
{
    QString r;
    char tmp[128];

    getcwd(tmp, sizeof(tmp));

    /* 1# arg: prompt */
    r = QFileDialog::getOpenFileName(window,
                                     v_to_qstring(mpdm_aget(a, 0)), tmp);

    return qstring_to_v(r);
}


static mpdm_t qt4_drv_savefile(mpdm_t a, mpdm_t ctxt)
{
    QString r;
    char tmp[128];

    getcwd(tmp, sizeof(tmp));

    /* 1# arg: prompt */
    r = QFileDialog::getSaveFileName(window,
                                     v_to_qstring(mpdm_aget(a, 0)), tmp);

    return qstring_to_v(r);
}


static mpdm_t qt4_drv_openfolder(mpdm_t a, mpdm_t ctxt)
{
    QString r;
    char tmp[128];

    getcwd(tmp, sizeof(tmp));

    /* 1# arg: prompt */
    r = QFileDialog::getExistingDirectory(window,
                                    v_to_qstring(mpdm_aget(a, 0)),
                                    tmp,
                                    QFileDialog::ShowDirsOnly);

    return qstring_to_v(r);
}


class MPForm : public QDialog
{
public:
    QDialogButtonBox *button_box;

    MPForm(QWidget * parent = 0) : QDialog(parent)
    {
        button_box = new QDialogButtonBox(QDialogButtonBox::Ok |
                                          QDialogButtonBox::Cancel);

        connect(button_box, SIGNAL(accepted()), this, SLOT(accept()));
        connect(button_box, SIGNAL(rejected()), this, SLOT(reject()));
    }
};


static mpdm_t qt4_drv_form(mpdm_t a, mpdm_t ctxt)
{
    int n;
    mpdm_t widget_list;
    QWidget *qlist[100];
    mpdm_t r = NULL;

    MPForm *dialog = new MPForm(window);
    dialog->setWindowTitle("mp " VERSION);

    dialog->setModal(true);

    widget_list = mpdm_aget(a, 0);

    QWidget *form = new QWidget();
    QFormLayout *fl = new QFormLayout();

    for (n = 0; n < mpdm_size(widget_list); n++) {
        mpdm_t w = mpdm_aget(widget_list, n);
        wchar_t *type;
        mpdm_t t;
        QLabel *ql = new QLabel("");

        type = mpdm_string(mpdm_hget_s(w, L"type"));

        if ((t = mpdm_hget_s(w, L"label")) != NULL) {
            ql->setText(v_to_qstring(mpdm_gettext(t)));
        }

        t = mpdm_hget_s(w, L"value");

        if (wcscmp(type, L"text") == 0) {
            mpdm_t h;
            QComboBox *qc = new QComboBox();

            qc->setEditable(true);
            qc->setMinimumContentsLength(30);
            qc->setMaxVisibleItems(8);

            if (t != NULL)
                qc->setEditText(v_to_qstring(t));

            qlist[n] = qc;

            if ((h = mpdm_hget_s(w, L"history")) != NULL) {
                int i;

                /* has history; fill it */
                h = mp_get_history(h);

                for (i = mpdm_size(h) - 1; i >= 0; i--)
                    qc->addItem(v_to_qstring(mpdm_aget(h, i)));
            }

            /* select all the editable field */
            qc->lineEdit()->selectAll();

            fl->addRow(ql, qc);
        }
        else
        if (wcscmp(type, L"password") == 0) {
            QLineEdit *qe = new QLineEdit();

            qe->setEchoMode(QLineEdit::Password);

            qlist[n] = qe;

            fl->addRow(ql, qe);
        }
        else
        if (wcscmp(type, L"checkbox") == 0) {
            QCheckBox *qc = new QCheckBox();

            if (mpdm_ival(t))
                qc->setCheckState(Qt::Checked);

            qlist[n] = qc;

            fl->addRow(ql, qc);
        }
        else
        if (wcscmp(type, L"list") == 0) {
            int i;
            QListWidget *qlw = new QListWidget();
            qlw->setMinimumWidth(480);

            /* use a monospaced font */
            qlw->setFont(QFont(QString("Mono")));

            mpdm_t l = mpdm_hget_s(w, L"list");

            for (i = 0; i < mpdm_size(l); i++)
                qlw->addItem(v_to_qstring(mpdm_aget(l, i)));

            qlw->setCurrentRow(mpdm_ival(t));

            qlist[n] = qlw;

            fl->addRow(ql, qlw);
        }

        if (n == 0)
            qlist[n]->setFocus(Qt::OtherFocusReason);
    }

    form->setLayout(fl);

    QVBoxLayout *ml = new QVBoxLayout();
    ml->addWidget(form);
    ml->addWidget(dialog->button_box);

    dialog->setLayout(ml);

    if (dialog->exec()) {
        r = MPDM_A(mpdm_size(widget_list));

        /* fill the return values */
        for (n = 0; n < mpdm_size(widget_list); n++) {
            mpdm_t w = mpdm_aget(widget_list, n);
            mpdm_t v = NULL;
            wchar_t *type;

            type = mpdm_string(mpdm_hget_s(w, L"type"));

            if (wcscmp(type, L"text") == 0) {
                mpdm_t h;
                QComboBox *ql = (QComboBox *) qlist[n];

                v = mpdm_ref(qstring_to_v(ql->currentText()));

                /* if it has history, add to it */
                if (v && (h = mpdm_hget_s(w, L"history")) && mpdm_cmp_s(v, L"")) {
                    h = mp_get_history(h);

                    if (mpdm_cmp(v, mpdm_aget(h, -1)) != 0)
                        mpdm_push(h, v);
                }

                mpdm_unrefnd(v);
            }
            else
            if (wcscmp(type, L"password") == 0) {
                QLineEdit *ql = (QLineEdit *) qlist[n];

                v = qstring_to_v(ql->text());
            }
            else
            if (wcscmp(type, L"checkbox") == 0) {
                QCheckBox *qb = (QCheckBox *) qlist[n];

                v = MPDM_I(qb->checkState() == Qt::Checked);
            }
            else
            if (wcscmp(type, L"list") == 0) {
                QListWidget *ql = (QListWidget *) qlist[n];

                v = MPDM_I(ql->currentRow());
            }

            mpdm_aset(r, v, n);
        }
    }

    return r;
}


static void qt4_register_functions(void)
{
    mpdm_t drv;

    drv = mpdm_hget_s(mpdm_root(), L"mp_drv");
    mpdm_hset_s(drv, L"main_loop",   MPDM_X(qt4_drv_main_loop));
    mpdm_hset_s(drv, L"shutdown",    MPDM_X(qt4_drv_shutdown));
    mpdm_hset_s(drv, L"clip_to_sys", MPDM_X(qt4_drv_clip_to_sys));
    mpdm_hset_s(drv, L"sys_to_clip", MPDM_X(qt4_drv_sys_to_clip));
    mpdm_hset_s(drv, L"update_ui",   MPDM_X(qt4_drv_update_ui));
    mpdm_hset_s(drv, L"timer",       MPDM_X(qt4_drv_timer));
    mpdm_hset_s(drv, L"busy",        MPDM_X(qt4_drv_busy));
    mpdm_hset_s(drv, L"alert",       MPDM_X(qt4_drv_alert));
    mpdm_hset_s(drv, L"confirm",     MPDM_X(qt4_drv_confirm));
    mpdm_hset_s(drv, L"openfile",    MPDM_X(qt4_drv_openfile));
    mpdm_hset_s(drv, L"savefile",    MPDM_X(qt4_drv_savefile));
    mpdm_hset_s(drv, L"form",        MPDM_X(qt4_drv_form));
    mpdm_hset_s(drv, L"openfolder",  MPDM_X(qt4_drv_openfolder));
}


static mpdm_t qt4_drv_startup(mpdm_t a, mpdm_t ctxt)
/* driver initialization */
{
    qt4_register_functions();

    qk_build_colors();

    window = new MPWindow();
    window->show();

    return NULL;
}


#ifdef CONFOPT_QT4
extern "C" Display *XOpenDisplay(char *);

extern "C" int qt4_drv_detect(int *argc, char ***argv)
{
    int n, ret = 1;

    for (n = 0; n < *argc; n++) {
        if (strcmp(argv[0][n], "-txt") == 0)
            ret = 0;
    }

    if (ret) {
        Display *x11_display;

        /* try connecting directly to the Xserver */
        if ((x11_display = XOpenDisplay((char *) NULL))) {
            mpdm_t drv;

            /* this is where it crashes if no X server */
            app = new QApplication(x11_display);

            drv = mpdm_hset_s(mpdm_root(), L"mp_drv", MPDM_H(0));
            mpdm_hset_s(drv, L"id",      MPDM_LS(L"qt4"));
            mpdm_hset_s(drv, L"startup", MPDM_X(qt4_drv_startup));
        }
        else
            ret = 0;
    }

    return ret;
}

#endif


extern "C" int qt5_drv_detect(int *argc, char ***argv)
{
    int n, ret = 1;

    for (n = 0; n < *argc; n++) {
        if (strcmp(argv[0][n], "-txt") == 0)
            ret = 0;
    }

    if (ret) {
        char *display;

        if ((display = getenv("DISPLAY")) && *display) {
            mpdm_t drv;

            app = new QApplication(*argc, *argv);

            drv = mpdm_hset_s(mpdm_root(), L"mp_drv", MPDM_H(0));
            mpdm_hset_s(drv, L"id",      MPDM_LS(L"qt5"));
            mpdm_hset_s(drv, L"startup", MPDM_X(qt4_drv_startup));
        }
        else
            ret = 0;
    }

    return ret;
}

