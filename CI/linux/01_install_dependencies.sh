#!/bin/bash

##############################################################################
# Linux dependency management function
##############################################################################
#
# This script file can be included in build scripts for Linux or run directly
# with the -s/--standalone switch
#
##############################################################################

# Halt on errors
set -eE

install_build-deps() {
    shift
    status "Install OBS build dependencies"
    trap "caught_error 'install_build-deps'" ERR

    sudo apt-get install -y $@
}

install_obs-deps() {
    shift
    status "Install OBS dependencies"
    trap "caught_error 'install_obs-deps'" ERR

    if [ -z "${DISABLE_PIPEWIRE}" ]; then
        sudo apt-get install -y $@ libpipewire-0.3-dev
    else
        sudo apt-get install -y $@
    fi
}

install_qt-deps() {
    shift
    status "Install Qt dependencies"
    trap "caught_error 'install_qt-deps'" ERR

    sudo apt-get install -y $@
}

install_cef() {
    shift
    status "Setup for dependency CEF v${1}"
    ensure_dir "${DEPS_BUILD_DIR}"

    if [ "${CI}" -a "${RESTORED_CEF}" ]; then
        _SKIP=TRUE
    elif [ -d "${DEPS_BUILD_DIR}/cef_binary_${1}_linux64" -a -f "${DEPS_BUILD_DIR}/cef_binary_${1}_linux64/build/libcef_dll_wrapper/libcef_dll_wrapper.a" ]; then
        _SKIP=TRUE
    fi

    if [ -z "${_SKIP}" ]; then
        step "Download..."
        ${CURLCMD:-curl -O} https://cdn-fastly.obsproject.com/downloads/cef_binary_${1}_linux64.tar.bz2
        step "Unpack..."
        tar -xf cef_binary_${1}_linux64.tar.bz2
    else
        step "Found existing Chromium Embedded Framework and loader library..."
    fi
}

install_plugin-deps() {
    shift
    status "Install plugin dependencies"
    trap "caught_error 'install_plugin-deps'" ERR

    sudo apt-get install -y $@
}

install_dependencies() {
    status "Set up apt"
    trap "caught_error 'install_dependencies'" ERR

    BUILD_DEPS=(
        "build-deps cmake ninja-build pkg-config clang clang-format build-essential curl ccache"
        "obs-deps libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev libavutil-dev libswresample-dev \
         libswscale-dev libx264-dev libcurl4-openssl-dev libmbedtls-dev libgl1-mesa-dev libjansson-dev \
         libluajit-5.1-dev python3-dev libx11-dev libxcb-randr0-dev libxcb-shm0-dev libxcb-xinerama0-dev \
         libxcomposite-dev libxinerama-dev libxcb1-dev libx11-xcb-dev libxcb-xfixes0-dev swig libcmocka-dev \
         libxss-dev libglvnd-dev libgles2-mesa libgles2-mesa-dev libwayland-dev"
        "qt-deps qtbase5-dev qtbase5-private-dev libqt5svg5-dev qtwayland5"
        "cef ${LINUX_CEF_BUILD_VERSION:-${CI_LINUX_CEF_VERSION}}"
        "plugin-deps libasound2-dev libfdk-aac-dev libfontconfig-dev libfreetype6-dev libjack-jackd2-dev \
         libpulse-dev libsndio-dev libspeexdsp-dev libudev-dev libv4l-dev libva-dev libvlc-dev"
    )

    sudo dpkg --add-architecture amd64
    sudo apt-get -qq update

    for DEPENDENCY in "${BUILD_DEPS[@]}"; do
        set -- ${DEPENDENCY}
        trap "caught_error ${DEPENDENCY}" ERR
        FUNC_NAME="install_${1}"
        ${FUNC_NAME} ${@}
    done
}

install-dependencies-standalone() {
    CHECKOUT_DIR="$(/usr/bin/git rev-parse --show-toplevel)"
    PRODUCT_NAME="OBS-Studio"
    DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
    source "${CHECKOUT_DIR}/CI/include/build_support.sh"
    source "${CHECKOUT_DIR}/CI/include/build_support_linux.sh"

    status "Setup of OBS build dependencies"
    install_dependencies
}

print_usage() {
    echo -e "Usage: ${0}\n" \
            "-h, --help                     : Print this help\n" \
            "-q, --quiet                    : Suppress most build process output\n" \
            "-v, --verbose                  : Enable more verbose build process output\n" \
            "--disable-pipewire             : Disable building with Pipewire support (default: off)\n"
}

install-dependencies-main() {
    if [ -z "${_RUN_OBS_BUILD_SCRIPT}" ]; then
        while true; do
            case "${1}" in
                -h | --help ) print_usage; exit 0 ;;
                -q | --quiet ) export QUIET=TRUE; shift ;;
                -v | --verbose ) export VERBOSE=TRUE; shift ;;
                --disable-pipewire ) DISABLE_PIPEWIRE=TRUE; shift ;;
                -- ) shift; break ;;
                * ) break ;;
            esac
        done

        install-dependencies-standalone
    fi
}

install-dependencies-main $*
