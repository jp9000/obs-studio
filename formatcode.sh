#!/usr/bin/env bash
# Original source https://github.com/Project-OSRM/osrm-backend/blob/master/scripts/format.sh

set -o errexit
set -o pipefail
set -o nounset

if [ ${#} -eq 1 ]; then
    VERBOSITY="--verbose"
else
    VERBOSITY=""
fi

# Runs the Clang Formatter in parallel on the code base.
# Return codes:
#  - 1 there are files to be formatted
#  - 0 everything looks fine

# Get CPU count
OS=$(uname)
NPROC=1
if [[ ${OS} = "Linux" ]] ; then
    NPROC=$(nproc)
elif [[ ${OS} = "Darwin" ]] ; then
    NPROC=$(sysctl -n hw.physicalcpu)
fi

# Discover clang-format
if type clang-format-10 2> /dev/null ; then
    CLANG_FORMAT=clang-format-10
elif type clang-format 2> /dev/null ; then
    # Clang format found, but need to check version
    CLANG_FORMAT=clang-format
    V=$(clang-format --version)
    if [[ $V != "*version 10.0*" ]]; then
        echo "clang-format is not 10.0 (returned ${V})"
        exit 1
    fi
else
    echo "No appropriate clang-format found (expected clang-format-10.0.0, or clang-format)"
    exit 1
fi

find . -type d \( \
    -path ./\*build -o \
    -path ./cmake -o \
    -path ./deps -o \
    -path ./plugins/decklink/\*/decklink-sdk -o \
    -path ./plugins/enc-amf -o \
    -path ./plugins/mac-syphon/syphon-framework -o \
    -path ./plugins/obs-outputs/ftl-sdk -o \
    -path ./plugins/obs-vst \
\) -prune -false -type f -o \
    -name '*.h' -or \
    -name '*.hpp' -or \
    -name '*.m' -or \
    -name '*.m,' -or \
    -name '*.c' -or \
    -name '*.cpp' \
 | xargs -L100 -P ${NPROC} ${CLANG_FORMAT} ${VERBOSITY} -i -style=file -fallback-style=none
