# OpenCDMI L1 Tests

This directory contains the OpenCDMI client and plugin Level 1 tests. The tests are built with the repository-local CMake targets and executed with GoogleTest.

## Supported environment

The dependency bootstrap and L1 runner scripts support Ubuntu only.
For any non-Ubuntu platform, they exit with:
`ERROR: Platform not supported. This script supports Ubuntu only.`

Run the commands below from the repository root on an Ubuntu build machine.

Required tools include:

- Bash
- Git
- CMake and Ninja
- Python 3
- A working C++ compiler
- Permission to install Ubuntu packages with `sudo`

## Build dependencies

The first build, or a build on a clean machine, should bootstrap the Thunder and test dependencies:

```bash
cd /path/to/entservices-opencdmi
sudo ./build_dependencies.sh
```

The script installs system packages and builds the required dependencies under the repository `install` directory. It also downloads the required Thunder, ThunderTools, ThunderClientLibraries, entservices-apis, entservices-testframework, GoogleTest, and trower-base64 sources when they are not already present.

If the script was run as root and the generated files are not readable by the normal user, repair ownership from the repository root:

```bash
sudo chown -R "$USER:$USER" \
  install build ThunderTools Thunder ThunderClientLibraries \
  entservices-apis entservices-testframework googletest \
  trower-base64 .build-deps-venv
```

## Build and run all L1 tests

```bash
cd /path/to/entservices-opencdmi
./Tests/L1Tests/run.sh
```

The runner performs these steps:

1. Locates the dependency CMake package files.
2. Configures the project in `Tests/L1Tests/build`.
3. Builds and installs the test executable into `Tests/L1Tests/install`.
4. Runs the discovered L1 test executable.

Test results are written to per-runner JSON files under:

- Tests/L1Tests/build/OpenCDMIL1Tests.json
- Tests/L1Tests/build/OpenCDMIClientCoreL1Tests.json
- Tests/L1Tests/build/OpenCDMIAdapterRdkL1Tests.json

The runner sets `PLUGIN_OPENCDMI=OFF` for this L1 build because the tests compile the required OpenCDMI source files and mocks directly. The production OpenCDMI client/plugin shared libraries are not required to run these tests.

## Runner options

Build without executing tests:

```bash
./Tests/L1Tests/run.sh -n
```

Enable coverage compiler flags:

```bash
./Tests/L1Tests/run.sh -c
```

Run a GoogleTest filter:

```bash
./Tests/L1Tests/run.sh -f '*CapsParser*'
```

Set the test timeout in seconds:

```bash
./Tests/L1Tests/run.sh -t 120
```

Let the runner bootstrap missing dependencies automatically:

```bash
./Tests/L1Tests/run.sh -d
```

The `-d` option may require `sudo` because `build_dependencies.sh` installs Ubuntu packages. Running the bootstrap explicitly first gives clearer control over package installation.

## External dependency installation

If dependencies are installed outside the repository, set `EXT_INSTALL_ROOT` to the installation prefix containing the CMake package files:

```bash
EXT_INSTALL_ROOT=/path/to/install ./Tests/L1Tests/run.sh
```

The runner searches these locations by default:

```text
<repository>/install/usr/lib/cmake
<repository>/install/lib/cmake
/usr/local/lib/cmake
/usr/lib/cmake
```

It expects to find at least `WPEFrameworkConfig.cmake` and the WPEFramework plugins package configuration below one of those roots.

## Troubleshooting

### Missing CMake package configs

If the runner reports missing `WPEFrameworkConfig.cmake` or `WPEFrameworkPluginsConfig.cmake`, build the dependencies first:

```bash
sudo ./build_dependencies.sh
./Tests/L1Tests/run.sh
```

For a nonstandard install location, set `EXT_INSTALL_ROOT` as shown above.

### apt lock error during dependency bootstrap

If you see errors like "Could not get lock /var/lib/apt/lists/lock", another
package process (for example `aptd` or unattended updates) is running.

`build_dependencies.sh` now waits for apt locks to clear and retries apt
commands automatically. If the lock is held for too long, rerun later or stop
the other package operation first.

### CMake reports `CDMI_ADAPTER_IMPLEMENTATION` as unused

The L1 runner intentionally disables the production OpenCDMI subdirectories and removes the adapter setting before configuring. This warning is harmless for the L1-only build; the adapter implementation is not compiled by these tests.

### Running on non-Ubuntu platforms

Non-Ubuntu platforms are not supported by these scripts.
Use an Ubuntu environment such as a native machine, VM, container,
or CI runner for the documented flow.
