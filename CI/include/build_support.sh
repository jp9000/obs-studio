#!/bin/bash

##############################################################################
# Unix support functions
##############################################################################
#
# This script file can be included in build scripts for UNIX-compatible
# shells to compose build scripts.
#
##############################################################################

## DEFINE UTILITIES ##

if [ -z "${QUIET}" ]; then
    status() {
        echo -e "${COLOR_BLUE}[${PRODUCT_NAME}] ${1}${COLOR_RESET}"
    }

    step() {
        echo -e "${COLOR_GREEN}  + ${1}${COLOR_RESET}"
    }

    info() {
        echo -e "${COLOR_ORANGE}  + ${1}${COLOR_RESET}"
    }

    error() {
        echo -e "${COLOR_RED}  + ${1}${COLOR_RESET}"
    }
else
    status() {
        :
    }

    step() {
        :
    }

    info() {
        :
    }

    error() {
        echo -e "${COLOR_RED}  + ${1}${COLOR_RESET}"
    }
fi

exists() {
  /usr/bin/command -v "$1" >/dev/null 2>&1
}

ensure_dir() {
    [ -n "${1}" ] && /bin/mkdir -p "${1}" && builtin cd "${1}"
}

cleanup() {
    :
}

caught_error() {
    error "ERROR during build step: ${1}"
    cleanup
    exit 1
}

# Setup build environment

BUILD_DIR="${BUILD_DIR:-build}"
BUILD_CONFIG="${BUILD_CONFIG:-RelWithDebInfo}"
CI_WORKFLOW="${CHECKOUT_DIR}/.github/workflows/main.yml"
CURRENT_ARCH=$(uname -m)
CURRENT_DATE="$(date +"%Y-%m-%d")"

## Utility functions ##

check_ccache() {
    if ccache -V >/dev/null 2>&1; then
        info "CCache available"
        CMAKE_CCACHE_OPTIONS="-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache"

        if [ "${CI}" ]; then
            ccache --set-config=cache_dir=${GITHUB_WORKSPACE:-${HOME}}/.ccache
            ccache --set-config=max_size=${CCACHE_SIZE:-500M}
            ccache --set-config=compression=true
            ccache -z
        fi
    else
        info "CCache not available"
    fi
}

_add_ccache_to_path() {
    if [ "${CMAKE_CCACHE_OPTIONS}" ]; then
        PATH="/usr/local/opt/ccache/libexec:${PATH}"
        status "Compiler Info:"
        local IFS=$'\n'
        for COMPILER_INFO in $(type cc c++ gcc g++ clang clang++); do
            info "${COMPILER_INFO}"
        done
    fi
}

safe_fetch() {
    if [ $# -lt 2 ]; then
        error "Usage: safe_fetch URL HASH"
        return 1
    fi

    while true; do
        case "${1}" in
            -n | --nocontinue ) NOCONTINUE=TRUE; shift ;;
            -- ) shift; break ;;
            * ) break ;;
        esac
    done

    DOWNLOAD_URL="${1}"
    DOWNLOAD_HASH="${2}"
    DOWNLOAD_FILE="$(basename ${DOWNLOAD_URL})"
    CURLCMD=${CURLCMD:-curl}


    if [ "${NOCONTINUE}" ]; then
        ${CURLCMD/--continue-at -/} "${DOWNLOAD_URL}"
    else
        ${CURLCMD} "${DOWNLOAD_URL}"
    fi

    if [ "${DOWNLOAD_HASH}" = "$(sha256sum "${DOWNLOAD_FILE}" | cut -d " " -f 1)" ]; then
        info "${DOWNLOAD_FILE} downloaded successfully and passed hash check"
        return 0
    else
        error "${DOWNLOAD_FILE} downloaded successfully and failed hash check"
        return 1
    fi
}

check_and_fetch() {
    if [ $# -lt 2 ]; then
        caught_error "Usage: check_and_fetch URL HASH"
    fi

    while true; do
        case "${1}" in
            -n | --nocontinue ) NOCONTINUE=TRUE; shift ;;
            -- ) shift; break ;;
            * ) break ;;
        esac
    done

    DOWNLOAD_URL="${1}"
    DOWNLOAD_HASH="${2}"
    DOWNLOAD_FILE="$(basename "${DOWNLOAD_URL}")"

    if [ -f "${DOWNLOAD_FILE}" ] && [ "${DOWNLOAD_HASH}" = "$(sha256sum "${DOWNLOAD_FILE}" | cut -d " " -f 1)" ]; then
        info "${DOWNLOAD_FILE} exists and passed hash check"
        return 0
    else
        safe_fetch "${DOWNLOAD_URL}" "${DOWNLOAD_HASH}"
    fi
}

github_fetch() {
    if [ $# -ne 3 ]; then
        error "Usage: github_fetch GITHUB_USER GITHUB_REPOSITORY GITHUB_COMMIT_HASH"
        return 1
    fi

    GH_USER="${1}"
    GH_REPO="${2}"
    GH_REF="${3}"

    if [ -d "./.git" ]; then
        info "Repository ${GH_USER}/${GH_REPO} already exists, updating..."
        git config advice.detachedHead false
        git config remote.origin.url "https://github.com/${GH_USER}/${GH_REPO}.git"
        git config remote.origin.fetch "+refs/heads/master:refs/remotes/origin/master"
        git config remote.origin.tapOpt --no-tags

        if ! git rev-parse -q --verify "${GH_COMMIT}^{commit}"; then
            git fetch origin
        fi

        git checkout -f "${GH_REF}" --
        git reset --hard "${GH_REF}" --
        if [ -d "./.gitmodules" ]; then
            git submodule foreach --recursive git submodule sync
            git submodule update --init --recursive
        fi

    else
        git clone "https://github.com/${GH_USER}/${GH_REPO}.git" "$(pwd)"
        git config advice.detachedHead false
        info "Checking out commit ${GH_REF}.."
        git checkout -f "${GH_REF}" --

        if [ -d "./.gitmodules" ]; then
            git submodule foreach --recursive git submodule sync
            git submodule update --init --recursive
        fi
    fi
}

apply_patch() {
    if [ $# -ne 2 ]; then
        error "Usage: apply_patch PATCH_URL PATCH_HASH"
        return 1
    fi

    COMMIT_URL="${1}"
    COMMIT_HASH="${2}"
    PATCH_FILE="$(basename ${COMMIT_URL})"

    if [ "${COMMIT_URL:0:5}" = "https" ]; then
        ${CURLCMD:-curl} "${COMMIT_URL}"
        if [ "${COMMIT_HASH}" = "$(sha256sum ${PATCH_FILE} | cut -d " " -f 1)" ]; then
            info "${PATCH_FILE} downloaded successfully and passed hash check"
        else
            error "${PATCH_FILE} downloaded successfully and failed hash check"
            return 1
        fi

        info "Applying patch ${COMMIT_URL}"
    else
        PATCH_FILE="${COMMIT_URL}"
    fi

    patch -g 0 -f -p1 -i "${PATCH_FILE}"
}
