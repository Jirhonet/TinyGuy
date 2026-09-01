#include "services/claude_usage.h"

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static pthread_mutex_t snapshot_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t worker_once = PTHREAD_ONCE_INIT;
static claude_usage_snapshot_t snapshot = {
    .status = CLAUDE_USAGE_CONNECTING,
    .five_hour_used_percent = 0,
    .week_used_percent = 0,
};

static bool capture_command(char *const argv[], const char *input,
                            char *output, size_t capacity)
{
    int input_pipe[2];
    int output_pipe[2];
    if (pipe(input_pipe) != 0) return false;
    if (pipe(output_pipe) != 0) {
        close(input_pipe[0]); close(input_pipe[1]);
        return false;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, input_pipe[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, output_pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, input_pipe[1]);
    posix_spawn_file_actions_addclose(&actions, output_pipe[0]);

    pid_t child = 0;
    int error = posix_spawnp(&child, argv[0], &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(input_pipe[0]);
    close(output_pipe[1]);
    if (error != 0) {
        close(input_pipe[1]); close(output_pipe[0]);
        return false;
    }

    if (input != NULL) {
        size_t length = strlen(input);
        size_t sent = 0;
        while (sent < length) {
            ssize_t count = write(input_pipe[1], input + sent, length - sent);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) break;
            sent += (size_t)count;
        }
    }
    close(input_pipe[1]);

    size_t length = 0;
    while (length + 1 < capacity) {
        struct pollfd descriptor = { .fd = output_pipe[0], .events = POLLIN };
        int ready = poll(&descriptor, 1, 10000);
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) {
            kill(child, SIGTERM);
            break;
        }
        ssize_t count = read(output_pipe[0], output + length,
                             capacity - length - 1);
        if (count <= 0) break;
        length += (size_t)count;
    }
    output[length] = '\0';
    close(output_pipe[0]);

    int status = 0;
    waitpid(child, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 && length > 0;
}

static const char *after_key(const char *json, const char *key)
{
    const char *position = strstr(json, key);
    if (position == NULL) return NULL;
    position = strchr(position + strlen(key), ':');
    if (position == NULL) return NULL;
    do { position++; } while (*position == ' ' || *position == '\t');
    return position;
}

static bool read_json_string(const char *json, const char *key,
                             char *value, size_t capacity)
{
    const char *source = after_key(json, key);
    if (source == NULL || *source != '"' || capacity == 0) return false;
    source++;
    size_t length = 0;
    while (*source != '\0' && *source != '"') {
        char character = *source++;
        if (character == '\\') {
            character = *source++;
            if (character == '\0') return false;
        }
        if (length + 1 >= capacity) return false;
        value[length++] = character;
    }
    if (*source != '"') return false;
    value[length] = '\0';
    return length > 0;
}

static bool read_claude_token(char *token, size_t capacity)
{
    const char *home = getenv("HOME");
    if (home == NULL || *home == '\0') return false;

    char path[1024];
    int path_length = snprintf(path, sizeof(path),
                               "%s/.claude/.credentials.json", home);
    if (path_length < 0 || (size_t)path_length >= sizeof(path)) return false;

    FILE *file = fopen(path, "r");
    if (file == NULL) return false;
    char credentials[16384];
    size_t length = fread(credentials, 1, sizeof(credentials) - 1, file);
    bool complete = feof(file) != 0;
    fclose(file);
    if (!complete) return false;
    credentials[length] = '\0';

    bool found = read_json_string(credentials, "\"accessToken\"",
                                  token, capacity);
    memset(credentials, 0, sizeof(credentials));
    return found;
}

static bool parse_limit(const char *json, const char *key, double *used)
{
    const char *window = after_key(json, key);
    if (window == NULL || strncmp(window, "null", 4) == 0 || *window != '{')
        return false;
    const char *utilization = after_key(window, "\"utilization\"");
    if (utilization == NULL) return false;
    char *end = NULL;
    double value = strtod(utilization, &end);
    if (end == utilization || value < 0.0 || value > 100.0) return false;
    *used = value;
    return true;
}

static bool fetch_usage(claude_usage_snapshot_t *result)
{
    char token[4096];
    if (!read_claude_token(token, sizeof(token))) return false;

    char curl_config[4608];
    int config_length = snprintf(curl_config, sizeof(curl_config),
        "header = \"Accept: application/json\"\n"
        "header = \"Authorization: Bearer %s\"\n"
        "header = \"anthropic-beta: oauth-2025-04-20\"\n", token);
    if (config_length < 0 || (size_t)config_length >= sizeof(curl_config)) {
        memset(token, 0, sizeof(token));
        return false;
    }

    char response[32768];
    char *argv[] = {
        "curl", "--fail", "--silent", "--show-error", "--max-time", "10",
        "--config", "-", "https://api.anthropic.com/api/oauth/usage", NULL
    };
    bool fetched = capture_command(argv, curl_config, response, sizeof(response));
    memset(token, 0, sizeof(token));
    memset(curl_config, 0, sizeof(curl_config));
    if (!fetched) return false;

    double five_hour = 0.0;
    double seven_day = 0.0;
    bool has_five_hour = parse_limit(response, "\"five_hour\"", &five_hour);
    bool has_seven_day = parse_limit(response, "\"seven_day\"", &seven_day);
    if (!has_five_hour || !has_seven_day) return false;

    result->five_hour_used_percent = five_hour;
    result->week_used_percent = seven_day;
    result->status = CLAUDE_USAGE_READY;
    return true;
}

static void *worker_main(void *unused)
{
    (void)unused;
    for (;;) {
        claude_usage_snapshot_t next = { .status = CLAUDE_USAGE_UNAVAILABLE };
        fetch_usage(&next);
        pthread_mutex_lock(&snapshot_lock);
        snapshot = next;
        pthread_mutex_unlock(&snapshot_lock);
        sleep(next.status == CLAUDE_USAGE_READY ? 60 : 15);
    }
    return NULL;
}

static void launch_worker(void)
{
    pthread_t worker;
    if (pthread_create(&worker, NULL, worker_main, NULL) == 0)
        pthread_detach(worker);
    else
        snapshot.status = CLAUDE_USAGE_UNAVAILABLE;
}

void claude_usage_start(void)
{
    pthread_once(&worker_once, launch_worker);
}

claude_usage_snapshot_t claude_usage_get(void)
{
    pthread_mutex_lock(&snapshot_lock);
    claude_usage_snapshot_t result = snapshot;
    pthread_mutex_unlock(&snapshot_lock);
    return result;
}
