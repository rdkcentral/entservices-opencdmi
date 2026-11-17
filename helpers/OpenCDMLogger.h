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

OPENCDM_LOGGER_API void Initialize();
OPENCDM_LOGGER_API void Write(const int level, const char* format, ...);

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

#define TRACE_L1(...) ::OpenCDM::Logging::Write(1, __VA_ARGS__)
#define TRACE_L2(...) ::OpenCDM::Logging::Write(2, __VA_ARGS__)
#define TRACE_L3(...) ::OpenCDM::Logging::Write(3, __VA_ARGS__)

#endif // OPENCDM_LOGGER_H