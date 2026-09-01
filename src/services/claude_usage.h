#ifndef TINY_GUY_CLAUDE_USAGE_H
#define TINY_GUY_CLAUDE_USAGE_H

typedef enum {
    CLAUDE_USAGE_CONNECTING,
    CLAUDE_USAGE_READY,
    CLAUDE_USAGE_UNAVAILABLE
} claude_usage_status_t;

typedef struct {
    claude_usage_status_t status;
    double five_hour_used_percent;
    double week_used_percent;
} claude_usage_snapshot_t;

/* Reuses the OAuth login stored by the local Claude Code CLI. Credentials are
 * read only for the request and are never persisted by Tiny Guy. */
void claude_usage_start(void);
claude_usage_snapshot_t claude_usage_get(void);

#endif
