#!/usr/bin/env bash

##############################################################################
# FreeBSD support functions
##############################################################################
#
# This script file can be included in build scripts for FreeBSD.
#
##############################################################################

# Setup build environment

if [ "${TERM-}" -a -x /usr/local/bin/tput ]; then
    COLOR_RED=$(/usr/local/bin/tput setaf 1)
    COLOR_GREEN=$(/usr/local/bin/tput setaf 2)
    COLOR_BLUE=$(/usr/local/bin/tput setaf 4)
    COLOR_ORANGE=$(/usr/local/bin/tput setaf 3)
    COLOR_RESET=$(/usr/local/bin/tput sgr0)
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
