#ifndef TINY_GUY_CODEX_USAGE_H
#define TINY_GUY_CODEX_USAGE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CODEX_USAGE_CONNECTING,
    CODEX_USAGE_READY,
    CODEX_USAGE_UNAVAILABLE
} codex_usage_status_t;

typedef struct {
    codex_usage_status_t status;
    int five_hour_used_percent;
    int64_t five_hour_resets_at;
    int used_percent;
    int window_minutes;
    int64_t resets_at;
} codex_usage_snapshot_t;

/* Starts a small background poller. It reuses the login held by the local
 * `codex` CLI; no token is copied into Tiny Guy. */
void codex_usage_start(void);
codex_usage_snapshot_t codex_usage_get(void);

#endif
