#!/bin/bash
#
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -x
set -e

log_info()
{
    echo "INFO: $*"
}

##############################
THUNDER_TOOLS_COMMIT_SHA="d5dd83c7c19c49c7f25c558c126500bd2d64f7a4"
THUNDER_COMMIT_SHA="2c0fcc5529e7da734be558ca6efa05d934dcce31"
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
cd ${GITHUB_WORKSPACE}

OS_NAME="$(uname -s)"
UBUNTU_ID=""

if [[ -f /etc/os-release ]]; then
    UBUNTU_ID="$(. /etc/os-release && echo "${ID}")"
fi

if [[ "${OS_NAME}" != "Linux" || "${UBUNTU_ID}" != "ubuntu" ]]; then
    echo "ERROR: Platform not supported. This script supports Ubuntu only."
    exit 1
fi

is_root=0
if [[ "$(id -u)" -eq 0 ]]; then
    is_root=1
fi

SUDO=""
if [[ "${is_root}" -eq 0 ]]; then
    if command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    else
        echo "ERROR: sudo is required for installing missing system packages."
        exit 1
    fi
fi

PYTHON_VENV_DIR="${GITHUB_WORKSPACE}/.build-deps-venv"
PYTHON_VENV_BIN="${PYTHON_VENV_DIR}/bin/python3"
BUILD_PYTHON_BIN="$(command -v python3)"
RESTORE_OWNER_USER="${SUDO_USER:-}"
RESTORE_OWNER_GROUP=""

if [[ -n "${RESTORE_OWNER_USER}" ]]; then
    RESTORE_OWNER_GROUP="$(id -gn "${RESTORE_OWNER_USER}")"
fi

ensure_python_venv()
{
    if [[ -x "${PYTHON_VENV_BIN}" ]] && \
       [[ -f "${PYTHON_VENV_DIR}/bin/activate" ]]; then
        return 0
    fi

    rm -rf "${PYTHON_VENV_DIR}"

    if ! python3 -m venv "${PYTHON_VENV_DIR}"; then
        echo "ERROR: Failed to create Python virtualenv at ${PYTHON_VENV_DIR}."
        echo "Ensure python3-venv is installed, then rerun build_dependencies.sh."
        exit 1
    fi
}

ensure_python_module()
{
    local module_name="$1"

    if python3 - <<PY >/dev/null 2>&1
import importlib.util
import sys
sys.exit(0 if importlib.util.find_spec("${module_name}") else 1)
PY
    then
        return 0
    fi

    ensure_python_venv

    # Use a repo-local virtualenv to avoid PEP 668 restrictions on system Python.
    "${PYTHON_VENV_BIN}" -m pip install "${module_name}"

    if "${PYTHON_VENV_BIN}" - <<PY >/dev/null 2>&1
import importlib.util
import sys
sys.exit(0 if importlib.util.find_spec("${module_name}") else 1)
PY
    then
        BUILD_PYTHON_BIN="${PYTHON_VENV_BIN}"
        return 0
    fi

    echo "ERROR: Failed to install Python module '${module_name}'."
    exit 1
}

run_apt_with_retry()
{
    local max_attempts="${APT_RETRY_ATTEMPTS:-5}"
    local retry_delay_seconds="${APT_RETRY_DELAY_SECONDS:-10}"
    local attempt=1
    local rc=0

    while [[ "${attempt}" -le "${max_attempts}" ]]; do
        if "$@"; then
            return 0
        else
            rc=$?
        fi

        if [[ "${attempt}" -ge "${max_attempts}" ]]; then
            break
        fi

        echo "WARNING: apt command failed (attempt ${attempt}/${max_attempts}). Retrying in ${retry_delay_seconds}s..."
        sleep "${retry_delay_seconds}"
        attempt=$((attempt + 1))
    done

    echo "ERROR: apt command failed after ${max_attempts} attempts."
    return "${rc}"
}

clone_if_missing()
{
    local repo_dir="$1"
    shift

    if [[ ! -d "${repo_dir}" ]]; then
        git clone "$@" "${repo_dir}"
    fi
}

apply_patch_if_needed()
{
    local patch_file="$1"

    if patch -p1 -N --dry-run < "${patch_file}" >/dev/null 2>&1; then
        patch -p1 -N < "${patch_file}"
        return 0
    fi

    if patch -p1 -R --dry-run < "${patch_file}" >/dev/null 2>&1; then
        echo "INFO: Patch already applied: ${patch_file}"
        return 0
    fi

    echo "ERROR: Failed to apply patch ${patch_file}"
    exit 1
}

restore_workspace_ownership()
{
    if [[ -z "${RESTORE_OWNER_USER}" || -z "${RESTORE_OWNER_GROUP}" ]]; then
        return 0
    fi

    chown -R "${RESTORE_OWNER_USER}:${RESTORE_OWNER_GROUP}" \
        "${GITHUB_WORKSPACE}/install" \
        "${GITHUB_WORKSPACE}/build" \
        "${GITHUB_WORKSPACE}/ThunderTools" \
        "${GITHUB_WORKSPACE}/Thunder" \
        "${GITHUB_WORKSPACE}/ThunderClientLibraries" \
        "${GITHUB_WORKSPACE}/entservices-apis" \
        "${GITHUB_WORKSPACE}/entservices-testframework" \
        "${GITHUB_WORKSPACE}/googletest" \
        "${GITHUB_WORKSPACE}/trower-base64" \
        "${PYTHON_VENV_DIR}" 2>/dev/null || true
}

trap restore_workspace_ownership EXIT

# # ############################# 
#1. Install Dependencies and packages

if [[ -n "${SUDO}" ]]; then
    run_apt_with_retry ${SUDO} apt update
    run_apt_with_retry ${SUDO} apt install -y valgrind lcov clang meson curl \
        libsqlite3-dev libcurl4-openssl-dev libsystemd-dev \
        libboost-all-dev libwebsocketpp-dev libunwind-dev \
        libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
        libcunit1 libcunit1-dev protobuf-compiler-grpc \
        libgrpc-dev libgrpc++-dev python3-pip python3-venv
else
    run_apt_with_retry apt update
    run_apt_with_retry apt install -y valgrind lcov clang meson curl \
        libsqlite3-dev libcurl4-openssl-dev libsystemd-dev \
        libboost-all-dev libwebsocketpp-dev libunwind-dev \
        libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
        libcunit1 libcunit1-dev protobuf-compiler-grpc \
        libgrpc-dev libgrpc++-dev python3-pip python3-venv
fi
    log_info "System build packages are available."

ensure_python_module jsonref
    log_info "Python build dependency 'jsonref' is available."

############################
# Build trevor-base64
if [ ! -d "trower-base64" ]; then
git clone https://github.com/xmidt-org/trower-base64.git
fi
cd trower-base64
meson setup --warnlevel 3 --werror --prefix "${GITHUB_WORKSPACE}/install/usr" build
ninja -C build
ninja -C build install
cd ..
log_info "Dependency build completed: trower-base64."
###########################################
# Clone the required repositories


clone_if_missing ThunderTools --branch R4_4-RDK https://github.com/rdkcentral/ThunderTools.git
cd ThunderTools
git checkout $THUNDER_TOOLS_COMMIT_SHA
cd ..

clone_if_missing Thunder --branch R4_4-RDK https://github.com/rdkcentral/Thunder.git
cd Thunder
git checkout $THUNDER_COMMIT_SHA
cd ..

clone_if_missing ThunderClientLibraries --branch R4.4.2 https://github.com/rdkcentral/ThunderClientLibraries.git

clone_if_missing entservices-apis --branch main https://github.com/rdkcentral/entservices-apis.git

clone_if_missing entservices-testframework --branch 2.0.0 https://github.com/rdkcentral/entservices-testframework.git

############################
# Build Thunder-Tools
echo "======================================================================================"
echo "buliding thunderTools"
cd ThunderTools
cd -


cmake -G Ninja -S ThunderTools -B build/ThunderTools \
    -DEXCEPTIONS_ENABLE=ON \
    -DPYTHON_EXECUTABLE="${BUILD_PYTHON_BIN}" \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr"

cmake --build build/ThunderTools --target install
log_info "Dependency build completed: ThunderTools."


############################
# Build Thunder
echo "======================================================================================"
echo "buliding thunder"

cd Thunder
cd -

cmake -G Ninja -S Thunder -B build/Thunder \
    -DMESSAGING=ON \
    -DPYTHON_EXECUTABLE="${BUILD_PYTHON_BIN}" \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr" \
    -DBUILD_TYPE=Debug \
    -DBINDING=127.0.0.1 \
    -DPORT=55555 \
    -DEXCEPTIONS_ENABLE=ON

cmake --build build/Thunder --target install
log_info "Dependency build completed: Thunder."

############################
# Build entservices-apis
echo "======================================================================================"
echo "buliding entservices-apis"
cd entservices-apis
rm -rf jsonrpc/DTV.json
cd ..

cmake -G Ninja -S entservices-apis  -B build/entservices-apis \
    -DEXCEPTIONS_ENABLE=ON \
    -DPYTHON_EXECUTABLE="${BUILD_PYTHON_BIN}" \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr"

cmake --build build/entservices-apis --target install
log_info "Dependency build completed: entservices-apis."


#############################
# Build Thunder-clientlibraries

cmake -G Ninja -S ThunderClientLibraries -B build/ThunderClientLibraries \
    -DPYTHON_EXECUTABLE="${BUILD_PYTHON_BIN}" \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr"

cmake --build build/ThunderClientLibraries
log_info "Dependency build completed: ThunderClientLibraries."

#############################
# Build googletest

clone_if_missing googletest --branch v1.15.2 https://github.com/google/googletest.git

cmake -G Ninja -S googletest -B build/googletest \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DBUILD_TYPE=Debug \
    -DBUILD_GMOCK=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON

cmake --build build/googletest --target install
log_info "Dependency build completed: googletest."

ls -la ${GITHUB_WORKSPACE}
log_info "BUILD_DEPENDENCIES_COMPLETED: All build dependencies completed successfully."
