#!/bin/bash

##############################################################################
# macOS build function
##############################################################################
#
# This script file can be included in build scripts for macOS or run directly
#
##############################################################################

# Halt on errors
set -eE

build_obs() {
    status "Build OBS"
    if [ -z "${CI}" ]; then
        _backup_artifacts
    fi
    step "Configure OBS..."
    _configure_obs

    ensure_dir "${CHECKOUT_DIR}/"
    step "Build OBS targets..."
    cmake --build ${BUILD_DIR}
}

bundle_obs() {
    status "Create relocatable macOS application bundle"
    trap "caught_error 'package app'" ERR

    ensure_dir "${CHECKOUT_DIR}"

    step "Install OBS application bundle..."
    cmake --install ${BUILD_DIR}
}

# Function to configure OBS build
_configure_obs() {
    if [ "${CODESIGN}" ]; then
        read_codesign_ident
    fi

    ensure_dir "${CHECKOUT_DIR}"
    status "Configure OBS build system..."
    check_ccache

    if [ "${TWITCH_CLIENTID}" -a "${TWICH_HASH}" ]; then
        TWITCH_OPTIONS="-DTWITCH_CLIENTID=\"${TWITCH_CLIENTID}\" -DTWITCH_HASH=\"${TWITCH_HASH}\""
    fi

    if [ "${RESTREAM_CLIENTID}" -a "${RESTREAM_HASH}" ]; then
        RESTREAM_OPTIONS="-DRESTREAM_CLIENTID=\"${RESTREAM_CLIENTID}\" -DRESTREAM_HASH=\"${RESTREAM_HASH}\""
    fi

    cmake -S . -B ${BUILD_DIR} -G Ninja \
        -DCEF_ROOT_DIR="${DEPS_BUILD_DIR}/cef_binary_${MACOS_CEF_BUILD_VERSION:-${CI_MACOS_CEF_VERSION}}_macos_${ARCH:-x86_64}" \
        -DVLC_PATH="${DEPS_BUILD_DIR}/vlc-${VLC_VERSION:-${CI_VLC_VERSION}}" \
        -DCMAKE_PREFIX_PATH="/tmp/obsdeps;${DEPS_BUILD_DIR}/sparkle" \
        -DBROWSER_LEGACY=$(test "${MACOS_CEF_BUILD_VERSION:-${CI_MACOS_CEF_VERSION}}" -le 3770 && echo "ON" || echo "OFF") \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-${CI_MACOSX_DEPLOYMENT_TARGET}} \
        -DCMAKE_OSX_ARCHITECTURES=${CMAKE_ARCHS} \
        -DOBS_CODESIGN_LINKER=${CODESIGN_LINKER:-OFF} \
        -DCMAKE_INSTALL_PREFIX=${BUILD_DIR}/install \
        -DCMAKE_BUILD_TYPE=${BUILD_CONFIG} \
        -DOBS_BUNDLE_CODESIGN_IDENTITY="${CODESIGN_IDENT:--}" \
        ${CMAKE_CCACHE_OPTIONS} \
        ${TWITCH_OPTIONS} \
        ${RESTREAM_OPTIONS} \
        ${CI:+-DENABLE_UNIT_TESTS=ON -DBUILD_FOR_DISTRIBUTION=${BUILD_FOR_DISTRIBUTION} -DOBS_BUILD_NUMBER=${GITHUB_RUN_ID}} \
        ${QUIET:+-Wno-deprecated -Wno-dev --log-level=ERROR}
}

# Function to backup previous build artifacts
_backup_artifacts() {
    ensure_dir "${CHECKOUT_DIR}"
    if [ -d "${BUILD_DIR}" ]; then
        status "Backup old OBS build artifacts"

        CUR_DATE=$(/bin/date +"%Y-%m-%d@%H%M%S")
        NIGHTLY_DIR="${CHECKOUT_DIR}/nightly-${CUR_DATE}"
        PACKAGE_NAME=$(/usr/bin/find "${BUILD_DIR}" -name "*.dmg" -depth 1 | sort -rn | head -1)

        if [ -d "${BUILD_DIR}/install/OBS.app" ]; then
            step "Back up OBS.app..."
            ensure_dir "${NIGHTLY_DIR}"
            /bin/mv "${CHECKOUT_DIR}/${BUILD_DIR}/install/OBS.app" "${NIGHTLY_DIR}/"
            info "You can find OBS.app in ${NIGHTLY_DIR}"
        fi

        if [ "${PACKAGE_NAME}" ]; then
            step "Back up $(basename "${PACKAGE_NAME}")..."
            ensure_dir "${NIGHTLY_DIR}"
            /bin/mv "../${BUILD_DIR}/$(basename "${PACKAGE_NAME}")" "${NIGHTLY_DIR}/"
            info "You can find ${PACKAGE_NAME} in ${NIGHTLY_DIR}"
        fi
    fi
}

build-obs-standalone() {
    CHECKOUT_DIR="$(/usr/bin/git rev-parse --show-toplevel)"
    PRODUCT_NAME="OBS-Studio"
    DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
    source "${CHECKOUT_DIR}/CI/include/build_support.sh"
    source "${CHECKOUT_DIR}/CI/include/build_support_macos.sh"

    check_macos_version
    check_archs
    build_obs

    if [ "${BUNDLE}" ]; then
        bundle_obs
    fi
}

print_usage() {
    echo -e "Usage: ${0}\n" \
            "-h, --help                     : Print this help\n" \
            "-q, --quiet                    : Suppress most build process output\n" \
            "-v, --verbose                  : Enable more verbose build process output\n" \
            "-a, --architecture             : Specify build architecture (default: x86_64, alternative: arm64)\n" \
            "-c, --codesign                 : Codesign OBS and all libraries (default: ad-hoc only)\n" \
            "-b, --bundle                   : Create relocatable OBS application bundle in build directory (default: build/install/OBS.app)\n" \
            "--build-dir                    : Specify alternative build directory (default: build)\n"
}

build-obs-main() {
    if [ -z "${_RUN_OBS_BUILD_SCRIPT}" ]; then
        while true; do
            case "${1}" in
                -h | --help ) print_usage; exit 0 ;;
                -q | --quiet ) export QUIET=TRUE; shift ;;
                -v | --verbose ) export VERBOSE=TRUE; shift ;;
                -a | --architecture ) ARCH="${2}"; shift 2 ;;
                -c | --codesign ) CODESIGN=TRUE; shift ;;
                -b | --bundle ) BUNDLE=TRUE; shift ;;
                --build-dir ) BUILD_DIR="${2}"; shift 2 ;;
                -- ) shift; break ;;
                * ) break ;;
            esac
        done

        build-obs-standalone
    fi
}

build-obs-main $*
