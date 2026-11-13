/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

// Times the enclosing scope with RDKPerf when built with ENABLE_OCDM_RDKPERF.
// Always expands to a named object so the timer lives until the end of the
// scope (an unnamed RDKPerf temporary would stop immediately).
#ifdef ENABLE_OCDM_RDKPERF
#include <rdk_perf.h>
#define OCDM_PERF_SCOPE(name) RDKPerf ocdmPerfScope(name)
#else
#define OCDM_PERF_SCOPE(name) do {} while (0)
#endif
