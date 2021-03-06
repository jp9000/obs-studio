#!/usr/bin/env bash
# Original source https://github.com/Project-OSRM/osrm-backend/blob/master/scripts/format.sh

set +x
set -o errexit
set -o pipefail
set -o nounset

# Runs the Clang Formatter in parallel on the code base.
# Return codes:
#  - 1 there are files to be formatted
#  - 0 everything looks fine

# Get CPU count
OS=$(uname)
NPROC=1
if [[ $OS = "Linux" || $OS = "Darwin" ]] ; then
    NPROC=$(getconf _NPROCESSORS_ONLN)
fi

# Discover clang-format
if type clang-format-10 2> /dev/null ; then
    CLANG_FORMAT=clang-format-10
elif type clang-format-8 2> /dev/null ; then
    CLANG_FORMAT=clang-format-8
else
    CLANG_FORMAT=clang-format
fi

find . -type d \( -path ./deps \
-o -path ./cmake \
-o -path ./plugins/decklink/win/decklink-sdk \
-o -path ./plugins/decklink/mac/decklink-sdk \
-o -path ./plugins/decklink/linux/decklink-sdk \
-o -path ./plugins/enc-amf \
-o -path ./plugins/mac-syphon/syphon-framework \
-o -path ./plugins/obs-outputs/ftl-sdk \
-o -path ./plugins/obs-vst \
-o -path ./build \) -prune -type f -o -name '*.h' -or -name '*.hpp' -or -name '*.m' -or -name '*.mm' -or -name '*.c' -or -name '*.cpp' \
| xargs -L100 -P${NPROC} ${CLANG_FORMAT} -i -style=file  -fallback-style=none
