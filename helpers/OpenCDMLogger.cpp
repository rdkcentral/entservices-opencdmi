#define OPENCDM_LOGGER_BUILD
#include "OpenCDMLogger.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <cstdarg>

namespace OpenCDM {
namespace Logging {
namespace {

const char kConfigPath[] = "/opt/rdk_adapter_logging.conf";
const int kDefaultLevel = 1;
std::once_flag gInitialization;
std::mutex gOutputMutex;
int gLogLevel = kDefaultLevel;

void LoadConfiguration()
{
    FILE* config = std::fopen(kConfigPath, "r");
    if (config == nullptr) {
        return;
    }

    char line[128];
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

        if ((std::strcmp(value, "ERROR") == 0) || (std::strcmp(value, "error") == 0)) {
            gLogLevel = 1;
            break;
        }
        if ((std::strcmp(value, "WARNING") == 0) || (std::strcmp(value, "warning") == 0)) {
            gLogLevel = 2;
            break;
        }
        if ((std::strcmp(value, "INFO") == 0) || (std::strcmp(value, "info") == 0)) {
            gLogLevel = 3;
            break;
        }
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

    std::lock_guard<std::mutex> lock(gOutputMutex);
    std::fprintf(stderr, "[OpenCDM][L%d] %s\n", level, message);
}

} // namespace Logging
} // namespace OpenCDM