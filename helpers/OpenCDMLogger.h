#ifndef OPENCDM_LOGGER_H
#define OPENCDM_LOGGER_H

#if defined(_WIN32) && defined(OPENCDM_LOGGER_BUILD)
#define OPENCDM_LOGGER_API __declspec(dllexport)
#elif defined(_WIN32)
#define OPENCDM_LOGGER_API __declspec(dllimport)
#else
#define OPENCDM_LOGGER_API __attribute__((visibility("default")))
#endif

namespace OpenCDM {
namespace Logging {

enum LogLevel {
    ERROR = 1,
    WARNING = 2,
    DEBUG = 3,
    INFO = 4
};

OPENCDM_LOGGER_API void Initialize();
OPENCDM_LOGGER_API void Write(const int level, const char* format, ...);

/**
 * @brief Set log level at runtime via configuration
 * Allows enabling debug/info logs without restart
 * @param level The log level (ERROR=1, WARNING=2, DEBUG=3, INFO=4)
 */
OPENCDM_LOGGER_API void SetLogLevel(const int level);

/**
 * @brief Get current log level
 * @return int Current log level
 */
OPENCDM_LOGGER_API int GetLogLevel();

} // namespace Logging
} // namespace OpenCDM

#ifdef TRACE_L1
#undef TRACE_L1
#endif
#ifdef TRACE_L2
#undef TRACE_L2
#endif
#ifdef TRACE_L3
#undef TRACE_L3
#endif

// Log levels: L1=ERROR, L2=WARNING, L3=DEBUG, L4=INFO
#define TRACE_L1(...) ::OpenCDM::Logging::Write(::OpenCDM::Logging::ERROR, __VA_ARGS__)
#define TRACE_L2(...) ::OpenCDM::Logging::Write(::OpenCDM::Logging::WARNING, __VA_ARGS__)
#define TRACE_L3(...) ::OpenCDM::Logging::Write(::OpenCDM::Logging::DEBUG, __VA_ARGS__)
#define TRACE_L4(...) ::OpenCDM::Logging::Write(::OpenCDM::Logging::INFO, __VA_ARGS__)

#endif // OPENCDM_LOGGER_H
