#!/bin/bash

##############################################################################
# Linux support functions
##############################################################################
#
# This script file can be included in build scripts for Linux.
#
##############################################################################

# Setup build environment

CI_LINUX_CEF_VERSION=$(cat "${CI_WORKFLOW}" | sed -En "s/[ ]+CEF_BUILD_VERSION_LINUX: '([0-9]+)'/\1/p")

if [ "${TERM-}" -a -z "${CI}" ]; then
    COLOR_RED=$(tput setaf 1)
    COLOR_GREEN=$(tput setaf 2)
    COLOR_BLUE=$(tput setaf 4)
    COLOR_ORANGE=$(tput setaf 3)
    COLOR_RESET=$(tput sgr0)
else
    COLOR_RED=""
    COLOR_GREEN=""
    COLOR_BLUE=""
    COLOR_ORANGE=""
    COLOR_RESET=""
fi

if [ "${CI}" -o "${QUIET}" ]; then
    export CURLCMD="curl --silent --show-error --location -O"
else
    export CURLCMD="curl --progress-bar --location --continue-at - -O"
fi
