#!/usr/bin/env bash

##############################################################################
# FreeBSD OBS package function
##############################################################################
#
# This script file can be included in build scripts for FreeBSD or run directly
#
##############################################################################

# Halt on errors
set -eE

package_obs() {
    status "Create FreeBSD debian package"
    trap "caught_error 'package app'" ERR

    ensure_dir "${CHECKOUT_DIR}"

    step "Package OBS..."
    cmake --build ${BUILD_DIR} -t package

    ZIP_NAME="$(/usr/bin/find "${BUILD_DIR}" -maxdepth 1 -type f -name "obs-studio-*.sh" | sort -rn | head -1)"

    if [ "${ZIP_NAME}" ]; then
        mv "${ZIP_NAME%.*}.sh" "${BUILD_DIR}/${FILE_NAME}.sh"
        mv "${ZIP_NAME%.*}.tar.gz" "${BUILD_DIR}/${FILE_NAME}.tar.gz"
        mv "${ZIP_NAME%.*}.tar.Z" "${BUILD_DIR}/${FILE_NAME}.tar.Z"
    else
        error "ERROR No suitable OBS debian package generated"
    fi
}

package-obs-standalone() {
    PRODUCT_NAME="OBS-Studio"

    CHECKOUT_DIR="$(git rev-parse --show-toplevel)"
    DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
    source ${CHECKOUT_DIR}/CI/include/build_support.sh
    source ${CHECKOUT_DIR}/CI/include/build_support_freebsd.sh

    step "Fetch OBS tags..."
    git fetch origin --tags

    GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
    GIT_HASH=$(git rev-parse --short HEAD)
    GIT_TAG=$(git describe --tags --abbrev=0)

    FILE_NAME="obs-studio-${GIT_TAG}-${GIT_HASH}-FreeBSD"
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
