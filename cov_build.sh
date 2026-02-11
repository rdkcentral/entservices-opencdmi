#!/bin/bash
set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}

############################
# Clone meta-rdk-video for patches
echo "======================================================================================"
echo "Cloning meta-rdk-video for Thunder R4.4 compatibility patch"

if [ ! -d "meta-rdk-video" ]; then
    git clone --depth 1 https://github.com/rdkcentral/meta-rdk-video.git
fi

############################
# Apply Thunder R4.4 compatibility patch
echo "======================================================================================"
echo "Applying Thunder R4.4 compatibility patch to plugin"

cd ${GITHUB_WORKSPACE}
PATCH_DIR="${GITHUB_WORKSPACE}/meta-rdk-video/recipes-extended/entservices/files"

# Apply OCDM patches in exact order from Yocto recipe
PATCHES=(
    "0003-set-OCDM-sharepath-to-tmp-OCDM.patch"
    "0001-RDK-31882-Add-GstCaps-parsing-in-OCDM-to-rdkservices.patch"
    "0001-add_gstcaps_forcobalt_mediatype.patch"
    "0001-rdkservices_cbcs_changes.patch"
    "0002-Adding-Support-For-R4.patch"
    "0001-Add-a-new-metrics-punch-through-on-the-OCDM-framework-rdkservice.patch"
    "0001-set-OCDM-process-thread-name.patch"
)

for PATCH_NAME in "${PATCHES[@]}"; do
    PATCH_FILE="${PATCH_DIR}/${PATCH_NAME}"
    if [ -f "$PATCH_FILE" ]; then
        echo "Applying patch: $PATCH_NAME"
        
        # Temporarily rename plugin/ to OpenCDMi/ to match patch expectations
        mv plugin OpenCDMi 2>/dev/null || true
        
        # Apply the patch
        patch -p1 --forward --no-backup-if-mismatch < "$PATCH_FILE" || true
        
        # Rename back to plugin/
        mv OpenCDMi plugin 2>/dev/null || true
    fi
done

############################
# Build entservices-opencdmi
echo "======================================================================================"
echo "buliding entservices-opencdmi"

cd ${GITHUB_WORKSPACE}
cmake -G Ninja -S "$GITHUB_WORKSPACE" -B build/entservices-opencdmi \
-DUSE_THUNDER_R4=ON \
-DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
-DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
-DCMAKE_VERBOSE_MAKEFILE=ON \
-DCMAKE_DISABLE_FIND_PACKAGE_RFC=ON \
-DCMAKE_DISABLE_FIND_PACKAGE_DS=ON \
-DCOMCAST_CONFIG=OFF \
-DRDK_SERVICES_COVERITY=ON \
-DRDK_SERVICES_L1_TEST=OFF \
-DPLUGIN_OPENCDMI=ON \
-DDS_FOUND=ON \
-DHAS_FRONT_PANEL=ON \
-DHIDE_NON_EXTERNAL_SYMBOLS=OFF \
-DCMAKE_CXX_FLAGS="-DEXCEPTIONS_ENABLE=ON \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/audiocapturemgr \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/rdk/ds \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/ccec/drivers \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/headers/network \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks \
-I ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/thunder \
-I /usr/include/libdrm \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/devicesettings.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/Rfc.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/RBus.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/Telemetry.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/Udev.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/pkg.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/maintenanceMGR.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/readprocMockInterface.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/gdialservice.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/wpa_ctrl_mock.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/secure_wrappermock.h \
-include ${GITHUB_WORKSPACE}/entservices-testframework/Tests/mocks/HdmiCec.h \
-Wall -Werror -Wno-error=format \
-Wl,-wrap,system -Wl,-wrap,popen -Wl,-wrap,syslog \
-DENABLE_TELEMETRY_LOGGING -DHAS_API_SYSTEM \
-DUSE_THUNDER_R4 -DTHUNDER_VERSION=4 -DTHUNDER_VERSION_MAJOR=4 -DTHUNDER_VERSION_MINOR=4" \

cmake --build build/entservices-opencdmi --target install
echo "======================================================================================"
exit 0
