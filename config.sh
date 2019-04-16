#!/bin/sh

# Minimum Profit autoconfiguration script

DRIVERS=""
DRV_OBJS=""
MORE_OBJS=""
MORE_TARGETS=""
APPNAME="mp-5"
TARGET=""

# gets program version
VERSION=`cut -f2 -d\" VERSION`

# default installation prefix
PREFIX=/usr/local

# store command line args for configuring the libraries
CONF_ARGS="$*"

# add a default value for WINDRES
[ -z "$WINDRES" ] && WINDRES="windres"

# No KDE4 by default
WITHOUT_KDE4=1

# No msgfmt by default
WITHOUT_MSGFMT=1

WITH_EXTERNAL_TAR=0

# store command line args for configuring the libraries
CONF_ARGS="$*"

# parse arguments
while [ $# -gt 0 ] ; do

    case $1 in
    --without-curses)    WITHOUT_CURSES=1 ;;
    --without-gtk)       WITHOUT_GTK=1 ;;
    --without-win32)     WITHOUT_WIN32=1 ;;
    --with-kde4)         WITHOUT_KDE4=0 ;;
    --without-qt)        WITHOUT_QT5=1 && WITHOUT_QT4=1 ;;
    --without-qt4)       WITHOUT_QT4=1 ;;
    --without-qt5)       WITHOUT_QT5=1 ;;

    --without-ansi)      WITHOUT_ANSI=1 ;;
    --with-external-tar) WITH_EXTERNAL_TAR=1 ;;
    --help)              CONFIG_HELP=1 ;;

    --mingw32)          CC=i586-mingw32msvc-cc
                        WINDRES=i586-mingw32msvc-windres
                        AR=i586-mingw32msvc-ar
                        CFLAGS="-O3 $CFLAGS"
                        ;;

    --debian)           BUILD_FOR_DEBIAN=1
                        PREFIX=/usr
                        APPNAME=mped
                        ;;

    --prefix)           PREFIX=$2 ; shift ;;
    --prefix=*)         PREFIX=`echo $1 | sed -e 's/--prefix=//'` ;;
    --with-moc=*)       MOC=`echo $1 | sed -e 's/--with-moc=//'` ;;
    esac

    shift
done

if [ "$CONFIG_HELP" = "1" ] ; then

    echo "Available options:"
    echo "--prefix=PREFIX       Installation prefix ($PREFIX)."
    echo "--without-curses      Disable curses (text) interface detection."
    echo "--without-gtk         Disable GTK interface detection."
    echo "--without-win32       Disable win32 interface detection."
    echo "--with-kde4           Enable KDE4 interface detection."
    echo "--without-qt          Disable Qt interface detection."
    echo "--without-qt4         Disable Qt4 interface detection."
    echo "--without-qt5         Disable Qt5 interface detection."
    echo "--with-moc            Path to your Qt moc. Ie: --with-moc=/usr/lib64/qt4/bin/moc"
    echo "--without-ansi        Disable ANSI terminal interface detection."
    echo "--without-unix-glob   Disable glob.h usage (use workaround)."
    echo "--with-included-regex Use included regex code (gnu_regex.c)."
    echo "--with-pcre           Enable PCRE library detection."
    echo "--without-gettext     Disable gettext usage."
    echo "--without-iconv       Disable iconv usage."
    echo "--without-wcwidth     Disable system wcwidth() (use workaround)."
    echo "--with-null-hash      Tell MPDM to use a NULL hashing function."
    echo "--mingw32             Build using the mingw32 compiler."
    echo "--debian              Build for Debian ('make deb')."
    echo "--with-external-tar   Store code in external tar (vs. embedded)."

    echo
    echo "Environment variables:"
    echo "CC                    C Compiler."
    echo "CPP                   C++ Compiler."
    echo "LD                    Linker."
    echo "AR                    Library Archiver."
    echo "CFLAGS                Compile flags (i.e., -O3)."
    echo "WINDRES               MS Windows resource compiler."

    exit 1
fi

echo "Configuring..."

echo "/* automatically created by config.sh - do not modify */" > config.h
echo "# automatically created by config.sh - do not modify" > makefile.opts
> config.ldflags
> config.cflags
> .config.log

# set compiler
if [ "$CC" = "" ] ; then
    CC=cc
    # if CC is unset, try if gcc is available
    which gcc > /dev/null 2>&1 && CC=gcc
fi

if [ "$CPP" = "" ] ; then
    CPP=c++
    # if CC is unset, try if gcc is available
    which g++ > /dev/null 2>&1 && CPP=g++
fi

if [ "$LD" = "" ] ; then
    LD=ld
fi

if [ "$TAR" = "" ] ; then
    TAR=tar
fi

echo "CC=$CC" >> makefile.opts
echo "CPP=$CPP" >> makefile.opts
echo "LD=$LD" >> makefile.opts
echo "TAR=$TAR" >> makefile.opts

# add version
cat VERSION >> config.h

# add installation prefix and application name
echo "#define CONFOPT_PREFIX \"$PREFIX\"" >> config.h
echo "#define CONFOPT_APPNAME \"$APPNAME\"" >> config.h

################################################################

# CFLAGS
if [ -z "$CFLAGS" ] ; then
    CFLAGS="-g -Wall"
fi

echo -n "Testing if C compiler supports ${CFLAGS}... "
echo "int main(int argc, char *argv[]) { return 0; }" > .tmp.c

$CC .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "OK"
else
    echo "No; resetting to defaults"
    CFLAGS=""
fi

echo "CFLAGS=$CFLAGS" >> makefile.opts

# Add CFLAGS to CC
CC="$CC $CFLAGS"

# MPDM
echo -n "Looking for MPDM... "

for MPDM in ./mpdm ../mpdm NOTFOUND ; do
    if [ -d $MPDM ] && [ -f $MPDM/mpdm.h ] ; then
        break
    fi
done

if [ "$MPDM" != "NOTFOUND" ] ; then
    echo "-I$MPDM" >> config.cflags
    echo "-L$MPDM -lmpdm" >> config.ldflags
    echo "OK ($MPDM)"
else
    echo "No"
    exit 1
fi

echo "MPDM=$MPDM" >> makefile.opts

# MPSL
echo -n "Looking for MPSL... "

for MPSL in ./mpsl ../mpsl NOTFOUND ; do
    if [ -d $MPSL ] && [ -f $MPSL/mpsl.h ] ; then
        break
    fi
done

if [ "$MPSL" != "NOTFOUND" ] ; then
    echo "-I$MPSL" >> config.cflags
    echo "-L$MPSL -lmpsl" >> config.ldflags
    echo "OK ($MPSL)"
else
    echo "No"
    exit 1
fi

echo
(cd $MPSL && ./config.sh --prefix=$PREFIX --docdir=$PREFIX/share/doc/$APPNAME $CONF_ARGS)
echo

echo "MPSL=$MPSL" >> makefile.opts

cat $MPDM/config.ldflags >> config.ldflags
cat $MPSL/config.ldflags >> config.ldflags

# Win32

echo -n "Testing for win32... "
if [ "$WITHOUT_WIN32" = "1" ] ; then
    echo "Disabled"
else
    grep CONFOPT_WIN32 ${MPDM}/config.h >/dev/null

    if [ $? = 0 ] ; then
        echo "-mwindows -lcomctl32" >> config.ldflags
        echo "#define CONFOPT_WIN32 1" >> config.h
        echo "OK"
        DRIVERS="win32 $DRIVERS"
        DRV_OBJS="mpv_win32.o mpv_win32c.o $DRV_OBJS"
        WITHOUT_UNIX_GLOB=1
        WITHOUT_KDE4=1
        WITHOUT_GTK=1
        WITHOUT_CURSES=1
        WITHOUT_QT4=1
        WITHOUT_ANSI=1
        TARGET="mp-5.exe mp-5c.exe"
    else
        echo "No"
    fi
fi

# test for curses / ncurses library
echo -n "Testing for ncursesw... "

if [ "$WITHOUT_CURSES" = "1" ] ; then
    echo "Disabled"
else
    echo "#include <ncursesw/ncurses.h>" > .tmp.c
    echo "int main(void) { initscr(); endwin(); return 0; }" >> .tmp.c

    TMP_CFLAGS="-I/usr/local/include -I/usr/include/ncurses -I/usr/include/ncursesw"
    TMP_LDFLAGS="-L/usr/local/lib -lncursesw"

    $CC $TMP_CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log
    if [ $? = 0 ] ; then
        echo "#define CONFOPT_CURSES 1" >> config.h
        echo $TMP_CFLAGS >> config.cflags
        echo $TMP_LDFLAGS >> config.ldflags
        echo "OK (ncursesw)"
        DRIVERS="ncursesw $DRIVERS"
        DRV_OBJS="mpv_curses.o $DRV_OBJS"
        WITHOUT_ANSI=1
    else
        echo "No"
        WITHOUT_CURSES=1
    fi
fi

if [ "$WITHOUT_CURSES" != "1" ] ; then
    # test for transparent colors in curses
    echo -n "Testing for transparency support in curses... "

    echo "#include <ncurses.h>" > .tmp.c
    echo "int main(void) { initscr(); use_default_colors(); endwin(); return 0; }" >> .tmp.c

    $CC $TMP_CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log
    if [ $? = 0 ] ; then
        echo "#define CONFOPT_TRANSPARENCY 1" >> config.h
        echo "OK"
    else
        echo "No"
    fi

    # test now for wget_wch() existence
    echo -n "Testing for wget_wch()... "

    echo "#include <wchar.h>" > .tmp.c
    echo "#include <ncurses.h>" >> .tmp.c
    echo "int main(void) { wchar_t c[2]; initscr(); wget_wch(stdscr, c); endwin(); return 0; }" >> .tmp.c

    $CC $TMP_CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log
    if [ $? = 0 ] ; then
        echo "#define CONFOPT_WGET_WCH 1" >> config.h
        echo "OK"
    else
        echo "No"
    fi
fi

# ANSI
echo -n "Testing for ANSI terminal support... "

if [ "$WITHOUT_ANSI" = "1" ] ; then
    echo "Disabled"
else
    rm -f .tmp.c
    echo "#include <stdio.h>" >> .tmp.c
    echo "#include <termios.h>" >> .tmp.c
    echo "#include <unistd.h>" >> .tmp.c
    echo "#include <sys/select.h>" >> .tmp.c
    echo "#include <signal.h>" >> .tmp.c
    echo "int main(void) { struct termios o; tcgetattr(0, &o); return 0; }" >> .tmp.c

    TMP_CFLAGS=""
    TMP_LDFLAGS=""

    $CC $TMP_CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log
    if [ $? = 0 ] ; then
        echo "#define CONFOPT_ANSI 1" >> config.h
        echo $TMP_CFLAGS >> config.cflags
        echo $TMP_LDFLAGS >> config.ldflags
        echo "OK"
        DRIVERS="ansi $DRIVERS"
        DRV_OBJS="mpv_ansi.o $DRV_OBJS"
    else
        echo "No"
        WITHOUT_ANSI=1
    fi
fi

# KDE4

echo -n "Testing for KDE4... "
if [ "$WITHOUT_KDE4" = "1" ] ; then
    echo "Disabled"
else
    if which kde4-config > /dev/null 2>&1
    then
        TMP_CFLAGS=$(pkg-config --cflags QtGui)
        TMP_CFLAGS="$TMP_CFLAGS -I`kde4-config --install include` -I`kde4-config --install include`KDE"

        TMP_LDFLAGS="$(pkg-config --libs QtGui) -lX11"
        TMP_LDFLAGS="$TMP_LDFLAGS -L`kde4-config --install lib` -L`kde4-config --install lib`/kde4/devel -lkio -lkfile -lkdeui -lkdecore"

        echo "#include <KApplication>" > .tmp.cpp
        echo "int main(void) { new KApplication() ; return 0; } " >> .tmp.cpp

        echo "$CPP $TMP_CFLAGS .tmp.cpp $TMP_LDFLAGS -o .tmp.o" >> .config.log
        $CPP $TMP_CFLAGS .tmp.cpp $TMP_LDFLAGS -o .tmp.o 2>> .config.log

        if [ $? = 0 ] ; then
            echo $TMP_CFLAGS >> config.cflags
            echo $TMP_LDFLAGS >> config.ldflags

            echo "#define CONFOPT_KDE4 1" >> config.h
            echo "OK"

            DRIVERS="kde4 $DRIVERS"
            DRV_OBJS="mpv_kde4.o $DRV_OBJS"
            if [ "$CCLINK" = "" ] ; then
                CCLINK="g++"
            fi

            WITHOUT_GTK=1
            WITHOUT_QT4=1
        else
            echo "No"
        fi
    else
        echo "No"
    fi
fi

# Qt5

echo -n "Testing for Qt5... "
if [ "$WITHOUT_QT5" = "1" ] ; then
    echo "Disabled"
else
    if which pkg-config > /dev/null 2>&1
    then
        TMP_CFLAGS="`sh -c 'pkg-config --cflags Qt5Widgets' 2>/dev/null` -fPIC"
        TMP_LDFLAGS="`sh -c 'pkg-config --libs Qt5Widgets' 2>/dev/null`"

        echo "#include <QtWidgets>" > .tmp.cpp
        echo "int main(int argc, char *argv[]) { new QApplication(argc, argv) ; return 0; } " >> .tmp.cpp
 
        echo "$CPP $TMP_CFLAGS .tmp.cpp $TMP_LDFLAGS -o .tmp.o" >> .config.log
        $CPP $TMP_CFLAGS .tmp.cpp $TMP_LDFLAGS -o .tmp.o 2>> .config.log

        if [ $? = 0 ] ; then
            echo $TMP_CFLAGS >> config.cflags
            echo $TMP_LDFLAGS >> config.ldflags

            echo "#define CONFOPT_QT5 1" >> config.h
            echo "OK"

            DRIVERS="qt5 $DRIVERS"
            DRV_OBJS="mpv_qt4.o $DRV_OBJS"
            if [ "$CCLINK" = "" ] ; then
                CCLINK="g++"
            fi

            WITHOUT_QT4=1
            WITHOUT_GTK=1

            [ -z "$MOC" ] && which moc-qt5 > /dev/null 2>&1 && MOC=moc-qt5
            [ -z "$MOC" ] && MOC="moc -qt5"
            echo "MOC=$MOC" >> makefile.opts
        else
            echo "No"
        fi
    else
        echo "No"
    fi
fi


# Qt4

echo -n "Testing for Qt4... "
if [ "$WITHOUT_QT4" = "1" ] ; then
    echo "Disabled"
else
    if which pkg-config > /dev/null 2>&1
    then
        TMP_CFLAGS=`sh -c 'pkg-config --cflags QtGui' 2>/dev/null`
        TMP_LDFLAGS="`sh -c 'pkg-config --libs QtGui' 2>/dev/null` -lX11"

        echo "#include <QtGui>" > .tmp.cpp
        echo "int main(int argc, char *argv[]) { new QApplication(argc, argv) ; return 0; } " >> .tmp.cpp

        echo "$CPP $TMP_CFLAGS .tmp.cpp $TMP_LDFLAGS -o .tmp.o" >> .config.log
        $CPP $TMP_CFLAGS .tmp.cpp $TMP_LDFLAGS -o .tmp.o 2>> .config.log

        if [ $? = 0 ] ; then
            echo $TMP_CFLAGS >> config.cflags
            echo $TMP_LDFLAGS >> config.ldflags

            echo "#define CONFOPT_QT4 1" >> config.h
            echo "OK"

            DRIVERS="qt4 $DRIVERS"
            DRV_OBJS="mpv_qt4.o $DRV_OBJS"
            if [ "$CCLINK" = "" ] ; then
                CCLINK="g++"
            fi

            WITHOUT_GTK=1

            [ -z "$MOC" ] && which moc-qt4 > /dev/null 2>&1 && MOC=moc-qt4
            [ -z "$MOC" ] && MOC="moc -qt4"
            echo "MOC=$MOC" >> makefile.opts
        else
            echo "No"
        fi
    else
        echo "No"
    fi
fi

# GTK
echo -n "Testing for GTK... "

if [ "$WITHOUT_GTK" = "1" ] ; then
    echo "Disabled"
else
    echo "#include <gtk/gtk.h>" > .tmp.c
    echo "#include <gdk/gdkkeysyms.h>" >> .tmp.c
    echo "int main(void) { gtk_main(); return 0; } " >> .tmp.c

    # Try first GTK 3.0
    TMP_CFLAGS=`sh -c 'pkg-config --cflags gtk+-3.0' 2>/dev/null`
    TMP_LDFLAGS=`sh -c 'pkg-config --libs gtk+-3.0' 2>/dev/null`

    $CC $TMP_CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log
    if [ $? = 0 ] ; then
        echo "#define CONFOPT_GTK 3" >> config.h
        echo "$TMP_CFLAGS " >> config.cflags
        echo "$TMP_LDFLAGS " >> config.ldflags
        echo "OK (3.0)"
        DRIVERS="gtk $DRIVERS"
        DRV_OBJS="mpv_gtk.o $DRV_OBJS"
    else
        # Try now GTK 2.0
        TMP_CFLAGS=`sh -c 'pkg-config --cflags gtk+-2.0' 2>/dev/null`
        TMP_LDFLAGS=`sh -c 'pkg-config --libs gtk+-2.0' 2>/dev/null`

        $CC $TMP_CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log
        if [ $? = 0 ] ; then
            echo "#define CONFOPT_GTK 2" >> config.h
            echo "$TMP_CFLAGS " >> config.cflags
            echo "$TMP_LDFLAGS " >> config.ldflags
            echo "OK (2.0)"
            DRIVERS="gtk $DRIVERS"
            DRV_OBJS="mpv_gtk.o $DRV_OBJS"
        else
            echo "No"
        fi
    fi
fi

# msgfmt
echo -n "Testing for msgfmt... "

if [ "$WITHOUT_MSGFMT" = "1" ] ; then
    echo "Disabled"
    echo "BUILDMO=" >> makefile.opts
    echo "INSTALLMO=" >> makefile.opts
    echo "UNINSTALLMO=" >> makefile.opts
else
    if which msgfmt > /dev/null 2>&1 ; then
        echo "OK"
        echo "BUILDMO=build-mo" >> makefile.opts
        echo "INSTALLMO=install-mo" >> makefile.opts
        echo "UNINSTALLMO=uninstall-mo" >> makefile.opts
    else
        echo "No"
        echo "BUILDMO=" >> makefile.opts
        echo "INSTALLMO=" >> makefile.opts
        echo "UNINSTALLMO=" >> makefile.opts
    fi
fi

# test for embeddable binaries
echo -n "Testing for embeddable binaries... "
if [ "$WITH_EXTERNAL_TAR" = "1" ] ; then
    echo "Disabled"
else
    echo "test" > tmp.bin
    $LD -r -b binary tmp.bin -o tmp.bin.o

    echo "extern const char _binary_tmp_bin_start;" > .tmp.c
    echo "extern const char _binary_tmp_bin_end;" >> .tmp.c
    echo "extern const char binary_tmp_bin_start;" >> .tmp.c
    echo "extern const char binary_tmp_bin_end;" >> .tmp.c
    echo "int main(void) { " >> .tmp.c
    echo "#ifdef CONFOPT_EMBED_NOUNDER" >> .tmp.c
    echo "  int c = &binary_tmp_bin_end - &binary_tmp_bin_start;" >> .tmp.c
    echo "#else" >> .tmp.c
    echo "  int c = &_binary_tmp_bin_end - &_binary_tmp_bin_start;" >> .tmp.c
    echo "#endif" >> .tmp.c
    echo "  return c; } " >> .tmp.c

    $CC .tmp.c tmp.bin.o -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "Yes (with underscores)"
        MORE_OBJS="mp.tar.o ${MORE_OBJS}"
    else
        $CC -DCONFOPT_EMBED_NOUNDER .tmp.c tmp.bin.o -o .tmp.o 2>> .config.log

        if [ $? = 0 ] ; then
            echo "Yes (without underscores)"
            MORE_OBJS="mp.tar.o ${MORE_OBJS}"
            echo "#define CONFOPT_EMBED_NOUNDER 1" >> config.h
        else
            echo "No"
            WITH_EXTERNAL_TAR=1
        fi
    fi

    rm -f tmp.bin tmp.bin.o
fi

if [ "$CCLINK" = "" ] ; then
    CCLINK=$CC
fi

if [ "$WITH_EXTERNAL_TAR" = "1" ] ; then
    echo "#define CONFOPT_EXTERNAL_TAR 1" >> config.h
    MORE_TARGETS="mp-dist.tar $MORE_TARGETS"
fi

echo >> config.h

if [ "$TARGET" = "" ] ; then
    TARGET="$APPNAME"
fi

grep DOCS $MPDM/makefile.opts >> makefile.opts
echo "VERSION=$VERSION" >> makefile.opts
echo "WINDRES=$WINDRES" >> makefile.opts
echo "PREFIX=\$(DESTDIR)$PREFIX" >> makefile.opts
echo "APPNAME=$APPNAME" >> makefile.opts
echo "TARGET=$TARGET" >> makefile.opts
echo "DRV_OBJS=$DRV_OBJS" >> makefile.opts
echo "MORE_OBJS=$MORE_OBJS" >> makefile.opts
echo "MORE_TARGETS=$MORE_TARGETS" >> makefile.opts
echo "CONF_ARGS=$CONF_ARGS" >> makefile.opts
echo "CCLINK=$CCLINK" >> makefile.opts
echo >> makefile.opts

cat makefile.opts makefile.in makefile.depend > Makefile

##############################################

if [ "$DRIVERS" = "" ] ; then

    echo
    echo "*ERROR* No usable drivers (interfaces) found"
    echo "See the README file for the available options."

    exit 1
fi

echo
echo "Configured drivers:" $DRIVERS
echo
echo "Type 'make' to build Minimum Profit."

# insert driver detection code into config.h

TRY_DRIVERS="#define TRY_DRIVERS() ("
echo >> config.h
for drv in $DRIVERS ; do
    echo "int ${drv}_drv_detect(int * argc, char *** argv);" >> config.h
    TRY_DRIVERS="$TRY_DRIVERS ${drv}_drv_detect(&argc, &argv) || "
done

echo >> config.h
echo $TRY_DRIVERS '0)' >> config.h

# cleanup
rm -f .tmp.c .tmp.cpp .tmp.o

exit 0
