/*

    Minimum Profit - A Text Editor
    Qt4 (and Qt5) driver.

    Angel Ortega <angel@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

/* override auto-generated definition in config.h */
extern "C" int qt4_drv_detect(int *argc, char ***argv);
extern "C" int qt5_drv_detect(int *argc, char ***argv);

#include "config.h"

#include <stdio.h>
#include <wchar.h>
#include <unistd.h>

#define MPDM_OLD_COMPAT

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

#define MENU_CLASS QMenu
#define MENUBAR_CLASS QMenuBar

#include "mpv_qk_common.cpp"

class MPWindow : public QMainWindow
{
public:
    MPWindow(QWidget * parent = 0);
    ~MPWindow(void);
    bool queryExit(void);
    void closeEvent(QCloseEvent *event);
    bool event(QEvent * event);
    void save_settings(void);
    MPArea *area;
};

QApplication *app;
MPWindow *window;


/** MPWindow methods **/

MPWindow::MPWindow(QWidget * parent) : QMainWindow(parent)
{
    QVBoxLayout *vb;
    QHBoxLayout *hb;

    setWindowTitle("mp " VERSION);

//    menubar = this->menuBar();
    qk_build_menu(this->menuBar());

    /* pick an optimal height for the menu & tabs */
//    int height = menubar->sizeHint().height();

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

    connect(this->menuBar(), SIGNAL(triggered(QAction *)),
            area, SLOT(from_menu(QAction *)));

    this->setWindowIcon(QIcon(QPixmap(mp_xpm)));

    mpdm_t v = mpdm_hget_s(MP, L"state");
    if ((v = mpdm_hget_s(v, L"window")) == NULL) {
        v = mpdm_hset_s(mpdm_hget_s(MP, L"state"), L"window", MPDM_H(0));
        mpdm_hset_s(v, L"x", MPDM_I(20));
        mpdm_hset_s(v, L"y", MPDM_I(20));
        mpdm_hset_s(v, L"w", MPDM_I(600));
        mpdm_hset_s(v, L"h", MPDM_I(400));
    }

    move(QPoint(mpdm_ival(mpdm_hget_s(v, L"x")), mpdm_ival(mpdm_hget_s(v, L"y"))));
    resize(QSize(mpdm_ival(mpdm_hget_s(v, L"w")), mpdm_ival(mpdm_hget_s(v, L"h"))));
}


MPWindow::~MPWindow(void)
{
}


void MPWindow::save_settings(void)
{
    mpdm_t v;

    v = mpdm_hget_s(MP, L"state");
    v = mpdm_hset_s(v, L"window", MPDM_H(0));
    mpdm_hset_s(v, L"x", MPDM_I(pos().x()));
    mpdm_hset_s(v, L"y", MPDM_I(pos().y()));
    mpdm_hset_s(v, L"w", MPDM_I(size().width()));
    mpdm_hset_s(v, L"h", MPDM_I(size().height()));
}


bool MPWindow::queryExit(void)
{
    mp_process_event(MPDM_S(L"close-window"));

    return mp_exit_requested ? true : false;
}


void MPWindow::closeEvent(QCloseEvent *event)
{
    mp_process_event(MPDM_S(L"close-window"));

    if (!mp_exit_requested)
        event->ignore();
}


static mpdm_t qt4_drv_shutdown(mpdm_t a, mpdm_t ctxt);

bool MPWindow::event(QEvent *event)
{
    /* do the processing */
    bool r = QWidget::event(event);

    if (mp_exit_requested) {
        save_settings();
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

    for (n = 0; n < (int) mpdm_size(widget_list); n++) {
        mpdm_t w = mpdm_aget(widget_list, n);
        wchar_t *type;
        mpdm_t t;
        QLabel *ql = new QLabel("");
        QWidget *qw = NULL;

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

            if ((h = mpdm_hget_s(w, L"history")) != NULL) {
                int i;

                /* has history; fill it */
                h = mp_get_history(h);

                for (i = 0; i < (int) mpdm_size(h); i++)
                    qc->addItem(v_to_qstring(mpdm_aget(h, i)));

                qc->setCurrentIndex(mpdm_size(h) - 1);
            }

            /* select all the editable field */
            qc->lineEdit()->selectAll();

            qw = qc;
        }
        else
        if (wcscmp(type, L"password") == 0) {
            QLineEdit *qe = new QLineEdit();

            qe->setEchoMode(QLineEdit::Password);

            qw = qe;
        }
        else
        if (wcscmp(type, L"checkbox") == 0) {
            QCheckBox *qc = new QCheckBox();

            if (mpdm_ival(t))
                qc->setCheckState(Qt::Checked);

            qw = qc;
        }
        else
        if (wcscmp(type, L"list") == 0) {
            int i;
            QTableWidget *qtw = new QTableWidget();
            qtw->setSelectionBehavior(QAbstractItemView::SelectRows);
            qtw->setShowGrid(false);

            qtw->setMinimumWidth(600);

            /* if it's the only widget, make it tall */
            if (mpdm_size(widget_list) == 1)
                qtw->setMinimumHeight(400);

            qtw->horizontalHeader()->hide();
            qtw->horizontalHeader()->setStretchLastSection(true);
            qtw->verticalHeader()->hide();

            mpdm_t l = mpdm_hget_s(w, L"list");

            qtw->setColumnCount(2);
            qtw->setRowCount(mpdm_size(l));

            for (i = 0; i < (int) mpdm_size(l); i++) {
                QTableWidgetItem *qtwi;
                wchar_t *ptr1, *ptr2;

                ptr1 = wcsdup(mpdm_string(mpdm_aget(l, i)));
                if ((ptr2 = wcschr(ptr1, L'\t'))) {
                    *ptr2 = L'\0';
                    ptr2++;
                }
                else
                    ptr2 = (wchar_t *)L"";

                qtwi = new QTableWidgetItem(QString::fromWCharArray(ptr1));
                qtw->setItem(i, 0, qtwi);

                qtwi = new QTableWidgetItem(QString::fromWCharArray(ptr2));
                qtwi->setTextAlignment(Qt::AlignRight);
                qtw->setItem(i, 1, qtwi);

                qtw->setRowHeight(i, 24);

                free(ptr1);
            }

            qtw->selectRow(mpdm_ival(t));

            qtw->resizeColumnsToContents();

            qw = qtw;
        }

        qlist[n] = qw;

        if (mpdm_size(widget_list) == 1) {
            fl->addRow(ql);
            fl->addRow(qw);
        }
        else
            fl->addRow(ql, qw);

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
        for (n = 0; n < (int) mpdm_size(widget_list); n++) {
            mpdm_t w = mpdm_aget(widget_list, n);
            mpdm_t v = NULL;
            wchar_t *type;

            type = mpdm_string(mpdm_hget_s(w, L"type"));

            if (wcscmp(type, L"text") == 0) {
                mpdm_t h;
                QComboBox *ql = (QComboBox *) qlist[n];

                v = mpdm_ref(qstring_to_v(ql->currentText()));

                /* if it has history, add to it */
                if (v && (h = mpdm_hget_s(w, L"history")) && mpdm_cmp_wcs(v, L"")) {
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


static mpdm_t qt4_drv_update_ui(mpdm_t a, mpdm_t ctxt)
{
    window->area->font = qk_build_font();
    qk_build_colors();
    qk_build_menu(window->menuBar());

    window->area->ls_width = -1;
    window->area->ls_height = -1;
    window->area->update();

    return NULL;
}


static mpdm_t qt4_drv_busy(mpdm_t a, mpdm_t ctxt)
{
    int onoff = mpdm_ival(mpdm_aget(a, 0));

    window->setCursor(onoff ? Qt::WaitCursor : Qt::ArrowCursor);

    return NULL;
}


static mpdm_t qt4_drv_main_loop(mpdm_t a, mpdm_t ctxt)
{
    app->exec();

    return NULL;
}


static mpdm_t qt4_drv_timer(mpdm_t a, mpdm_t ctxt)
{
    int msecs = mpdm_ival(mpdm_aget(a, 0));
    mpdm_t func = mpdm_aget(a, 1);

    window->area->timer->stop();

    mpdm_hset_s(MP, L"timer_func", func);

    if (msecs)
        window->area->timer->start(msecs);

    return NULL;
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
            mpdm_hset_s(drv, L"id",      MPDM_S(L"qt4"));
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
            mpdm_hset_s(drv, L"id",      MPDM_S(L"qt5"));
            mpdm_hset_s(drv, L"startup", MPDM_X(qt4_drv_startup));
        }
        else
            ret = 0;
    }

    return ret;
}

