#ifndef _GNU_SOURCE
#    define _GNU_SOURCE 1
#endif

#include <ctype.h>
#include <errno.h>
#include <iniparser.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define CONTINUOUS -1
#define ONCE 0

#define BUFF_SIZE 1024

#define ERROR(STR) fprintf(stderr, "\034[31merror:\033[0m " #STR);
#define ERROR_FMT(STR, ...) fprintf(stderr, "\034[31merror:\033[0m " #STR, __VA_ARGS__);

int LEMONBAR_PIPE_STDIN[2];
int LEMONBAR_PIPE_STDOUT[2];

const char* HOME() {
    const char* HOME = getenv("HOME");
    if (!HOME) HOME = getpwuid(getuid())->pw_dir;
    return HOME;
}

char* safe_strdup(const char* str) {
    return str ? strdup(str) : NULL;
}

void safe_free(void* ptr) {
    if (ptr) free(ptr);
}

const char*
iniparser_getsecstring(const dictionary* dic, const char* section, const char* key, char* def) {
    char* search;
    asprintf(&search, "%s:%s", section, key);
    const char* r = iniparser_getstring(dic, search, def);
    free(search);
    return r;
}

int iniparser_getsecboolean(const dictionary* dic, const char* section, const char* key, int def) {
    char* search;
    asprintf(&search, "%s:%s", section, key);
    int r = iniparser_getboolean(dic, search, def);
    free(search);
    return r;
}

enum BAR_POSITON { top, bottom };
enum DOCKING_MODE { normal, force };

typedef struct {
    char* delimiter;
    char* delimiter_color;
    char* font;
    char* underline_width;
    char* background_color;
    char* foreground_color;
    char* text_offset;
    size_t right_padding;
    size_t left_padding;
    enum BAR_POSITON bar_position;
    enum DOCKING_MODE docking_mode;
} Config;

void config_destroy(Config* config) {
    safe_free(config->delimiter);
    safe_free(config->delimiter_color);
    safe_free(config->font);
    safe_free(config->underline_width);
    safe_free(config->background_color);
    safe_free(config->foreground_color);
    safe_free(config->text_offset);
    free(config);
}

Config* config_load(dictionary* ini) {
    Config* config = malloc(sizeof(Config));

    config->delimiter = safe_strdup(iniparser_getstring(ini, "config:delimiter", " | "));
    config->delimiter_color =
        safe_strdup(iniparser_getstring(ini, "config:delimiter_color", "#FFFFFF"));
    config->font = safe_strdup(iniparser_getstring(ini, "config:font", NULL));
    config->underline_width = safe_strdup(iniparser_getstring(ini, "config:underline_width", "2"));
    config->background_color =
        safe_strdup(iniparser_getstring(ini, "config:background_color", NULL));
    config->foreground_color =
        safe_strdup(iniparser_getstring(ini, "config:foreground_color", NULL));
    config->text_offset = safe_strdup(iniparser_getstring(ini, "config:text_offset", NULL));
    config->right_padding = iniparser_getint(ini, "config:right_padding", 0);
    config->left_padding = iniparser_getint(ini, "config:left_padding", 0);

    const char* bar_position = iniparser_getstring(ini, "config:bar_position", "top");

    if (bar_position) {
        if (strstr(bar_position, "top") != NULL) {
            config->bar_position = top;
        } else if (strstr(bar_position, "bottom") != NULL) {
            config->bar_position = bottom;
        } else {
            ERROR("config: invalid field: config:bar_position\n"
                  "Can only be top or bottom\n")
            return NULL;
        }
    }

    const char* docking_mode = iniparser_getstring(ini, "config:docking_mode", "normal");

    if (docking_mode) {
        if (strstr(docking_mode, "normal") != NULL) {
            config->docking_mode = normal;
        } else if (strstr(docking_mode, "force") != NULL) {
            config->docking_mode = force;
        } else {
            ERROR("config: invalid field: config:docking_mode\n"
                  "Can only be normal or force\n");
            return NULL;
        }
    }

    return config;
}

typedef struct {
    pthread_mutex_t* lock;
    char* name;
    char* command;
    char* button_command;
    char* text;
    char* text_color;
    char* underline_color;
    int delay;
    int essential;
    size_t id;
} Block;

void block_destroy(Block* block) {
    pthread_mutex_destroy(block->lock);

    safe_free(block->name);
    safe_free(block->command);
    safe_free(block->button_command);
    safe_free(block->text);
    safe_free(block->text_color);
    safe_free(block->underline_color);

    free(block->lock);
    free(block);
}

typedef struct {
    Block** array;
    size_t n_blocks;
    size_t max_blocks;
} Block_Array;

Block_Array* block_array_make(size_t max) {
    Block_Array* block_array = malloc(sizeof(Block_Array));

    block_array->array = malloc(sizeof(Block*) * max);
    block_array->n_blocks = 0;
    block_array->max_blocks = max;

    return block_array;
}

void block_array_insert(Block_Array* blocks, Block* block) {
    if (blocks->n_blocks >= blocks->max_blocks) {
        blocks->array = realloc(blocks->array, sizeof(Block) * blocks->max_blocks * 2);
        blocks->max_blocks *= 2;
    }

    blocks->array[blocks->n_blocks] = block;
    blocks->n_blocks++;
}

void block_array_destroy(Block_Array* blocks) {
    for (size_t i = 0; i < blocks->n_blocks; i++) {
        block_destroy(blocks->array[i]);
    }
    free(blocks->array);
    free(blocks);
}

enum BAR_AREA { left, right, center };

typedef struct {
    int display_all;
    Block_Array* right;
    Block_Array* center;
    Block_Array* left;
    Config* config;
} Bar_State;

void bar_state_destroy(Bar_State* bs) {
    if (!bs) return;

    block_array_destroy(bs->left);
    block_array_destroy(bs->right);
    block_array_destroy(bs->center);
    config_destroy(bs->config);
    free(bs);
}

Block_Array* bar_state_get_block_array(Bar_State* bs, enum BAR_AREA area) {
    switch (area) {
        case left:
            return bs->left;
        case center:
            return bs->center;
        case right:
            return bs->right;
        default:
            return NULL;
    }
}

Block* bar_state_get_block(Bar_State* bs, size_t signal_id) {
    for (size_t i = 0; i < bs->right->n_blocks; i++) {
        if (bs->right->array[i]->id == signal_id) {
            return bs->right->array[i];
        }
    }

    for (size_t i = 0; i < bs->center->n_blocks; i++) {
        if (bs->center->array[i]->id == signal_id) {
            return bs->center->array[i];
        }
    }

    for (size_t i = 0; i < bs->left->n_blocks; i++) {
        if (bs->left->array[i]->id == signal_id) {
            return bs->left->array[i];
        }
    }

    return NULL;
}

int draw_side(
    char* buffer,
    Bar_State* bs,
    enum BAR_AREA position,
    char marker,
    int left_padding,
    int right_padding) {
    int size = sprintf(buffer, "%%{%c}", marker);

    for (int i = 0; i < left_padding; i++) {
        size += sprintf(buffer + size, " ");
    }

    Block_Array* blocks = bar_state_get_block_array(bs, position);

    int block_printed = 0;
    for (size_t i = 0; i < blocks->n_blocks; i++) {

        Block* block = blocks->array[i];

        pthread_mutex_lock(block->lock);

        if (!(bs->display_all || block->essential)) {
            pthread_mutex_unlock(block->lock);
            continue;
        }

        if (block->text && strlen(block->text) > 0) {

            if (block_printed) {
                size += sprintf(
                    buffer + size, "%%{F%s}%s", bs->config->delimiter_color, bs->config->delimiter);
            }

            if (block->button_command) {
                size += sprintf(buffer + size, " ");
                for (size_t button_id = 1; button_id < 6; button_id++) {
                    size += sprintf(
                        buffer + size, "%%{A%zu:%zu %zu:}", button_id, block->id, button_id);
                }
            }

            if (block->underline_color) {
                size += sprintf(
                    buffer + size,
                    "%%{+u}%%{F%s}%%{U%s}%s%%{U-}%%{-u}",
                    block->text_color ? block->text_color : "-",
                    block->underline_color ? block->underline_color : "-",
                    block->text);

            } else {
                size += sprintf(
                    buffer + size,
                    "%%{F%s}%%{U%s}%s",
                    block->text_color ? block->text_color : "-",
                    block->underline_color ? block->underline_color : "-",
                    block->text);
            }

            if (block->button_command) {
                for (size_t button_id = 1; button_id < 6; button_id++) {
                    size += sprintf(buffer + size, "%%{A}");
                }
            }

            block_printed = 1;

        } else {
            block_printed = 0;
        }

        pthread_mutex_unlock(block->lock);
    }

    for (int i = 0; i < right_padding; i++) {
        size += sprintf(buffer + size, " ");
    }

    size += sprintf(buffer + size, "%%{F-}");

    return size;
}

void draw_bar(Bar_State* bs) {
    char buffer[4096];
    int offset = 0;
    offset += draw_side(buffer + offset, bs, left, 'l', bs->config->left_padding, 0);
    offset += draw_side(buffer + offset, bs, center, 'c', 0, 0);
    offset += draw_side(buffer + offset, bs, right, 'r', 0, bs->config->right_padding);

    buffer[offset] = '\n';
    write(LEMONBAR_PIPE_STDIN[1], buffer, offset + 1);
}

void block_update(Block* block) {
    FILE* fp = popen(block->command, "r");

    if (fp == NULL) {
        ERROR_FMT("Failed to run command: %s\n", block->command);
        return;
    }

    char* line = calloc(BUFF_SIZE, sizeof(char));

    char* text = NULL;
    char* text_color = NULL;
    char* text_underline = NULL;

    for (size_t n_lines_read = 0; fgets(line, BUFF_SIZE, fp) != NULL; n_lines_read++) {
        line[strlen(line) - 1] = '\0';
        switch (n_lines_read) {
            case 0:
                text = strdup(line);
                break;
            case 1:
                text_color = strdup(line);
                break;
            case 2:
                text_underline = strdup(line);
                break;
        }
    }

    pthread_mutex_lock(block->lock);

    free(block->text);
    block->text = text;
    free(block->text_color);
    block->text_color = text_color;
    free(block->underline_color);
    block->underline_color = text_underline;

    pthread_mutex_unlock(block->lock);

    free(line);
    pclose(fp);
}

// Damm you signal handling :(
Bar_State* g_bar_state;

void update_block_and_draw_bar(int signal_id) {
    Block* block = bar_state_get_block(g_bar_state, signal_id);

    if (!block) return;

    block_update(block);
    draw_bar(g_bar_state);
}

void* update_timed_thread(void* signalid) {
    size_t id = (size_t) signalid;
    Block* block = bar_state_get_block(g_bar_state, id);
    size_t delay = block->delay;

    while (1) {
        update_block_and_draw_bar(id);
        sleep(delay);
    }

    pthread_exit(NULL);
}

void* update_continuous_thread(void* signalid) {
    size_t id = (size_t) signalid;
    Block* block = bar_state_get_block(g_bar_state, id);

    FILE* fp = popen(block->command, "r");

    if (fp == NULL) {
        ERROR_FMT("Failed to run command: %s\n", block->command);
        return NULL;
    }

    char* line = calloc(BUFF_SIZE, sizeof(char));

    while (fgets(line, BUFF_SIZE, fp) != NULL) {
        pthread_mutex_lock(block->lock);

        free(block->text);
        block->text = strdup(line);
        block->text[strlen(block->text) - 1] = '\0';

        pthread_mutex_unlock(block->lock);

        draw_bar(g_bar_state);
    }

    free(line);
    pthread_exit(NULL);
}

void block_run(Block* block, const pthread_attr_t* attr) {
    int rc = 0;
    pthread_t thread;
    switch (block->delay) {
        case ONCE:
            block_update(block);
            signal(block->id, update_block_and_draw_bar);
            break;
        case CONTINUOUS:
            rc = pthread_create(&thread, attr, update_continuous_thread, (void*) block->id);
            break;
        default:
            rc = pthread_create(&thread, attr, update_timed_thread, (void*) block->id);
            signal(block->id, update_block_and_draw_bar);
            break;
    }

    if (rc) {
        ERROR_FMT(
            "Could not create pthread\n"
            "return code from pthread_create(): %d\n",
            rc);
    }
}

void bar_state_run(Bar_State* bs) {
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 128);

    for (size_t i = 0; i < bs->right->n_blocks; i++) {
        block_run(bs->right->array[i], &at);
    }

    for (size_t i = 0; i < bs->center->n_blocks; i++) {
        block_run(bs->center->array[i], &at);
    }

    for (size_t i = 0; i < bs->left->n_blocks; i++) {
        block_run(bs->left->array[i], &at);
    }

    pthread_attr_destroy(&at);
}

char* parse_path(const char* dir) {
    char* parsed_dir = NULL;

    if (strstr(dir, "scripts/") == dir) {
        asprintf(&parsed_dir, "%s/.config/thonkbar/%s", HOME(), dir);
    } else {
        parsed_dir = strdup(dir);
    }

    return parsed_dir;
}

void bar_state_insert_block(
    Bar_State* bs,
    enum BAR_AREA bar_mode,
    const char* block_name,
    const char* block_command,
    const char* button_command,
    int delay,
    int essential) {

    static size_t last_id_right = 34;
    static size_t last_id_other = 64;
    size_t new_id = (bar_mode == right) ? last_id_right++ : last_id_other--;

    char* block_command_full_path = parse_path(block_command);

    printf("script: %s\n", block_command_full_path);
    printf("    essential: %d\n", essential);

    char* button_command_full_path = NULL;
    if (button_command) {
        button_command_full_path = parse_path(button_command);
        printf("    button handler: %s\n", button_command_full_path);
    }

    if (delay == CONTINUOUS)
        printf("    CONTINUOUS\n");
    else if (delay == ONCE)
        printf("    signal: %zu\n", new_id);
    else
        printf("    update frequency: %ds\n    signal: %zu\n", delay, new_id);

    printf("\n");

    Block* block = calloc(1, sizeof(Block));

    block->name = safe_strdup(block_name);
    block->command = block_command_full_path;
    block->button_command = button_command_full_path;
    block->delay = delay;
    block->essential = essential;
    block->id = new_id;

    block->lock = malloc(sizeof(pthread_mutex_t));

    pthread_mutex_init(block->lock, NULL);

    block_array_insert(bar_state_get_block_array(bs, bar_mode), block);
}

Bar_State* bar_state_load(dictionary* ini, Config* config) {

    Bar_State* bs = malloc(sizeof(Bar_State));

    bs->display_all = 1;
    bs->left = block_array_make(10);
    bs->center = block_array_make(10);
    bs->right = block_array_make(10);
    bs->config = config;

    for (int i = 0; i < iniparser_getnsec(ini); i++) {
        char const* section_name = iniparser_getsecname(ini, i);

        if (strstr(section_name, "config") != NULL) {
            continue;
        }

        const char* side = iniparser_getsecstring(ini, section_name, "side", NULL);
        enum BAR_AREA bar_mode;
        if (strcmp("left", side) == 0) {
            bar_mode = left;
        } else if (strcmp("right", side) == 0) {
            bar_mode = right;
        } else if (strcmp("center", side) == 0) {
            bar_mode = center;
        } else {
            ERROR_FMT(
                "config: invalid field: %s:side\n"
                "Can only be left, right, center or config\n",
                section_name);
            return NULL;
        }
        const char* cmd = iniparser_getsecstring(ini, section_name, "cmd", NULL);
        const char* update = iniparser_getsecstring(ini, section_name, "update", NULL);

        int update_time;
        if (strstr(update, "ONCE") != NULL) {
            update_time = ONCE;
        } else if (strstr(update, "CONTINUOUS") != NULL) {
            update_time = CONTINUOUS;
        } else {
            update_time = strtol(update, NULL, 10);
            if (errno == EINVAL || update_time <= 0) {
                ERROR_FMT(
                    "config:  invalid field: %s:update\n"
                    "Can only be ONCE, CONTINUOUS or an int greater than 0\n",
                    section_name);
                return NULL;
            }
        }

        const char* event = iniparser_getsecstring(ini, section_name, "event", NULL);

        int essential = iniparser_getsecboolean(ini, section_name, "essential", 1);

        bar_state_insert_block(bs, bar_mode, section_name, cmd, event, update_time, essential);
    }

    return bs;
}

int button_handler(Bar_State* bs) {

    char* line = calloc(BUFF_SIZE, sizeof(char));
    FILE* lemonbar_output = fdopen(LEMONBAR_PIPE_STDOUT[0], "r");

    for (size_t n_lines = 1; fgets(line, BUFF_SIZE, lemonbar_output); n_lines++) {
        size_t block_id;
        size_t button_id;
        sscanf(line, "%zu %zu", &block_id, &button_id);

        char* button_str = NULL;
        switch (button_id) {
            case 1:
                button_str = "LEFT";
                break;
            case 2:
                button_str = "CENTER";
                break;
            case 3:
                button_str = "RIGHT";
                break;
            case 4:
                button_str = "UP";
                break;
            case 5:
                button_str = "DOWN";
                break;
            default:
                break;
        }

        if (!button_str) {
            ERROR_FMT("invalid button id: %s\n", line);
            continue;
        }

        Block* block = bar_state_get_block(bs, block_id);
        if (!block) {
            ERROR_FMT("invalid block id: %s\n", line);
            continue;
        }

        char* id_str = NULL;
        asprintf(&id_str, "%zu", block_id);

        switch (fork()) {
            // child process
            case 0: {
                char* pathname = block->button_command;
                char* program_name = basename(block->button_command);
                printf("> %s %s %s %s\n", pathname, program_name, button_str, id_str);
                execlp(pathname, program_name, button_str, id_str, (char*) NULL);
                ERROR_FMT("failed to run command: %d\n", errno);
                break;
            }
            // parent process
            default:
                wait(NULL);
                break;
        }

        free(id_str);
    }
    fclose(lemonbar_output);

    return 0;
}

int g_lemonbar_id = -1;

int fork_lemonbar(Bar_State* bs) {
    char* argv[32] = {NULL};
    char** iter = argv;

    *iter++ = "lemonbar";
    *iter++ = "-p";
    *iter++ = "-u";
    *iter++ = bs->config->underline_width;
    *iter++ = "-a";
    char max_clickable[10];
    sprintf(
        max_clickable,
        "%zu",
        (bs->left->n_blocks + bs->right->n_blocks + bs->center->n_blocks) * 5);
    *iter++ = max_clickable;

    if (bs->config->font) {
        *iter++ = "-f";
        *iter++ = bs->config->font;
    };
    if (bs->config->background_color) {
        *iter++ = "-B";
        *iter++ = bs->config->background_color;
    }
    if (bs->config->foreground_color) {
        *iter++ = "-F";
        *iter++ = bs->config->foreground_color;
    }
    if (bs->config->text_offset) {
        *iter++ = "-o";
        *iter++ = bs->config->text_offset;
    }
    if (bs->config->bar_position == bottom) *iter++ = "-b";
    if (bs->config->docking_mode == force) *iter++ = "-d";

    if (pipe(LEMONBAR_PIPE_STDIN) < 0) exit(1);
    if (pipe(LEMONBAR_PIPE_STDOUT) < 0) exit(1);

    switch (g_lemonbar_id = fork()) {
        // error
        case -1:
            exit(1);
            break;
        // child process
        case 0:
            dup2(LEMONBAR_PIPE_STDIN[0], 0);
            close(LEMONBAR_PIPE_STDIN[0]);
            dup2(LEMONBAR_PIPE_STDOUT[1], 1);
            close(LEMONBAR_PIPE_STDOUT[1]);

            execvp("lemonbar", argv);
            ERROR_FMT("failed to run lemonbar: %d\n", errno);
            break;
        // parent process
        default: {
            break;
        }
    }

    return 0;
}

void exit_handler(int signal) {
    if (g_lemonbar_id > 0) {
        kill(g_lemonbar_id, signal);
    }
    bar_state_destroy(g_bar_state);
    exit(0);
}

void toggle_bar_drawing_handler(int signal) {
    g_bar_state->display_all = !g_bar_state->display_all;
    printf("> display all: %d\n (signal: %d)", g_bar_state->display_all, signal);
    draw_bar(g_bar_state);
}

int main(void) {
    signal(SIGTERM, exit_handler);
    signal(SIGUSR2, toggle_bar_drawing_handler);

    char* config_file;
    asprintf(&config_file, "%s/.config/thonkbar/config.ini", HOME());

    dictionary* ini = iniparser_load(config_file);
    free(config_file);
    if (!ini) ERROR_FMT("Could not open config file: %s\n", config_file);

    Config* config = config_load(ini);
    if (!config) return 1;

    g_bar_state = bar_state_load(ini, config);
    if (!g_bar_state) return 1;

    iniparser_freedict(ini);

    fork_lemonbar(g_bar_state);
    bar_state_run(g_bar_state);
    button_handler(g_bar_state);

    pthread_exit(NULL);

    bar_state_destroy(g_bar_state);

    return 0;
}
