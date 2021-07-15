#!/usr/bin/env bash

##############################################################################
# FreeBSD dependency management function
##############################################################################
#
# This script file can be included in build scripts or run directly
#
##############################################################################

# Halt on errors
set -eE

install_build-deps() {
    shift
    status "Install OBS build dependencies"
    trap "caught_error 'install_build-deps'" ERR

    sudo pkg install -U -y $@
}

install_obs-deps() {
    shift
    status "Install OBS dependencies"
    trap "caught_error 'install_obs-deps'" ERR

    sudo pkg install -U -y $@
}

install_qt-deps() {
    shift
    status "Install Qt dependencies"
    trap "caught_error 'install_qt-deps'" ERR

    sudo pkg install -U -y $@
}

install_plugin-deps() {
    shift
    status "Install plugin dependencies"
    trap "caught_error 'install_plugin-deps'" ERR

    sudo pkg install -U -y $@
}

install_dependencies() {
    status "Set up apt"
    trap "caught_error 'install_dependencies'" ERR

    BUILD_DEPS=(
        "build-deps cmake ninja pkgconf curl ccache"
        "obs-deps ffmpeg libx264 mbedtls mesa-libs jansson lua52 luajit python37 libX11 xorgproto libxcb \
         libXcomposite libXext libXfixes libXinerama libXrandr swig dbus jansson libICE libSM libsysinfo"
        "qt-deps qt5-buildtools qt5-qmake qt5-imageformats qt5-core qt5-gui qt5-svg qt5-widgets qt5-xml"
        "plugin-deps v4l_compat fdk-aac fontconfig freetype2 speexdsp libudev-devd libv4l vlc audio/jack pulseaudio sndio"
    )

    for DEPENDENCY in "${BUILD_DEPS[@]}"; do
        set -- ${DEPENDENCY}
        trap "caught_error ${DEPENDENCY}" ERR
        FUNC_NAME="install_${1}"
        ${FUNC_NAME} ${@}
    done
}

install-dependencies-standalone() {
    CHECKOUT_DIR="$(git rev-parse --show-toplevel)"
    PRODUCT_NAME="OBS-Studio"
    DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
    source "${CHECKOUT_DIR}/CI/include/build_support.sh"
    source "${CHECKOUT_DIR}/CI/include/build_support_freebsd.sh"

    status "Setup of OBS build dependencies"
    install_dependencies
}

print_usage() {
    echo -e "Usage: ${0}\n" \
            "-h, --help                     : Print this help\n" \
            "-q, --quiet                    : Suppress most build process output\n" \
            "-v, --verbose                  : Enable more verbose build process output\n"
}

install-dependencies-main() {
    if [ -z "${_RUN_OBS_BUILD_SCRIPT}" ]; then
        while true; do
            case "${1}" in
                -h | --help ) print_usage; exit 0 ;;
                -q | --quiet ) export QUIET=TRUE; shift ;;
                -v | --verbose ) export VERBOSE=TRUE; shift ;;
                -- ) shift; break ;;
                * ) break ;;
            esac
        done

        install-dependencies-standalone
    fi
}

install-dependencies-main $*
