#!/bin/bash

##############################################################################
# macOS build script
##############################################################################
#
# This script contains all steps necessary to:
#
#   * Build OBS with all default plugins and dependencies
#   * Create a macOS application bundle
#   * Codesign the macOS application bundle
#   * Package a macOS installation image
#   * Notarize macOS application bundle and/or installation image
#
# Parameters:
#   -h, --help                     : Print usage help
#   -q, --quiet                    : Suppress most build process output
#   -v, --verbose                  : Enable more verbose build process output
#   -d, --skip-dependency-checks   : Skip dependency checks (default: off)
#   -b, --bundle                   : Create relocatable application bundle
#                                    (default: off)
#   -p, --package                  : Create distributable disk image
#                                    (default: off)
#   -c, --codesign                 : Codesign OBS and all libraries
#                                    (default: ad-hoc only)
#   -n, --notarize                 : Notarize OBS (default: off)
#   --build-dir                    : Specify alternative build directory
#                                    (default: build)"
# Environment Variables (optional):
#
#   MACOS_DEPS_VERSION        : Precompiled macOS dependencies version
#   MACOS_CEF_BUILD_VERSION   : Chromium Embedded Framework version
#   VLC_VERSION               : VLC version
#   SPARKLE_VERSION           : Sparkle Framework version
#
##############################################################################

# Halt on errors
set -eE

## SET UP ENVIRONMENT ##
_RUN_OBS_BUILD_SCRIPT=TRUE
PRODUCT_NAME="OBS-Studio"

CHECKOUT_DIR="$(/usr/bin/git rev-parse --show-toplevel)"
DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
source "${CHECKOUT_DIR}/CI/include/build_support.sh"
source "${CHECKOUT_DIR}/CI/include/build_support_macos.sh"

## INSTALL DEPENDENCIES ##
source "${CHECKOUT_DIR}/CI/macos/01_install_dependencies.sh"

## BUILD OBS ##
source "${CHECKOUT_DIR}/CI/macos/02_build_obs.sh"

## PACKAGE OBS AND NOTARIZE ##
source "${CHECKOUT_DIR}/CI/macos/03_package_obs.sh"

## MAIN SCRIPT FUNCTIONS ##
print_usage() {
    echo "build-macos.sh - Build script for OBS-Studio\n"
    echo -e "Usage: ${0}\n" \
            "-h, --help                     : Print this help\n" \
            "-q, --quiet                    : Suppress most build process output\n" \
            "-v, --verbose                  : Enable more verbose build process output\n" \
            "-d, --skip-dependency-checks   : Skip dependency checks (default: off)\n" \
            "-b, --bundle                   : Create relocatable application bundle (default: off)\n" \
            "-p, --package                  : Create distributable disk image (default: off)\n" \
            "-c, --codesign                 : Codesign OBS and all libraries (default: ad-hoc only)\n" \
            "-n, --notarize                 : Notarize OBS (default: off)\n" \
            "--build-dir                    : Specify alternative build directory (default: build)\n"
}

print_deprecation() {
    echo -e "DEPRECATION ERROR:\n" \
            "The '${1}' switch has been deprecated!\n"

    if [ "${1}" = "-s" ]; then
        echo -e "The macOS build script system has changed:\n" \
                " - To configure and build OBS, run the script 'CI/macos/02_build_obs.sh'\n" \
                " - To bundle OBS into a relocatable application bundle, run the script 'CI/macos/02_build_obs.sh --bundle\n" \
                " - To package OBS, run the script 'CI/macos/03_package_obs.sh'\n" \
                " - To notarize OBS, run the script 'CI/macos/03_package_obs.sh --notarize'\n"
    fi

}

obs-build-main() {
    while true; do
        case "${1}" in
            -h | --help ) print_usage; exit 0 ;;
            -q | --quiet ) export QUIET=TRUE; shift ;;
            -v | --verbose ) export VERBOSE=TRUE; shift ;;
            -d | --skip-dependency-checks ) SKIP_DEP_CHECKS=TRUE; shift ;;
            -p | --package ) PACKAGE=TRUE; shift ;;
            -c | --codesign ) CODESIGN=TRUE; shift ;;
            -n | --notarize ) NOTARIZE=TRUE; PACKAGE=TRUE CODESIGN=TRUE; shift ;;
            -b | --bundle ) BUNDLE=TRUE; shift ;;
            --build-dir ) BUILD_DIR="${2}"; shift 2 ;;
            -s ) print_deprecation ${1}; exit 1 ;;
            -- ) shift; break ;;
            * ) break ;;
        esac
    done

    ensure_dir "${CHECKOUT_DIR}"
    check_archs
    check_macos_version
    step "Fetching OBS tags..."
    /usr/bin/git fetch origin --tags

    GIT_BRANCH=$(/usr/bin/git rev-parse --abbrev-ref HEAD)
    GIT_HASH=$(/usr/bin/git rev-parse --short HEAD)
    GIT_TAG=$(/usr/bin/git describe --tags --abbrev=0)

    if [ "${BUILD_FOR_DISTRIBUTION}" ]; then
        VERSION_STRING="${GIT_TAG}"
    else
        VERSION_STRING="${GIT_TAG}-${GIT_HASH}"
    fi

    if [ "${ARCH}" = "arm64" ]; then
        FILE_NAME="obs-studio-${VERSION_STRING}-macOS-Apple.dmg"
    elif [ "${ARCH}" = "universal" ]; then
        FILE_NAME="obs-studio-${VERSION_STRING}-macOS.dmg"
    else
        FILE_NAME="obs-studio-${VERSION_STRING}-macOS-Intel.dmg"
    fi

    if [ -z "${SKIP_DEP_CHECKS}" ]; then
        install_dependencies
    fi

    build_obs

    if [ "${BUNDLE}" ]; then
        bundle_obs
    fi

    if [ "${PACKAGE}" ]; then
        package_obs
    fi

    if [ "${NOTARIZE}" ]; then
        notarize_obs
    fi

    cleanup
}

obs-build-main $*
