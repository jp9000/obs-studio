#!/bin/bash

##############################################################################
# macOS full build script
##############################################################################
#
# This script contains all steps necessary to:
#
#   * Build OBS with all default plugins and dependencies
#   * Create a macOS application bundle
#   * Code-sign the macOS application-bundle
#   * Package a macOS installation image
#   * Notarize macOS application-bundle and/or installation image
#
# Parameters:
#   -b: Create macOS bundle
#   -d: Skip dependency checks
#   -p: Create macOS distribution image
#   -n: Notarize macOS app and disk image (implies bundling)
#   -s: Skip the build process (useful for bundling/packaging only)
#   -h: Print usage help
#
# Environment Variables (optional):
#   MACOS_DEPS_VERSION        : Pre-compiled macOS dependencies version
#   MACOS_CEF_BUILD_VERSION   : Chromium Embedded Framework version
#   VLC_VERISON               : VLC version
#   SPARKLE_VERSION           : Sparke Framework version
#   BUILD_DIR                 : Alternative directory to build OBS in
#
##############################################################################

# Halt on errors
set -eE

## SET UP ENVIRONMENT ##
PRODUCT_NAME="OBS-Studio"

CHECKOUT_DIR="$(/usr/bin/git rev-parse --show-toplevel)"
DEPS_BUILD_DIR="${CHECKOUT_DIR}/../obs-build-dependencies"
BUILD_DIR="${BUILD_DIR:-build}"
BUILD_CONFIG=${BUILD_CONFIG:-RelWithDebInfo}
CI_SCRIPTS="${CHECKOUT_DIR}/CI/scripts/macos"
CI_WORKFLOW="${CHECKOUT_DIR}/.github/workflows/main.yml"
CI_MACOS_CEF_VERSION=$(/bin/cat ${CI_WORKFLOW} | /usr/bin/sed -En "s/[ ]+MACOS_CEF_BUILD_VERSION: '([0-9]+)'/\1/p")
CI_DEPS_VERSION=$(/bin/cat ${CI_WORKFLOW} | /usr/bin/sed -En "s/[ ]+MACOS_DEPS_VERSION: '([0-9\-]+)'/\1/p")
CI_VLC_VERSION=$(/bin/cat ${CI_WORKFLOW} | /usr/bin/sed -En "s/[ ]+VLC_VERSION: '([0-9\.]+)'/\1/p")
CI_SPARKLE_VERSION=$(/bin/cat ${CI_WORKFLOW} | /usr/bin/sed -En "s/[ ]+SPARKLE_VERSION: '([0-9\.]+)'/\1/p")
CI_QT_VERSION=$(/bin/cat ${CI_WORKFLOW} | /usr/bin/sed -En "s/[ ]+QT_VERSION: '([0-9\.]+)'/\1/p" | /usr/bin/head -1)
CI_MIN_MACOS_VERSION=$(/bin/cat ${CI_WORKFLOW} | /usr/bin/sed -En "s/[ ]+MIN_MACOS_VERSION: '([0-9\.]+)'/\1/p")
NPROC="${NPROC:-$(sysctl -n hw.ncpu)}"

BUILD_DEPS=(
    "obs-deps ${MACOS_DEPS_VERSION:-${CI_DEPS_VERSION}}"
    "qt-deps ${QT_VERSION:-${CI_QT_VERSION}} ${MACOS_DEPS_VERSION:-${CI_DEPS_VERSION}}"
    "cef ${MACOS_CEF_BUILD_VERSION:-${CI_MACOS_CEF_VERSION}}"
    "vlc ${VLC_VERSION:-${CI_VLC_VERSION}}"
    "sparkle ${SPARKLE_VERSION:-${CI_SPARKLE_VERSION}}"
)

if [ -n "${TERM-}" ]; then
    COLOR_RED=$(/usr/bin/tput setaf 1)
    COLOR_GREEN=$(/usr/bin/tput setaf 2)
    COLOR_BLUE=$(/usr/bin/tput setaf 4)
    COLOR_ORANGE=$(/usr/bin/tput setaf 3)
    COLOR_RESET=$(/usr/bin/tput sgr0)
else
    COLOR_RED=""
    COLOR_GREEN=""
    COLOR_BLUE=""
    COLOR_ORANGE=""
    COLOR_RESET=""
fi


MACOS_VERSION="$(/usr/bin/sw_vers -productVersion)"
MACOS_MAJOR="$(/bin/echo ${MACOS_VERSION} | /usr/bin/cut -d '.' -f 1)"
MACOS_MINOR="$(/bin/echo ${MACOS_VERSION} | /usr/bin/cut -d '.' -f 2)"

## DEFINE UTILITIES ##

hr() {
    /bin/echo "${COLOR_BLUE}[${PRODUCT_NAME}] ${1}${COLOR_RESET}"
}

step() {
    /bin/echo "${COLOR_GREEN}  + ${1}${COLOR_RESET}"
}

info() {
    /bin/echo "${COLOR_ORANGE} + ${1}${COLOR_RESET}"
}

error() {
    /bin/echo "${COLOR_RED}  + ${1}${COLOR_RESET}"
}

exists() {
  /usr/bin/command -v "$1" >/dev/null 2>&1
}

ensure_dir() {
    [[ -n ${1} ]] && /bin/mkdir -p ${1} && builtin cd ${1}
}

cleanup() {
    /bin/rm -rf "${CHECKOUT_DIR}/${BUILD_DIR}/settings.json"
    unset CODESIGN_IDENT
    unset CODESIGN_IDENT_USER
    unset CODESIGN_IDENT_PASS
}

caught_error() {
    error "ERROR during build step: ${1}"
    cleanup
    exit 1
}

## CHECK AND INSTALL DEPENDENCIES ##
check_macos_version() {
    MIN_VERSION=${MIN_MACOS_VERSION:-${CI_MIN_MACOS_VERSION}}
    MIN_MAJOR=$(/bin/echo ${MIN_VERSION} | /usr/bin/cut -d '.' -f 1)
    MIN_MINOR=$(/bin/echo ${MIN_VERSION} | /usr/bin/cut -d '.' -f 2)

    if [ "${MACOS_MAJOR}" -lt "11" ] && [ "${MACOS_MINOR}" -lt "${MIN_MINOR}" ]; then
        error "WARNING: Minimum required macOS version is ${MIN_VERSION}, but running on ${MACOS_VERSION}"
    fi
}

install_homebrew_deps() {
    if ! exists brew; then
        error "Homebrew not found - please install homebrew (https://brew.sh)"
        exit 1
    fi

    if [ -d /usr/local/opt/openssl@1.0.2t ]; then
        brew uninstall openssl@1.0.2t
        brew untap local/openssl
    fi

    if [ -d /usr/local/opt/python@2.7.17 ]; then
        brew uninstall python@2.7.17
        brew untap local/python2
    fi

    brew bundle --file ${CI_SCRIPTS}/Brewfile

    check_curl
}

check_curl() {
    if [ "${MACOS_MAJOR}" -lt "11" ] && [ "${MACOS_MINOR}" -lt "15" ]; then
        if [ ! -d /usr/local/opt/curl ]; then
            step "Installing Homebrew curl.."
            brew install curl
        fi
        export CURLCMD="/usr/local/opt/curl/bin/curl"
    else
        export CURLCMD="curl"
    fi
}

check_ccache() {
    export PATH=/usr/local/opt/ccache/libexec:${PATH}
    CCACHE_STATUS=$(ccache -s >/dev/null 2>&1 && /bin/echo "CCache available." || /bin/echo "CCache is not available.")
    info "${CCACHE_STATUS}"
}

install_obs-deps() {
    hr "Setting up pre-built macOS OBS dependencies v${1}"
    ensure_dir ${DEPS_BUILD_DIR}
    step "Download..."
    ${CURLCMD} --progress-bar -L -C - -O https://github.com/obsproject/obs-deps/releases/download/${1}/macos-deps-${1}.tar.gz
    step "Unpack..."
    /usr/bin/tar -xf ./macos-deps-${1}.tar.gz -C /tmp
}

install_qt-deps() {
    hr "Setting up pre-built dependency QT v${1}"
    ensure_dir ${DEPS_BUILD_DIR}
    step "Download..."
    ${CURLCMD} --progress-bar -L -C - -O https://github.com/obsproject/obs-deps/releases/download/${2}/macos-qt-${1}-${2}.tar.gz
    step "Unpack..."
    /usr/bin/tar -xf ./macos-qt-${1}-${2}.tar.gz -C /tmp
    /usr/bin/xattr -r -d com.apple.quarantine /tmp/obsdeps
}

install_vlc() {
    hr "Setting up dependency VLC v${1}"
    ensure_dir ${DEPS_BUILD_DIR}
    step "Download..."
    ${CURLCMD} --progress-bar -L -C - -O https://downloads.videolan.org/vlc/${1}/vlc-${1}.tar.xz
    step "Unpack ..."
    /usr/bin/tar -xf vlc-${1}.tar.xz
}

install_sparkle() {
    hr "Setting up dependency Sparkle v${1} (might prompt for password)"
    ensure_dir ${DEPS_BUILD_DIR}/sparkle
    step "Download..."
    ${CURLCMD} --progress-bar -L -C - -o sparkle.tar.bz2 https://github.com/sparkle-project/Sparkle/releases/download/${1}/Sparkle-${1}.tar.bz2
    step "Unpack..."
    /usr/bin/tar -xf ./sparkle.tar.bz2
    step "Copy to destination..."
    if [ -d /Library/Frameworks/Sparkle.framework/ ]; then
        info "Warning - Sparkle framework already found in /Library/Frameworks"
    else
        sudo /bin/cp -R ./Sparkle.framework/ /Library/Frameworks/Sparkle.framework/
    fi
}

install_cef() {
    hr "Building dependency CEF v${1}"
    ensure_dir ${DEPS_BUILD_DIR}
    step "Download..."
    ${CURLCMD} --progress-bar -L -C - -O https://cdn-fastly.obsproject.com/downloads/cef_binary_${1}_macosx64.tar.bz2
    step "Unpack..."
    /usr/bin/tar -xf ./cef_binary_${1}_macosx64.tar.bz2
    cd ./cef_binary_${1}_macosx64
    step "Fix tests..."
    /usr/bin/sed -i '.orig' '/add_subdirectory(tests\/ceftests)/d' ./CMakeLists.txt
    /usr/bin/sed -i '.orig' 's/"'$(test "${MACOS_CEF_BUILD_VERSION:-${CI_MACOS_CEF_VERSION}}" -le 3770 && echo "10.9" || echo "10.10")'"/"'${MIN_MACOS_VERSION:-${CI_MIN_MACOS_VERSION}}'"/' ./cmake/cef_variables.cmake
    ensure_dir ./build
    step "Run CMAKE..."
    cmake \
        -DCMAKE_CXX_FLAGS="-std=c++11 -stdlib=libc++ -Wno-deprecated-declarations"\
        -DCMAKE_EXE_LINKER_FLAGS="-std=c++11 -stdlib=libc++"\
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${MIN_MACOS_VERSION:-${CI_MIN_MACOS_VERSION}} \
        ..
    step "Build..."
    /usr/bin/make -j${NPROC}
    if [ ! -d libcef_dll ]; then /bin/mkdir libcef_dll; fi
}

## CHECK AND INSTALL PACKAGING DEPENDENCIES ##
install_dmgbuild() {
    if ! exists dmgbuild; then
        if exists "pip3"; then
            PIPCMD="pip3"
        elif exists "pip"; then
            PIPCMD="pip"
        else
            error "Pip not found - please install pip via 'python -m ensurepip'"
            exit 1
        fi

        ${PIPCMD} install dmgbuild
    fi
}

## OBS BUILD FROM SOURCE ##
configure_obs_build() {
    ensure_dir "${CHECKOUT_DIR}/${BUILD_DIR}"

    CUR_DATE=$(/bin/date +"%Y-%m-%d@%H%M%S")
    NIGHTLY_DIR="${CHECKOUT_DIR}/nightly-${CUR_DATE}"
    PACKAGE_NAME=$(/usr/bin/find . -name "*.dmg")

    if [ -d ./OBS.app ]; then
        ensure_dir "${NIGHTLY_DIR}"
        /bin/mv ../${BUILD_DIR}/OBS.app .
        info "You can find OBS.app in ${NIGHTLY_DIR}"
    fi
    ensure_dir "${CHECKOUT_DIR}/${BUILD_DIR}"
    if ([ -n "${PACKAGE_NAME}" ] && [ -f ${PACKAGE_NAME} ]); then
        ensure_dir "${NIGHTLY_DIR}"
        /bin/mv ../${BUILD_DIR}/$(basename "${PACKAGE_NAME}") .
        info "You can find ${PACKAGE_NAME} in ${NIGHTLY_DIR}"
    fi

    ensure_dir "${CHECKOUT_DIR}/${BUILD_DIR}"

    hr "Run CMAKE for OBS..."
    cmake -DENABLE_SPARKLE_UPDATER=ON \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${MIN_MACOS_VERSION:-${CI_MIN_MACOS_VERSION}} \
        -DQTDIR="/tmp/obsdeps" \
        -DSWIGDIR="/tmp/obsdeps" \
        -DDepsPath="/tmp/obsdeps" \
        -DVLCPath="${DEPS_BUILD_DIR}/vlc-${VLC_VERSION:-${CI_VLC_VERSION}}" \
        -DBUILD_BROWSER=ON \
        -DBROWSER_LEGACY="$(test "${MACOS_CEF_BUILD_VERSION:-${CI_MACOS_CEF_VERSION}}" -le 3770 && echo "ON" || echo "OFF")" \
        -DWITH_RTMPS=ON \
        -DCEF_ROOT_DIR="${DEPS_BUILD_DIR}/cef_binary_${MACOS_CEF_BUILD_VERSION:-${CI_MACOS_CEF_VERSION}}_macosx64" \
        -DCMAKE_BUILD_TYPE="${BUILD_CONFIG}" \
        ..

}

run_obs_build() {
    ensure_dir "${CHECKOUT_DIR}/${BUILD_DIR}"
    hr "Build OBS..."
    /usr/bin/make -j${NPROC}
}

## OBS BUNDLE AS MACOS APPLICATION ##
bundle_dylibs() {
    ensure_dir "${CHECKOUT_DIR}/${BUILD_DIR}"

    if [ ! -d ./OBS.app ]; then
        error "No OBS.app bundle found"
        exit 1
    fi

    hr "Bundle dylibs for macOS application"

    step "Run dylibBundler.."

    BUNDLE_PLUGINS=(
        ./OBS.app/Contents/PlugIns/coreaudio-encoder.so
        ./OBS.app/Contents/PlugIns/decklink-ouput-ui.so
        ./OBS.app/Contents/PlugIns/decklink-captions.so
        ./OBS.app/Contents/PlugIns/frontend-tools.so
        ./OBS.app/Contents/PlugIns/image-source.so
        ./OBS.app/Contents/PlugIns/mac-avcapture.so
        ./OBS.app/Contents/PlugIns/mac-capture.so
        ./OBS.app/Contents/PlugIns/mac-decklink.so
        ./OBS.app/Contents/PlugIns/mac-syphon.so
        ./OBS.app/Contents/PlugIns/mac-vth264.so
        ./OBS.app/Contents/PlugIns/mac-virtualcam.so
        ./OBS.app/Contents/PlugIns/obs-browser.so
        ./OBS.app/Contents/PlugIns/obs-ffmpeg.so
        ./OBS.app/Contents/PlugIns/obs-filters.so
        ./OBS.app/Contents/PlugIns/obs-transitions.so
        ./OBS.app/Contents/PlugIns/obs-vst.so
        ./OBS.app/Contents/PlugIns/rtmp-services.so
        ./OBS.app/Contents/MacOS/obs-ffmpeg-mux
        ./OBS.app/Contents/MacOS/obslua.so
        ./OBS.app/Contents/MacOS/_obspython.so
        ./OBS.app/Contents/PlugIns/obs-x264.so
        ./OBS.app/Contents/PlugIns/text-freetype2.so
        ./OBS.app/Contents/PlugIns/obs-outputs.so
        )
    if ! [ "${MACOS_CEF_BUILD_VERSION:-${CI_MACOS_CEF_VERSION}}" -le 3770 ]; then
        ${CI_SCRIPTS}/app/dylibbundler -cd -of -a ./OBS.app -q -f \
            -s ./OBS.app/Contents/MacOS \
            -s "${DEPS_BUILD_DIR}/sparkle/Sparkle.framework" \
            -s ./rundir/${BUILD_CONFIG}/bin/ \
            $(echo "${BUNDLE_PLUGINS[@]/#/-x }")
    else
        ${CI_SCRIPTS}/app/dylibbundler -cd -of -a ./OBS.app -q -f \
            -s ./OBS.app/Contents/MacOS \
            -s "${DEPS_BUILD_DIR}/sparkle/Sparkle.framework" \
            -s ./rundir/${BUILD_CONFIG}/bin/ \
            $(echo "${BUNDLE_PLUGINS[@]/#/-x }") \
            -x ./OBS.app/Contents/PlugIns/obs-browser-page
    fi

    step "Move libobs-opengl to final destination"
    if [ -f "./libobs-opengl/libobs-opengl.so" ]; then
        /bin/cp ./libobs-opengl/libobs-opengl.so ./OBS.app/Contents/Frameworks
    else
        /bin/cp ./libobs-opengl/${BUILD_CONFIG}/libobs-opengl.so ./OBS.app/Contents/Frameworks
    fi

    step "Copy QtNetwork for plugin support"
    /bin/cp -R /tmp/obsdeps/lib/QtNetwork.framework ./OBS.app/Contents/Frameworks
    /bin/chmod -R +w ./OBS.app/Contents/Frameworks/QtNetwork.framework
    /bin/rm -r ./OBS.app/Contents/Frameworks/QtNetwork.framework/Headers
    /bin/rm -r ./OBS.app/Contents/Frameworks/QtNetwork.framework/Versions/5/Headers/
    /bin/chmod 644 ./OBS.app/Contents/Frameworks/QtNetwork.framework/Versions/5/Resources/Info.plist
    install_name_tool -id @executable_path/../Frameworks/QtNetwork.framework/Versions/5/QtNetwork ./OBS.app/Contents/Frameworks/QtNetwork.framework/Versions/5/QtNetwork
    install_name_tool -change /tmp/obsdeps/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore ./OBS.app/Contents/Frameworks/QtNetwork.framework/Versions/5/QtNetwork
}

install_frameworks() {
    ensure_dir "${CHECKOUT_DIR}/${BUILD_DIR}"

    if [ ! -d ./OBS.app ]; then
        error "No OBS.app bundle found"
        exit 1
    fi

    hr "Adding Chromium Embedded Framework"
    step "Copy Framework..."
    /bin/cp -R "${DEPS_BUILD_DIR}/cef_binary_${MACOS_CEF_BUILD_VERSION:-${CI_MACOS_CEF_VERSION}}_macosx64/Release/Chromium Embedded Framework.framework" ./OBS.app/Contents/Frameworks/
}

prepare_macos_bundle() {
    ensure_dir "${CHECKOUT_DIR}/${BUILD_DIR}"

    if [ ! -d ./rundir/${BUILD_CONFIG}/bin ]; then
        error "No OBS build found"
        return
    fi

    if [ -d ./OBS.app ]; then /bin/rm -rf ./OBS.app; fi

    hr "Preparing OBS.app bundle"
    step "Copy binary and plugins..."
    /bin/mkdir -p OBS.app/Contents/MacOS
    /bin/mkdir OBS.app/Contents/PlugIns
    /bin/mkdir OBS.app/Contents/Resources
    /bin/mkdir OBS.app/Contents/Frameworks

    /bin/cp rundir/${BUILD_CONFIG}/bin/obs ./OBS.app/Contents/MacOS
    /bin/cp rundir/${BUILD_CONFIG}/bin/obs-ffmpeg-mux ./OBS.app/Contents/MacOS
    /bin/cp rundir/${BUILD_CONFIG}/bin/libobsglad.0.dylib ./OBS.app/Contents/MacOS
    if ! [ "${MACOS_CEF_BUILD_VERSION:-${CI_MACOS_CEF_VERSION}}" -le 3770 ]; then
        /bin/cp -R "rundir/${BUILD_CONFIG}/bin/OBS Helper.app" "./OBS.app/Contents/Frameworks/OBS Helper.app"
        /bin/cp -R "rundir/${BUILD_CONFIG}/bin/OBS Helper (GPU).app" "./OBS.app/Contents/Frameworks/OBS Helper (GPU).app"
        /bin/cp -R "rundir/${BUILD_CONFIG}/bin/OBS Helper (Plugin).app" "./OBS.app/Contents/Frameworks/OBS Helper (Plugin).app"
        /bin/cp -R "rundir/${BUILD_CONFIG}/bin/OBS Helper (Renderer).app" "./OBS.app/Contents/Frameworks/OBS Helper (Renderer).app"
    fi
    /bin/cp -R rundir/${BUILD_CONFIG}/data ./OBS.app/Contents/Resources
    /bin/cp ${CI_SCRIPTS}/app/AppIcon.icns ./OBS.app/Contents/Resources
    /bin/cp -R rundir/${BUILD_CONFIG}/obs-plugins/ ./OBS.app/Contents/PlugIns
    /bin/cp ${CI_SCRIPTS}/app/Info.plist ./OBS.app/Contents
    # Scripting plugins are required to be placed in same directory as binary
    if [ -d ./OBS.app/Contents/Resources/data/obs-scripting ]; then
        /bin/mv ./OBS.app/Contents/Resources/data/obs-scripting/obslua.so ./OBS.app/Contents/MacOS/
        /bin/mv ./OBS.app/Contents/Resources/data/obs-scripting/_obspython.so ./OBS.app/Contents/MacOS/
        /bin/mv ./OBS.app/Contents/Resources/data/obs-scripting/obspython.py ./OBS.app/Contents/MacOS/
        /bin/rm -rf ./OBS.app/Contents/Resources/data/obs-scripting/
    fi

    bundle_dylibs
    install_frameworks

    /bin/cp ${CI_SCRIPTS}/app/OBSPublicDSAKey.pem ./OBS.app/Contents/Resources

    step "Set bundle meta information..."
    /usr/bin/plutil -insert CFBundleVersion -string ${GIT_TAG}-${GIT_HASH} ./OBS.app/Contents/Info.plist
    /usr/bin/plutil -insert CFBundleShortVersionString -string ${GIT_TAG}-${GIT_HASH} ./OBS.app/Contents/Info.plist
    /usr/bin/plutil -insert OBSFeedsURL -string https://obsproject.com/osx_update/feeds.xml ./OBS.app/Contents/Info.plist
    /usr/bin/plutil -insert SUFeedURL -string https://obsproject.com/osx_update/stable/updates.xml ./OBS.app/Contents/Info.plist
    /usr/bin/plutil -insert SUPublicDSAKeyFile -string OBSPublicDSAKey.pem ./OBS.app/Contents/Info.plist
}

## CREATE MACOS DISTRIBUTION AND INSTALLER IMAGE ##
prepare_macos_image() {
    ensure_dir "${CHECKOUT_DIR}/${BUILD_DIR}"

    if [ ! -d ./OBS.app ]; then
        error "No OBS.app bundle found"
        return
    fi

    hr "Preparing macOS installation image"

    if [ -f "${FILE_NAME}" ]; then
        /bin/rm "${FILE_NAME}"
    fi

    step "Run dmgbuild..."
    /bin/cp "${CI_SCRIPTS}/package/settings.json.template" ./settings.json
    /usr/bin/sed -i '' 's#\$\$VERSION\$\$#'"${GIT_TAG}"'#g' ./settings.json
    /usr/bin/sed -i '' 's#\$\$CI_PATH\$\$#'"${CI_SCRIPTS}"'#g' ./settings.json
    /usr/bin/sed -i '' 's#\$\$BUNDLE_PATH\$\$#'"${CHECKOUT_DIR}"'/build#g' ./settings.json
    /bin/echo -n "${COLOR_ORANGE}"
    dmgbuild "OBS-Studio ${GIT_TAG}" "${FILE_NAME}" -s ./settings.json
    /bin/echo -n "${COLOR_RESET}"

    if [ -n "${CODESIGN_OBS}" ]; then
        codesign_image
    fi
}

## SET UP CODE SIGNING AND NOTARIZATION CREDENTIALS ##
##############################################################################
# Apple Developer Identity needed:
#
#    + Signing the code requires a developer identity in the system's keychain
#    + codesign will look up and find the identity automatically
#
##############################################################################
read_codesign_ident() {
    if [ ! -n "${CODESIGN_IDENT}" ]; then
        step "Code-signing Setup"
        /usr/bin/read -p "${COLOR_ORANGE}  + Apple developer identity: ${COLOR_RESET}" CODESIGN_IDENT
    fi
}

##############################################################################
# Apple Developer credentials necessary:
#
#   + Signing for distribution and notarization require an active Apple
#     Developer membership
#   + An Apple Development identity is needed for code signing
#     (i.e. 'Apple Development: YOUR APPLE ID (PROVIDER)')
#   + Your Apple developer ID is needed for notarization
#   + An app-specific password is necessary for notarization from CLI
#   + This password will be stored in your macOS keychain under the identifier
#     'OBS-Codesign-Password'with access Apple's 'altool' only.
##############################################################################

read_codesign_pass() {
    if [ ! -n "${CODESIGN_IDENT_PASS}" ]; then
        step "Notarization Setup"
        /usr/bin/read -p "${COLOR_ORANGE}  + Apple account id: ${COLOR_RESET}" CODESIGN_IDENT_USER
        CODESIGN_IDENT_PASS=$(stty -echo; /usr/bin/read -p "${COLOR_ORANGE}  + Apple developer password: ${COLOR_RESET}" pwd; stty echo; /bin/echo $pwd)
        /bin/echo -n "${COLOR_ORANGE}"
        /usr/bin/xcrun altool --store-password-in-keychain-item "OBS-Codesign-Password" -u "${CODESIGN_IDENT_USER}" -p "${CODESIGN_IDENT_PASS}"
        /bin/echo -n "${COLOR_RESET}"
        CODESIGN_IDENT_SHORT=$(/bin/echo "${CODESIGN_IDENT}" | /usr/bin/sed -En "s/.+\((.+)\)/\1/p")
    fi
}

codesign_bundle() {
    if [ ! -n "${CODESIGN_OBS}" ]; then step "Skipping application bundle code signing"; return; fi

    ensure_dir "${CHECKOUT_DIR}/${BUILD_DIR}"
    trap "caught_error 'code-signing app'" ERR

    if [ ! -d ./OBS.app ]; then
        error "No OBS.app bundle found"
        return
    fi

    hr "Code-signing application bundle"

    /usr/bin/xattr -crs ./OBS.app

    read_codesign_ident
    step "Code-sign Sparkle framework..."
    /bin/echo -n "${COLOR_ORANGE}"
    /usr/bin/codesign --force --options runtime --sign "${CODESIGN_IDENT}" "./OBS.app/Contents/Frameworks/Sparkle.framework/Versions/A/Resources/Autoupdate.app/Contents/MacOS/fileop"
    /usr/bin/codesign --force --options runtime --sign "${CODESIGN_IDENT}" "./OBS.app/Contents/Frameworks/Sparkle.framework/Versions/A/Resources/Autoupdate.app/Contents/MacOS/Autoupdate"
    /usr/bin/codesign --force --options runtime --sign "${CODESIGN_IDENT}" --deep ./OBS.app/Contents/Frameworks/Sparkle.framework
    /bin/echo -n "${COLOR_RESET}"

    step "Code-sign CEF framework..."
    /bin/echo -n "${COLOR_ORANGE}"
    /usr/bin/codesign --force --options runtime --entitlements "${CI_SCRIPTS}/app/entitlements.plist" --sign "${CODESIGN_IDENT}" "./OBS.app/Contents/Frameworks/Chromium Embedded Framework.framework/Libraries/libEGL.dylib"
    /usr/bin/codesign --force --options runtime --entitlements "${CI_SCRIPTS}/app/entitlements.plist" --sign "${CODESIGN_IDENT}" "./OBS.app/Contents/Frameworks/Chromium Embedded Framework.framework/Libraries/libswiftshader_libEGL.dylib"
    /usr/bin/codesign --force --options runtime --entitlements "${CI_SCRIPTS}/app/entitlements.plist" --sign "${CODESIGN_IDENT}" "./OBS.app/Contents/Frameworks/Chromium Embedded Framework.framework/Libraries/libGLESv2.dylib"
    /usr/bin/codesign --force --options runtime --entitlements "${CI_SCRIPTS}/app/entitlements.plist" --sign "${CODESIGN_IDENT}" "./OBS.app/Contents/Frameworks/Chromium Embedded Framework.framework/Libraries/libswiftshader_libGLESv2.dylib"
    if ! [ "${MACOS_CEF_BUILD_VERSION:-${CI_MACOS_CEF_VERSION}}" -le 3770 ]; then
        /usr/bin/codesign --force --options runtime --entitlements "${CI_SCRIPTS}/app/entitlements.plist" --sign "${CODESIGN_IDENT}" "./OBS.app/Contents/Frameworks/Chromium Embedded Framework.framework/Libraries/libvk_swiftshader.dylib"
    fi

    /bin/echo -n "${COLOR_RESET}"

    if ! [ "${MACOS_CEF_BUILD_VERSION:-${CI_MACOS_CEF_VERSION}}" -le 3770 ]; then
        step "Code-sign CEF helper apps..."
        /bin/echo -n "${COLOR_ORANGE}"
        /usr/bin/codesign --force --options runtime --entitlements "${CI_SCRIPTS}/helpers/helper-entitlements.plist" --sign "${CODESIGN_IDENT}" --deep "./OBS.app/Contents/Frameworks/OBS Helper.app"
        /usr/bin/codesign --force --options runtime --entitlements "${CI_SCRIPTS}/helpers/helper-gpu-entitlements.plist" --sign "${CODESIGN_IDENT}" --deep "./OBS.app/Contents/Frameworks/OBS Helper (GPU).app"
        /usr/bin/codesign --force --options runtime --entitlements "${CI_SCRIPTS}/helpers/helper-plugin-entitlements.plist" --sign "${CODESIGN_IDENT}" --deep "./OBS.app/Contents/Frameworks/OBS Helper (Plugin).app"
        /usr/bin/codesign --force --options runtime --entitlements "${CI_SCRIPTS}/helpers/helper-renderer-entitlements.plist" --sign "${CODESIGN_IDENT}" --deep "./OBS.app/Contents/Frameworks/OBS Helper (Renderer).app"
        /bin/echo -n "${COLOR_RESET}"
    fi

    step "Code-sign DAL Plugin..."
    /bin/echo -n "${COLOR_ORANGE}"
    /usr/bin/codesign --force --options runtime --deep --sign "${CODESIGN_IDENT}" "./OBS.app/Contents/Resources/data/obs-mac-virtualcam.plugin"
    /bin/echo -n "${COLOR_RESET}"

    step "Code-sign OBS code..."
    /bin/echo -n "${COLOR_ORANGE}"
    /usr/bin/codesign --force --options runtime --entitlements "${CI_SCRIPTS}/app/entitlements.plist" --sign "${CODESIGN_IDENT}" --deep ./OBS.app
    /bin/echo -n "${COLOR_RESET}"

    step "Check code-sign result..."
    /usr/bin/codesign -dvv ./OBS.app
}

codesign_image() {
    if [ ! -n "${CODESIGN_OBS}" ]; then step "Skipping installer image code signing"; return; fi

    ensure_dir "${CHECKOUT_DIR}/${BUILD_DIR}"
    trap "caught_error 'code-signing image'" ERR

    if [ ! -f "${FILE_NAME}" ]; then
        error "No OBS disk image found"
        return
    fi

    hr "Code-signing installation image"

    read_codesign_ident

    step "Code-sign OBS installer image..."
    /bin/echo -n "${COLOR_ORANGE}";
    /usr/bin/codesign --force --sign "${CODESIGN_IDENT}" "${FILE_NAME}"
    /bin/echo -n "${COLOR_RESET}"
    step "Check code-sign result..."
    /usr/bin/codesign -dvv "${FILE_NAME}"
}

## BUILD FROM SOURCE META FUNCTION ##
full-build-macos() {
    if [ -n "${SKIP_BUILD}" ]; then step "Skipping full build"; return; fi

    if [ ! -n "${SKIP_DEPS}" ]; then

        hr "Installing Homebrew dependencies"
        install_homebrew_deps

        for DEPENDENCY in "${BUILD_DEPS[@]}"; do
            set -- ${DEPENDENCY}
            trap "caught_error ${DEPENDENCY}" ERR
            FUNC_NAME="install_${1}"
            ${FUNC_NAME} ${2} ${3}
        done

        check_ccache
        trap "caught_error 'cmake'" ERR
    fi

    configure_obs_build
    run_obs_build
}

## BUNDLE MACOS APPLICATION META FUNCTION ##
bundle_macos() {
    if [ ! -n "${BUNDLE_OBS}" ]; then step "Skipping application bundle creation"; return; fi

    hr "Creating macOS app bundle"
    trap "caught_error 'bundle app'" ERR
    ensure_dir ${CHECKOUT_DIR}
    prepare_macos_bundle
}

## PACKAGE MACOS DISTRIBUTION IMAGE META FUNCTION ##
package_macos() {
    if [ ! -n "${PACKAGE_OBS}" ]; then step "Skipping installer image creation"; return; fi

    hr "Creating macOS .dmg image"
    trap "caught_error 'package app'" ERR

    install_dmgbuild
    prepare_macos_image
}

## NOTARIZATION META FUNCTION ##
notarize_macos() {
    if [ ! -n "${NOTARIZE_OBS}" ]; then step "Skipping macOS notarization"; return; fi;

    hr "Notarizing OBS for macOS"
    trap "caught_error 'notarizing app'" ERR

    ensure_dir "${CHECKOUT_DIR}/${BUILD_DIR}"

    if [ -f "${FILE_NAME}" ]; then
        NOTARIZE_TARGET="${FILE_NAME}"
        xcnotary precheck "./OBS.app"
    elif [ -d "OBS.app" ]; then
        NOTARIZE_TARGET="./OBS.app"
    else
        error "No notarization app bundle ('OBS.app') or disk image ('${FILE_NAME}') found"
        return
    fi

    if [ "$?" -eq 0 ]; then
        read_codesign_ident
        read_codesign_pass

        step "Run xcnotary with ${NOTARIZE_TARGET}..."
        xcnotary notarize "${NOTARIZE_TARGET}" --developer-account "${CODESIGN_IDENT_USER}" --developer-password-keychain-item "OBS-Codesign-Password" --provider "${CODESIGN_IDENT_SHORT}"
    fi
}

## MAIN SCRIPT FUNCTIONS ##
print_usage() {
    /bin/echo "full-build-macos.sh - Build helper script for OBS-Studio\n"
    /bin/echo "Usage: ${0}\n" \
        "-d: Skip dependency checks\n" \
        "-b: Create macOS app bundle\n" \
        "-c: Codesign macOS app bundle\n" \
        "-p: Package macOS app into disk image\n" \
        "-n: Notarize macOS app and disk image (implies -b)\n" \
        "-s: Skip build process (useful for bundling/packaging only)\n" \
        "-h: Print this help"
    exit 0
}

obs-build-main() {
    ensure_dir ${CHECKOUT_DIR}
    check_macos_version
    step "Fetching OBS tags..."
    /usr/bin/git fetch origin --tags
    GIT_BRANCH=$(/usr/bin/git rev-parse --abbrev-ref HEAD)
    GIT_HASH=$(/usr/bin/git rev-parse --short HEAD)
    GIT_TAG=$(/usr/bin/git describe --tags --abbrev=0)
    FILE_NAME="obs-studio-${GIT_TAG}-${GIT_HASH}-macOS.dmg"

    ##########################################################################
    # IMPORTANT:
    #
    # Be careful when choosing to notarize and code-sign. The script will try
    # to sign any pre-existing bundle but also pre-existing images.
    #
    # This could lead to a package containing a non-signed bundle, which
    # will then fail notarization.
    #
    # To avoid this, run this script with -b -c first, then -p -c or -p -n
    # after to make sure that a code-signed bundle will be packaged.
    #
    ##########################################################################

    while getopts ":hdsbnpc" OPTION; do
        case ${OPTION} in
            h) print_usage ;;
            d) SKIP_DEPS=1 ;;
            s) SKIP_BUILD=1 ;;
            b) BUNDLE_OBS=1 ;;
            n) CODESIGN_OBS=1; NOTARIZE_OBS=1 ;;
            p) PACKAGE_OBS=1 ;;
            c) CODESIGN_OBS=1 ;;
            \?) ;;
        esac
    done

    full-build-macos
    bundle_macos
    codesign_bundle
    package_macos
    codesign_image
    notarize_macos

    cleanup
}

obs-build-main $*
