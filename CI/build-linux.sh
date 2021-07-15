#!/bin/bash

##############################################################################
# Linux full build script
##############################################################################
#
# This script contains all steps necessary to:
#
#   * Build OBS with all default plugins and dependencies
#   * Package a Linux deb package
#
# Parameters:
#   -h, --help                     : Print usage help
#   -q, --quiet                    : Suppress most build process output
#   -v, --verbose                  : Enable more verbose build process output
#   -d, --skip-dependency-checks   : Skip dependency checks (default: off)
#   -p, --portable                 : Create portable build (default: off)
#   -pkg, --package                : Create distributable disk image
#                                    (default: off)
#   --build-dir                    : Specify alternative build directory
#                                    (default: build)"
#
##############################################################################

# Halt on errors
set -eE

## SET UP ENVIRONMENT ##
_RUN_OBS_BUILD_SCRIPT=TRUE
PRODUCT_NAME="OBS-Studio"

CHECKOUT_DIR="$(git rev-parse --show-toplevel)"
DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
source "${CHECKOUT_DIR}/CI/include/build_support.sh"
source "${CHECKOUT_DIR}/CI/include/build_support_linux.sh"

## DEPENDENCY INSTALLATION
source "${CHECKOUT_DIR}/CI/linux/01_install_dependencies.sh"

## BUILD OBS ##
source "${CHECKOUT_DIR}/CI/linux/02_build_obs.sh"

## PACKAGE OBS AND NOTARIZE ##
source "${CHECKOUT_DIR}/CI/linux/03_package_obs.sh"

## MAIN SCRIPT FUNCTIONS ##
print_usage() {
    echo "build-linux.sh - Build script for OBS-Studio\n"
    echo -e "Usage: ${0}\n" \
            "-h, --help                     : Print this help\n" \
            "-q, --quiet                    : Suppress most build process output\n" \
            "-v, --verbose                  : Enable more verbose build process output\n" \
            "-d, --skip-dependency-checks   : Skip dependency checks (default: off)\n" \
            "-p, --portable                 : Create portable build (default: off)\n" \
            "-pkg, --package                : Create distributable disk image (default: off)\n" \
            "--disable-pipewire             : Disable building with Pipewire support (default: off)\n" \
            "--build-dir                    : Specify alternative build directory (default: build)\n"
}

obs-build-main() {
    while true; do
        case "${1}" in
            -h | --help ) print_usage; exit 0 ;;
            -q | --quiet ) export QUIET=TRUE; shift ;;
            -v | --verbose ) export VERBOSE=TRUE; shift ;;
            -d | --skip-dependency-checks ) SKIP_DEP_CHECKS=TRUE; shift ;;
            -p | --portable ) PORTABLE=TRUE; shift ;;
            -pkg | --package ) PACKAGE=TRUE; shift ;;
            --disable-pipewire ) DISABLE_PIPEWIRE=TRUE; shift ;;
            --build-dir ) BUILD_DIR="${2}"; shift 2 ;;
            -- ) shift; break ;;
            * ) break ;;
        esac
    done

    ensure_dir "${CHECKOUT_DIR}"
    step "Fetching OBS tags..."
    git fetch origin --tags

    GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
    GIT_HASH=$(git rev-parse --short HEAD)
    GIT_TAG=$(git describe --tags --abbrev=0)

    if [ "${BUILD_FOR_DISTRIBUTION}" ]; then
        VERSION_STRING="${GIT_TAG}"
    else
        VERSION_STRING="${GIT_TAG}-${GIT_HASH}"
    fi

    FILE_NAME="obs-studio-${VERSION_STRING}-Linux.deb"

    if [ -z "${SKIP_DEP_CHECKS}" ]; then
        install_dependencies
    fi

    build_obs

    if [ "${PACKAGE}" ]; then
        package_obs
    fi

    cleanup
}

obs-build-main $*
