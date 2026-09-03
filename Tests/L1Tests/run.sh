#!/bin/bash -e
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
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
#
# Build and run OpenCDMI L1 tests using repository-local CMake targets.
# This follows the same high-level flow as AAMP L1 run.sh:
#   1) configure
#   2) build
#   3) execute tests
#   4) return non-zero on failure
#
# Options:
#   -c              enable coverage-oriented compiler flags
#   -t <seconds>    per-run timeout for test execution (default: 60)
#   -f <filter>     gtest filter passed to each L1 test runner
#   -n              configure/build only, do not execute tests
#   -d              bootstrap dependencies using repo build_dependencies.sh

if [[ "$(uname -s)" != "Linux" || ! -f /etc/os-release || \
      "$(. /etc/os-release && echo "${ID}")" != "ubuntu" ]]; then
    echo "ERROR: Platform not supported. This script supports Ubuntu only."
    exit 1
fi

if [[ -z "${MAKEFLAGS}" ]]; then
    export MAKEFLAGS=-j$(nproc)
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(realpath "${SCRIPT_DIR}/../..")"
BUILD_DIR="${SCRIPT_DIR}/build"
INSTALL_DIR="${SCRIPT_DIR}/install"

# Optional external install root (for Thunder/WPE packages built outside repo).
# If unset, try common install roots used by local dependency builds.
EXT_INSTALL_ROOT="${EXT_INSTALL_ROOT:-}"

ENABLE_COVERAGE=0
RUN_TIMEOUT=${CTEST_TIMEOUT:-60}
GTEST_FILTER=""
BUILD_ONLY=0
BOOTSTRAP_DEPS=0

while getopts "ct:f:nd" opt; do
    case ${opt} in
        c)
            ENABLE_COVERAGE=1
            ;;
        t)
            RUN_TIMEOUT=${OPTARG}
            ;;
        f)
            GTEST_FILTER=${OPTARG}
            ;;
        n)
            BUILD_ONLY=1
            ;;
        d)
            BOOTSTRAP_DEPS=1
            ;;
        *)
            ;;
    esac
done

mkdir -p "${BUILD_DIR}" "${INSTALL_DIR}"

# Resolve dependency root when EXT_INSTALL_ROOT is not set explicitly.
if [[ -z "${EXT_INSTALL_ROOT}" ]]; then
    _auto_candidates=(
        "${REPO_ROOT}/install"
        "${SCRIPT_DIR}/install"
        "/usr/local"
        "/usr"
    )

    for _root in "${_auto_candidates[@]}"; do
        if [[ -f "${_root}/usr/lib/cmake/WPEFramework/WPEFrameworkConfig.cmake" || \
              -f "${_root}/lib/cmake/WPEFramework/WPEFrameworkConfig.cmake" ]]; then
            EXT_INSTALL_ROOT="${_root}"
            break
        fi
    done

    if [[ -z "${EXT_INSTALL_ROOT}" ]]; then
        EXT_INSTALL_ROOT="${REPO_ROOT}/install"
    fi
fi

# Match CI layout: package configs are typically installed under
# <root>/usr/lib/cmake and helper cmake modules under <root>/tools/cmake.
_cmake_prefix_path="${EXT_INSTALL_ROOT};${EXT_INSTALL_ROOT}/usr"
_cmake_module_path="${EXT_INSTALL_ROOT}/tools/cmake;${EXT_INSTALL_ROOT}/usr/lib/cmake"

_cmake_config_roots=(
    "${EXT_INSTALL_ROOT}/usr/lib/cmake"
    "${EXT_INSTALL_ROOT}/lib/cmake"
)

find_cmake_package_config()
{
    local package_name="$1"
    local root=""
    local candidate=""

    for root in "${_cmake_config_roots[@]}"; do
        candidate="${root}/${package_name}/${package_name}Config.cmake"
        if [[ -f "${candidate}" ]]; then
            echo "${candidate}"
            return 0
        fi
    done

    return 1
}

is_unreadable_install_root()
{
    local root=""

    for root in "${_cmake_config_roots[@]}"; do
        if [[ -e "${root}" && ! -r "${root}" ]]; then
            return 0
        fi
    done

    return 1
}

find_required_configs()
{
    _wpe_cfg="$(find_cmake_package_config "WPEFramework" || true)"
    _plugins_cfg="$(find_cmake_package_config "WPEFrameworkPlugins" || \
        find_cmake_package_config "Plugins" || true)"
}

# Quick dependency probes so users get actionable guidance instead of generic
# CMake parsing errors (e.g. missing NAMESPACE causing string() errors).
find_required_configs

if [[ -z "${_wpe_cfg}" || -z "${_plugins_cfg}" ]] && [[ "${BOOTSTRAP_DEPS}" -eq 1 ]]; then
    if [[ -x "${REPO_ROOT}/build_dependencies.sh" ]]; then
        echo "Dependency configs missing under ${EXT_INSTALL_ROOT}."
        echo "Attempting bootstrap via ${REPO_ROOT}/build_dependencies.sh ..."
        if ! (
            cd "${REPO_ROOT}"
            ./build_dependencies.sh
        ); then
            echo "ERROR: dependency bootstrap failed."
            echo "On Ubuntu this is commonly due to sudo/package-install permissions. Retry with:"
            echo "  cd ${REPO_ROOT}"
            echo "  sudo ./build_dependencies.sh"
            exit 1
        fi

        # Re-probe after dependency bootstrap.
        find_required_configs
    else
        echo "WARNING: -d specified but build_dependencies.sh not found at ${REPO_ROOT}."
    fi
fi

if [[ -z "${_wpe_cfg}" || -z "${_plugins_cfg}" ]]; then
    if is_unreadable_install_root; then
        echo "ERROR: Dependency install tree exists but is not readable by user $(id -un)."
        echo "This commonly happens when build_dependencies.sh was run with sudo."
        echo "Repair ownership from repo root with:"
        echo "  sudo chown -R \"$USER:$USER\" install build ThunderTools Thunder ThunderClientLibraries entservices-apis entservices-testframework trower-base64 .build-deps-venv"
        exit 1
    fi

    echo "ERROR: Missing required CMake package configs for L1 build."
    echo "Expected WPEFrameworkConfig.cmake and WPEFrameworkPluginsConfig.cmake under:"
    echo "  ${EXT_INSTALL_ROOT}/usr/lib/cmake or ${EXT_INSTALL_ROOT}/lib/cmake"
    echo "Auto-detection checks: ${REPO_ROOT}/install, ${SCRIPT_DIR}/install, /usr/local, /usr"
    echo "Set EXT_INSTALL_ROOT to your installed toolchain root, e.g.:"
    echo "  EXT_INSTALL_ROOT=\"/path/to/install\" ./run.sh"
    echo "If dependencies are not built yet, run from repo root:"
    echo "  sudo ./build_dependencies.sh"
    echo "Or let this script trigger it for you:"
    echo "  ./run.sh -d"
    echo ""
    echo "Note: build_dependencies.sh installs system packages and Python"
    echo "dependencies itself, and usually needs sudo privileges on Ubuntu."
    echo ""
    echo "This avoids CMake errors such as:"
    echo "  string no output variable specified"
    echo "  include could not find requested file: CmakeHelperFunctions"
    echo "  Could not find package: WPEFrameworkPlugins"
    exit 1
fi

# USE_THUNDER_R4 comes from the Plugins imported target; redefining it here warns.
EXTRA_CXX_FLAGS="-DEXCEPTIONS_ENABLE=ON -DRDK_SERVICES_L1_TEST"
if [[ "${ENABLE_COVERAGE}" -eq 1 ]]; then
    EXTRA_CXX_FLAGS="${EXTRA_CXX_FLAGS} -fprofile-arcs -ftest-coverage --coverage"
fi

cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -DCMAKE_PREFIX_PATH="${_cmake_prefix_path}" \
    -DCMAKE_MODULE_PATH="${_cmake_module_path}" \
    -DRDK_SERVICES_L1_TEST=ON \
    -DPLUGIN_OPENCDMI=OFF \
    -UCDMI_ADAPTER_IMPLEMENTATION \
    -DCDMI_ADAPTER_IMPLEMENTATION:STRING=gstreamer \
    -DCOMCAST_CONFIG=OFF \
    -DCMAKE_CXX_FLAGS="${EXTRA_CXX_FLAGS}"

cmake --build "${BUILD_DIR}"
cmake --install "${BUILD_DIR}"

if [[ "${BUILD_ONLY}" -eq 1 ]]; then
    echo "Build completed. Skipping test execution (-n)."
    exit 0
fi

export PATH="${INSTALL_DIR}/bin:${PATH}"
export LD_LIBRARY_PATH="${INSTALL_DIR}/lib:${INSTALL_DIR}/lib/wpeframework/plugins:${EXT_INSTALL_ROOT}/usr/lib:${EXT_INSTALL_ROOT}/usr/lib/wpeframework/plugins:${EXT_INSTALL_ROOT}/lib:${EXT_INSTALL_ROOT}/lib/wpeframework/plugins:${LD_LIBRARY_PATH}"

RUNNER_NAMES=(
    OpenCDMIL1Tests
    OpenCDMIClientCoreL1Tests
    OpenCDMIAdapterRdkL1Tests
)

overall_status=0
for runner_name in "${RUNNER_NAMES[@]}"; do
    runner="${INSTALL_DIR}/bin/${runner_name}"
    if [[ ! -x "${runner}" ]]; then
        echo "ERROR: Expected L1 test runner not found: ${runner}"
        overall_status=1
        continue
    fi

    results_json="${BUILD_DIR}/${runner_name}.json"
    rm -f "${results_json}"
    runner_command=("${runner}")
    if [[ -n "${GTEST_FILTER}" ]]; then
        runner_command+=("--gtest_filter=${GTEST_FILTER}")
    fi

    if command -v timeout >/dev/null 2>&1; then
        if ! GTEST_OUTPUT="json:${results_json}" \
            timeout "${RUN_TIMEOUT}" "${runner_command[@]}"; then
            overall_status=1
        fi
    else
        if ! GTEST_OUTPUT="json:${results_json}" "${runner_command[@]}"; then
            overall_status=1
        fi
    fi

    if [[ -f "${results_json}" ]]; then
        echo "L1 test results: ${results_json}"
    else
        echo "ERROR: L1 test runner did not produce results: ${runner_name}"
        overall_status=1
    fi
done

python3 - "${BUILD_DIR}" "${RUNNER_NAMES[@]}" <<'PY'
import json
import pathlib
import sys

build_dir = pathlib.Path(sys.argv[1])
runner_names = sys.argv[2:]
totals = {"tests": 0, "passed": 0, "failed": 0, "disabled": 0}

print("\nL1 TEST SUMMARY")
print(f"{'Runner':<32} {'Total':>7} {'Passed':>7} {'Failed':>7} "
    f"{'Disabled':>9}  Status")
print("-" * 82)

for runner_name in runner_names:
    result_path = build_dir / f"{runner_name}.json"
    if not result_path.is_file():
      print(f"{runner_name:<32} {'-':>7} {'-':>7} {'-':>7} "
          f"{'-':>9}  NO RESULTS")
      continue

    with result_path.open(encoding="utf-8") as result_file:
      result = json.load(result_file)

    tests = int(result.get("tests", 0))
    failures = int(result.get("failures", 0))
    errors = int(result.get("errors", 0))
    disabled = int(result.get("disabled", 0))
    failed = failures + errors
    passed = tests - failed - disabled
    status = "PASSED" if failed == 0 else "FAILED"

    totals["tests"] += tests
    totals["passed"] += passed
    totals["failed"] += failed
    totals["disabled"] += disabled
    print(f"{runner_name:<32} {tests:>7} {passed:>7} {failed:>7} "
        f"{disabled:>9}  {status}")

print("-" * 82)
print(f"{'TOTAL':<32} {totals['tests']:>7} {totals['passed']:>7} "
    f"{totals['failed']:>7} {totals['disabled']:>9}")
PY

exit "${overall_status}"
