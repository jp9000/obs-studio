#!/bin/sh
set -ex

# Compile and install to an AppDir
cd ./build
make -j$(nproc)
make DESTDIR=appdir -j$(nproc) install ; find appdir/
find appdir -executable -type f -exec ldd {} \; | grep " => /usr" | cut -d " " -f 2-3 | sort | uniq

# Finalize the AppDir and convert it to AppImage
wget -c -nv "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
chmod a+x linuxdeployqt-continuous-x86_64.AppImage
unset QTDIR; unset QT_PLUGIN_PATH ; unset LD_LIBRARY_PATH
export VERSION=$(git rev-parse --short HEAD) # linuxdeployqt uses this for naming the file
./linuxdeployqt-continuous-x86_64.AppImage appdir/usr/share/applications/*.desktop -bundle-non-qt-libs -qmake=/usr/lib/x86_64-linux-gnu/qt5/bin/qmake
./linuxdeployqt-continuous-x86_64.AppImage --appimage-extract ; export PATH=./squashfs-root/usr/bin:$PATH # Get patchelf
patchelf --set-rpath '$ORIGIN' appdir/usr/lib/libobs-opengl.so.0 # This is loaded by obs with dlopen(), so linuxdeployqt can't know about it
# FIXME: This version of Qt seems to have a bug since it tries to load platforms from the current working directory (cwd)
# rather than from the prefix defined in qt.conf
# me@host:~$ strace ./squashfs-root/AppRun 2>&1 | grep platforms
# stat("/home/me/platforms/.", 0x7ffe8aca3130) = -1 ENOENT (No such file or directory)
./linuxdeployqt-continuous-x86_64.AppImage appdir/usr/share/applications/*.desktop -appimage -qmake=/usr/lib/x86_64-linux-gnu/qt5/bin/qmake

# TODO: The next line should be replaced by a native upload mechanism defined in .travis.yml,
# e.g., using https://github.com/probonopd/uploadtool
curl --upload-file OBS*.AppImage https://transfer.sh/OBS-git.$(git rev-parse --short HEAD)-x86_64.AppImage
