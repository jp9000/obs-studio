#!/bin/bash

set -ex
ccache -s || echo "CCache is not available."
mkdir build && cd build
cmake -DBUILD_BROWSER=ON -DCEF_ROOT_DIR="../cef_binary_${LINUX_CEF_BUILD_VERSION}_linux64" ..
