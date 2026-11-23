#define OPENCDM_LOGGER_BUILD
#include "OpenCDMLogger.h"

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <cstdarg>

namespace OpenCDM {
namespace Logging {
namespace {

const char kConfigPath[] = "/opt/rdk_adapter_logging.conf";
const int kDefaultLevel = Logging::WARNING;  // Default: WARNING (L2) and ERROR (L1) only
std::once_flag gInitialization;
std::mutex gOutputMutex;
std::atomic<int> gLogLevel{kDefaultLevel};

void LoadConfiguration()
{
    FILE* config = std::fopen(kConfigPath, "r");
    if (config == nullptr) {
        // Log to stderr directly (before initialization) that config file not found
        std::fprintf(stderr, "[OpenCDM][warn] Config file not found at %s, using default level (WARNING)\n", kConfigPath);
        std::fflush(stderr);  // Ensure diagnostic log appears immediately
        return;
    }

    std::fprintf(stderr, "[OpenCDM][warn] Config file found at %s, loading configuration...\n", kConfigPath);
    std::fflush(stderr);  // Ensure diagnostic log appears immediately

    char line[128];
    bool configFound = false;
    while (std::fgets(line, sizeof(line), config) != nullptr) {
        char* value = line;
        while (std::isspace(static_cast<unsigned char>(*value)) != 0) {
            ++value;
        }

        if (std::strncmp(value, "level=", 6) == 0) {
            value += 6;
        }

        char* end = value + std::strlen(value);
        while ((end > value) && (std::isspace(static_cast<unsigned char>(end[-1])) != 0)) {
            *--end = '\0';
        }

        if ((std::strcmp(value, "ERROR") == 0) || (std::strcmp(value, "error") == 0) || (std::strcmp(value, "1") == 0)) {
            gLogLevel = Logging::ERROR;
            std::fprintf(stderr, "[OpenCDM][warn] Log level set to ERROR (1) from config file\n");
            std::fflush(stderr);  // Ensure diagnostic log appears immediately
            configFound = true;
            break;
        }
        if ((std::strcmp(value, "WARNING") == 0) || (std::strcmp(value, "warning") == 0) || (std::strcmp(value, "2") == 0)) {
            gLogLevel = Logging::WARNING;
            std::fprintf(stderr, "[OpenCDM][warn] Log level set to WARNING (2) from config file\n");
            std::fflush(stderr);  // Ensure diagnostic log appears immediately
            configFound = true;
            break;
        }
        if ((std::strcmp(value, "DEBUG") == 0) || (std::strcmp(value, "debug") == 0) || (std::strcmp(value, "3") == 0)) {
            gLogLevel = Logging::DEBUG;
            std::fprintf(stderr, "[OpenCDM][warn] Log level set to DEBUG (3) from config file\n");
            std::fflush(stderr);  // Ensure diagnostic log appears immediately
            configFound = true;
            break;
        }
        if ((std::strcmp(value, "INFO") == 0) || (std::strcmp(value, "info") == 0) || (std::strcmp(value, "4") == 0)) {
            gLogLevel = Logging::INFO;
            std::fprintf(stderr, "[OpenCDM][warn] Log level set to INFO (4) from config file\n");
            std::fflush(stderr);  // Ensure diagnostic log appears immediately
            configFound = true;
            break;
        }
    }

    if (!configFound) {
        std::fprintf(stderr, "[OpenCDM][warn] No valid log level found in config file, using default level (WARNING)\n");
        std::fflush(stderr);  // Ensure diagnostic log appears immediately
    }

    std::fclose(config);
}

} // namespace

void Initialize()
{
    std::call_once(gInitialization, LoadConfiguration);
}

void Write(const int level, const char* format, ...)
{
    Initialize();
    if ((format == nullptr) || (level > gLogLevel)) {
        return;
    }

    char message[1024];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    const char* levelStr = "unknown";
    switch (level) {
        case ERROR:
            levelStr = "err";
            break;
        case WARNING:
            levelStr = "warn";
            break;
        case DEBUG:
            levelStr = "debug";
            break;
        case INFO:
            levelStr = "info";
            break;
    }

    std::lock_guard<std::mutex> lock(gOutputMutex);
    std::fprintf(stderr, "[OpenCDM][%s] %s\n", levelStr, message);
}

void SetLogLevel(const int level)
{
    // Load the configuration first so it cannot overwrite this override later
    Initialize();
    if (level >= Logging::ERROR && level <= Logging::INFO) {
        gLogLevel = level;
        const char* levelStr = "unknown";
        switch (level) {
            case ERROR:
                levelStr = "err";
                break;
            case WARNING:
                levelStr = "warn";
                break;
            case DEBUG:
                levelStr = "debug";
                break;
            case INFO:
                levelStr = "info";
                break;
        }
        std::fprintf(stderr, "[OpenCDM][%s] Log level changed to %s\n", levelStr, levelStr);
    }
}

int GetLogLevel()
{
    Initialize();
    return gLogLevel;
}

} // namespace Logging
} // namespace OpenCDM
