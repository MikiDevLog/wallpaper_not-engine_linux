#ifndef WAYLAND_PROTOCOLS_H
#define WAYLAND_PROTOCOLS_H

#ifdef __cplusplus
extern "C" {
#endif

// Workaround for C++ namespace keyword conflict
#ifdef __cplusplus
#define namespace ns
#endif

#include "../protocols/wlr-layer-shell-unstable-v1.h"
#include "../protocols/xdg-output-unstable-v1.h"

#ifdef __cplusplus
#undef namespace
#endif

#ifdef __cplusplus
}
#endif

#endif // WAYLAND_PROTOCOLS_H
