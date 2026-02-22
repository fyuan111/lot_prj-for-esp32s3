#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compile-time launch marker.
 * CMake scans source code for `launch(priority, func)` and generates
 * ordered init calls for app_main.
 */
#define launch(priority, func)

#ifdef __cplusplus
}
#endif
