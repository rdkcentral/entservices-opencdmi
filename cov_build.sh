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
PATCH_FILE="${GITHUB_WORKSPACE}/meta-rdk-video/recipes-extended/entservices/files/0001-rdkservices_cbcs_changes.patch"

if [ -f "$PATCH_FILE" ]; then
    echo "Found patch file: $PATCH_FILE"
    # The patch expects OpenCDMi/ directory, but our plugin is in plugin/
    # Also adjust git index lines and file paths
    sed -e 's|OpenCDMi/|plugin/|g' \
        -e 's|Index: git/OpenCDMi/|Index: git/plugin/|g' \
        -e 's|--- git.orig/OpenCDMi/|--- git.orig/plugin/|g' \
        -e 's|+++ git/OpenCDMi/|+++ git/plugin/|g' \
        "$PATCH_FILE" > /tmp/opencdmi_r4_patch.patch
    
    # Apply the patch - some hunks may fail if already applied or files differ
    # Expected partial failures: CapsParser.cpp/h (deleted files), CMakeLists.txt (differences)
    echo "Applying patch (some hunks may skip if already applied)..."
    patch -p1 -f --forward < /tmp/opencdmi_r4_patch.patch || {
        echo "Patch completed with some rejected hunks"
        
        # If FrameworkRPC.cpp still has old namespace, force apply just that file
        if grep -q "::OCDM::ISession" plugin/FrameworkRPC.cpp 2>/dev/null; then
            echo "FrameworkRPC.cpp still needs patching - extracting and applying separately"
            # Extract only FrameworkRPC.cpp changes from the patch
            grep -A 99999 "Index: git/plugin/FrameworkRPC.cpp" /tmp/opencdmi_r4_patch.patch | \
            grep -B 99999 -m 1 "^Index: " | head -n -1 > /tmp/frameworkrpc_only.patch || \
            grep -A 99999 "Index: git/plugin/FrameworkRPC.cpp" /tmp/opencdmi_r4_patch.patch > /tmp/frameworkrpc_only.patch
            
            # Apply with --forward to ignore reversed detection
            patch -p1 --forward < /tmp/frameworkrpc_only.patch || {
                echo "WARNING: FrameworkRPC.cpp patch failed - will verify manually"
            }
            rm -f /tmp/frameworkrpc_only.patch
        fi
    }
    
    # Verify Thunder R4.4 changes are present in key files
    echo "Verifying Thunder R4.4 compatibility changes..."
    if grep -q "Exchange::KeyId" plugin/CENCParser.h 2>/dev/null && \
       grep -q "Exchange::IAccessorOCDM" plugin/FrameworkRPC.cpp 2>/dev/null && \
       ! grep -q "::OCDM::ISession.*public ::OCDM::ISessionExt" plugin/FrameworkRPC.cpp 2>/dev/null; then
        echo "✓ Thunder R4.4 compatibility verified successfully"
    else
        echo "ERROR: Thunder R4.4 changes verification failed!"
        echo "CENCParser.h Exchange::KeyId:"
        grep -c "Exchange::KeyId" plugin/CENCParser.h || echo "NOT FOUND"
        echo "FrameworkRPC.cpp Exchange::IAccessorOCDM:"
        grep -c "Exchange::IAccessorOCDM" plugin/FrameworkRPC.cpp || echo "NOT FOUND"
        echo "FrameworkRPC.cpp old ::OCDM::ISession (line 239, should be Exchange::ISession):"
        grep  "class SessionImplementation.*ISession" plugin/FrameworkRPC.cpp | head -1
        exit 1
    fi
    
    rm -f /tmp/opencdmi_r4_patch.patch
else
    echo "ERROR: Patch file not found at $PATCH_FILE"
    exit 1
fi

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
