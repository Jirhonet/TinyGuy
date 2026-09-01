#include "services/cursor_usage.h"

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
static cursor_usage_snapshot_t snapshot = {
    .status = CURSOR_USAGE_CONNECTING,
    .cursor_models_used_percent = 0,
    .other_models_used_percent = 0,
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

static bool read_cursor_login(char *user_id, size_t user_capacity,
                              char *token, size_t token_capacity)
{
    const char *config = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    char database[1024];
    if (config != NULL && *config != '\0')
        snprintf(database, sizeof(database), "%s/Cursor/User/globalStorage/state.vscdb", config);
    else if (home != NULL && *home != '\0')
        snprintf(database, sizeof(database), "%s/.config/Cursor/User/globalStorage/state.vscdb", home);
    else
        return false;

    char *user_argv[] = {
        "sqlite3", database,
        "select value from ItemTable where key='cursorAuth/stripeMembershipAuthId';",
        NULL
    };
    char *token_argv[] = {
        "sqlite3", database,
        "select value from ItemTable where key='cursorAuth/accessToken';",
        NULL
    };
    if (!capture_command(user_argv, NULL, user_id, user_capacity) ||
        !capture_command(token_argv, NULL, token, token_capacity))
        return false;

    user_id[strcspn(user_id, "\r\n")] = '\0';
    token[strcspn(token, "\r\n")] = '\0';
    return user_id[0] != '\0' && token[0] != '\0';
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

static bool fetch_usage(cursor_usage_snapshot_t *result)
{
    char user_id[256];
    char token[2048];
    if (!read_cursor_login(user_id, sizeof(user_id), token, sizeof(token)))
        return false;

    char curl_config[3072];
    int config_length = snprintf(curl_config, sizeof(curl_config),
        "header = \"Accept: application/json\"\n"
        "header = \"Cookie: WorkosCursorSessionToken=%s::%s\"\n",
        user_id, token);
    if (config_length < 0 || (size_t)config_length >= sizeof(curl_config))
        return false;

    char response[32768];
    char *argv[] = {
        "curl", "--fail", "--silent", "--show-error", "--max-time", "10",
        "--config", "-", "https://cursor.com/api/usage-summary", NULL
    };
    bool fetched = capture_command(argv, curl_config, response, sizeof(response));
    memset(token, 0, sizeof(token));
    memset(curl_config, 0, sizeof(curl_config));
    if (!fetched) return false;

    const char *individual = after_key(response, "\"individualUsage\"");
    const char *plan = individual == NULL ? NULL : after_key(individual, "\"plan\"");
    const char *cursor_models = plan == NULL ? NULL :
        after_key(plan, "\"autoPercentUsed\"");
    const char *other_models = plan == NULL ? NULL :
        after_key(plan, "\"apiPercentUsed\"");
    if (cursor_models == NULL || other_models == NULL) return false;

    char *end = NULL;
    double cursor_value = strtod(cursor_models, &end);
    if (end == cursor_models || cursor_value < 0.0) return false;
    double other_value = strtod(other_models, &end);
    if (end == other_models || other_value < 0.0) return false;
    if (cursor_value > 100.0) cursor_value = 100.0;
    if (other_value > 100.0) other_value = 100.0;
    result->cursor_models_used_percent = cursor_value;
    result->other_models_used_percent = other_value;
    result->status = CURSOR_USAGE_READY;
    return true;
}

static void *worker_main(void *unused)
{
    (void)unused;
    for (;;) {
        cursor_usage_snapshot_t next = { .status = CURSOR_USAGE_UNAVAILABLE };
        fetch_usage(&next);
        pthread_mutex_lock(&snapshot_lock);
        snapshot = next;
        pthread_mutex_unlock(&snapshot_lock);
        sleep(next.status == CURSOR_USAGE_READY ? 60 : 15);
    }
    return NULL;
}

static void launch_worker(void)
{
    pthread_t worker;
    if (pthread_create(&worker, NULL, worker_main, NULL) == 0)
        pthread_detach(worker);
    else
        snapshot.status = CURSOR_USAGE_UNAVAILABLE;
}

void cursor_usage_start(void)
{
    pthread_once(&worker_once, launch_worker);
}

cursor_usage_snapshot_t cursor_usage_get(void)
{
    pthread_mutex_lock(&snapshot_lock);
    cursor_usage_snapshot_t result = snapshot;
    pthread_mutex_unlock(&snapshot_lock);
    return result;
}
