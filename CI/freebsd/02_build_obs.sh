#!/usr/bin/env bash

##############################################################################
# Linux build function
##############################################################################
#
# This script file can be included in build scripts for Linux or run directly
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

# Function to configure OBS build
_configure_obs() {
    ensure_dir "${CHECKOUT_DIR}"
    status "Configuration of OBS build system..."
    check_ccache

    if [ "${TWITCH_CLIENTID}" -a "${TWICH_HASH}" ]; then
        TWITCH_OPTIONS="-DTWITCH_CLIENTID=\"${TWITCH_CLIENTID}\" -DTWITCH_HASH=\"${TWITCH_HASH}\""
    fi

    if [ "${RESTREAM_CLIENTID}" -a "${RESTREAM_HASH}" ]; then
        RESTREAM_OPTIONS="-DRESTREAM_CLIENTID=\"${RESTREAM_CLIENTID}\" -DRESTREAM_HASH=\"${RESTREAM_HASH}\""
    fi

    if [ "${PORTABLE}" ]; then
        PORTABLE_BUILD="ON"
    fi

    cmake -S . -B ${BUILD_DIR} -G Ninja \
        -DCMAKE_BUILD_TYPE=${BUILD_CONFIG} \
        -DLINUX_PORTABLE=${PORTABLE_BUILD:-OFF} \
        -DENABLE_PIPEWIRE=OFF \
        ${CCACHE_OPTIONS} \
        ${TWITCH_OPTIONS} \
        ${RESTREAM_OPTIONS} \
        ${CI:+-DENABLE_UNIT_TESTS=ON -DBUILD_FOR_DISTRIBUTION=${BUILD_FOR_DISTRIBUTION} -DOBS_BUILD_NUMBER=${GITHUB_RUN_ID}} \
        ${QUIET:+-Wno-deprecated -Wno-dev --log-level=ERROR}
}

# Function to backup previous build artifacts
_backup_artifacts() {
    ensure_dir "${CHECKOUT_DIR}"
    if [ -d "${BUILD_DIR}" ]; then
        status "Backup of old OBS build artifacts"

        CUR_DATE=$(/bin/date +"%Y-%m-%d@%H%M%S")
        NIGHTLY_DIR="${CHECKOUT_DIR}/nightly-${CUR_DATE}"
        PACKAGE_NAME=$(/usr/bin/find ${BUILD_DIR} -maxdepth 1 -name "*.tar.gz" | sort -rn | head -1)

        if [ "${PACKAGE_NAME}" ]; then
            step "Back up $(basename "${PACKAGE_NAME}")..."
            ensure_dir "${NIGHTLY_DIR}"
            ensure_dir "${CHECKOUT_DIR}"
            /usr/bin/find "${BUILD_DIR}" -maxdepth 1 \( -name "obs-studio-*.sh" -o -name "obs-studio-*.tar.gz" -o -name "obs-studio-*.tar.Z" \) -print0 | /usr/bin/xargs -0 -I {} /bin/mv {} ${NIGHTLY_DIR}/
            info "You can find $(basename "${PACKAGE_NAME}") in ${NIGHTLY_DIR}"
        fi
    fi
}

build-obs-standalone() {
    CHECKOUT_DIR="$(git rev-parse --show-toplevel)"
    PRODUCT_NAME="OBS-Studio"
    DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
    source "${CHECKOUT_DIR}/CI/include/build_support.sh"
    source "${CHECKOUT_DIR}/CI/include/build_support_freebsd.sh"

    build_obs
}

print_usage() {
    echo -e "Usage: ${0}\n" \
            "-h, --help                     : Print this help\n" \
            "-q, --quiet                    : Suppress most build process output\n" \
            "-v, --verbose                  : Enable more verbose build process output\n" \
            "-p, --portable                 : Create portable build (default: off)\n" \
            "--build-dir                    : Specify alternative build directory (default: build)\n"
}

build-obs-main() {
    if [ -z "${_RUN_OBS_BUILD_SCRIPT}" ]; then
        while true; do
            case "${1}" in
                -h | --help ) print_usage; exit 0 ;;
                -q | --quiet ) export QUIET=TRUE; shift ;;
                -v | --verbose ) export VERBOSE=TRUE; shift ;;
                -p | --portable ) export PORTABLE=TRUE; shift ;;
                --build-dir ) BUILD_DIR="${2}"; shift 2 ;;
                -- ) shift; break ;;
                * ) break ;;
            esac
        done

        build-obs-standalone
    fi
}

build-obs-main $*
