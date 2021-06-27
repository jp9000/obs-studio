#!/bin/bash

##############################################################################
# Linux libobs plugin package function
##############################################################################
#
# This script file can be included in build scripts for Linux or run directly
#
##############################################################################

# Halt on errors
set -eE

package_obs() {
    status "Create Linux debian package"
    trap "caught_error 'package app'" ERR

    ensure_dir "${CHECKOUT_DIR}"

    step "Package OBS..."
    cmake --build ${BUILD_DIR} -t package

    DEB_NAME=$(find ${BUILD_DIR} -maxdepth 1 -type f -name "obs*.deb" | sort -rn | head -1)

    if [ "${DEB_NAME}" ]; then
        mv ${DEB_NAME} ${BUILD_DIR}/${FILE_NAME}
    else
        error "ERROR No suitable OBS debian package generated"
    fi
}

package-obs-standalone() {
    PRODUCT_NAME="OBS-Studio"

    CHECKOUT_DIR="$(git rev-parse --show-toplevel)"
    DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
    source "${CHECKOUT_DIR}/CI/include/build_support.sh"
    source "${CHECKOUT_DIR}/CI/include/build_support_linux.sh"

    step "Fetch OBS tags..."
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
    package_obs
}

print_usage() {
    echo -e "Usage: ${0}\n" \
            "-h, --help                     : Print this help\n" \
            "-q, --quiet                    : Suppress most build process output\n" \
            "-v, --verbose                  : Enable more verbose build process output\n" \
            "--build-dir                    : Specify alternative build directory (default: build)\n"
}

package-obs-main() {
    if [ -z "${_RUN_OBS_BUILD_SCRIPT}" ]; then
        while true; do
            case "${1}" in
                -h | --help ) print_usage; exit 0 ;;
                -q | --quiet ) export QUIET=TRUE; shift ;;
                -v | --verbose ) export VERBOSE=TRUE; shift ;;
                --build-dir ) BUILD_DIR="${2}"; shift 2 ;;
                -- ) shift; break ;;
                * ) break ;;
            esac
        done

        package-obs-standalone
    fi
}

package-obs-main $*
