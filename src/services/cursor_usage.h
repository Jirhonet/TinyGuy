#ifndef TINY_GUY_CURSOR_USAGE_H
#define TINY_GUY_CURSOR_USAGE_H

typedef enum {
    CURSOR_USAGE_CONNECTING,
    CURSOR_USAGE_READY,
    CURSOR_USAGE_UNAVAILABLE
} cursor_usage_status_t;

typedef struct {
    cursor_usage_status_t status;
    double cursor_models_used_percent;
    double other_models_used_percent;
} cursor_usage_snapshot_t;

/* Reuses the login stored by the local Cursor desktop app. Credentials are
 * read only for the request and are never persisted by Tiny Guy. */
void cursor_usage_start(void);
cursor_usage_snapshot_t cursor_usage_get(void);

#endif
