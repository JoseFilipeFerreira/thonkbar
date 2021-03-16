#ifndef _GNU_SOURCE
#    define _GNU_SOURCE 1
#endif

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define CONTINUOUS -1
#define ONCE 0

#define BUFF_SIZE 1024

int LEMONBAR_PIPE[2];

int trim(char* org, char* dest, char surround_char) {
    int r = 0;
    size_t beg = 0;
    size_t end = strlen(org) - 1;
    while (isspace(org[beg])) beg++;
    if (surround_char && org[beg] == surround_char) {
        r = 1;
        beg++;
    }
    while (isspace(org[end])) end--;
    if (surround_char && org[end] == surround_char) {
        r = 1;
        end--;
    }
    strncpy(dest, org + beg, end - beg + 1);
    return r;
}

struct Block {
    pthread_mutex_t lock;
    char* command;
    char* text;
    char* text_color;
    char* underline_color;
    int delay;
    size_t id;
};

struct Block_Array {
    struct Block* array;
    size_t n_blocks;
    size_t max_blocks;
};

struct Block_Array make(size_t max) {
    struct Block_Array block_arr = {
        .array = malloc(sizeof(struct Block) * max),
        .n_blocks = 0,
        .max_blocks = max,
    };

    return block_arr;
}

void insert(struct Block_Array* block_arr, struct Block* block) {
    if (block_arr->n_blocks >= block_arr->max_blocks) {
        block_arr->array =
            realloc(block_arr->array, sizeof(struct Block) * block_arr->max_blocks * 2);
        block_arr->max_blocks *= 2;
    }

    block_arr->array[block_arr->n_blocks] = *block;
    block_arr->n_blocks++;
}

enum BAR_MODE { left, right, center, config };

struct {
    struct Block_Array right;
    struct Block_Array center;
    struct Block_Array left;
} BAR_STATE;

struct Block_Array* get_block_array(enum BAR_MODE area) {
    switch (area) {
        case left:
            return &BAR_STATE.left;
        case center:
            return &BAR_STATE.center;
        case right:
            return &BAR_STATE.right;
        default:
            return NULL;
    }
}

enum BAR_POSITON { top, bottom };
enum DOCKING_MODE { normal, force };

struct Config {
    char* delimiter;
    char* delimiter_color;
    char* font;
    char* underline_width;
    char* background_color;
    char* foreground_color;
    char* text_offset;
    enum BAR_POSITON bar_position;
    enum DOCKING_MODE docking_mode;
};

struct Config BAR_CONFIG = {
    .delimiter = "  |  ",
    .delimiter_color = "#FFFFFF",
    .font = NULL,
    .underline_width = "2",
    .background_color = NULL,
    .foreground_color = NULL,
    .text_offset = NULL,
    .bar_position = top,
    .docking_mode = normal};

struct Block* get_block(size_t signal_id) {
    for (size_t i = 0; i < BAR_STATE.right.n_blocks; i++)
        if (BAR_STATE.right.array[i].id == signal_id) return &BAR_STATE.right.array[i];

    for (size_t i = 0; i < BAR_STATE.center.n_blocks; i++)
        if (BAR_STATE.center.array[i].id == signal_id) return &BAR_STATE.center.array[i];

    for (size_t i = 0; i < BAR_STATE.left.n_blocks; i++)
        if (BAR_STATE.left.array[i].id == signal_id) return &BAR_STATE.left.array[i];

    return NULL;
}

int draw_side(char* buffer, struct Block_Array block_arr, char marker) {
    int size = sprintf(buffer, "%%{%c}", marker);

    int print_delimiter = 1;
    for (size_t i = 0; i < block_arr.n_blocks; i++) {

        struct Block block = block_arr.array[i];

        pthread_mutex_lock(&block.lock);

        if (block.text) {
            if (block.underline_color) {
                size += sprintf(
                    buffer + size,
                    "%%{+u}%%{F%s}%%{U%s}%s%%{U-}%%{-u}",
                    block.text_color ? block.text_color : "-",
                    block.underline_color ? block.underline_color : "-",
                    block.text);

            } else {
                size += sprintf(
                    buffer + size,
                    "%%{F%s}%%{U%s}%s",
                    block.text_color ? block.text_color : "-",
                    block.underline_color ? block.underline_color : "-",
                    block.text);
            }
        } else {
            print_delimiter = 0;
        }

        pthread_mutex_unlock(&block_arr.array[i].lock);

        if (i < block_arr.n_blocks - 1 && print_delimiter) {
            size += sprintf(
                buffer + size, "%%{F%s}%s", BAR_CONFIG.delimiter_color, BAR_CONFIG.delimiter);
        }

        print_delimiter = 1;
    }

    size += sprintf(buffer + size, "%%{F-}");

    return size;
}

void draw_bar() {
    char buffer[4096];
    int offset = 0;
    offset += draw_side(buffer, BAR_STATE.left, 'l');
    offset += draw_side(buffer + offset, BAR_STATE.center, 'c');
    offset += draw_side(buffer + offset, BAR_STATE.right, 'r');

    buffer[offset] = '\n';

    write(LEMONBAR_PIPE[1], buffer, offset + 1);
}

void update_block(struct Block* block) {
    FILE* fp = popen(block->command, "r");

    if (fp == NULL) {
        fprintf(stderr, "Failed to run command: %s\n", block->command);
        return;
    }

    char line[BUFF_SIZE];

    char* text = NULL;
    char* text_color = NULL;
    char* text_underline = NULL;

    size_t n_lines_read = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        n_lines_read++;
        line[strlen(line) - 1] = '\0';
        switch (n_lines_read) {
            case 1:
                text = strdup(line);
                break;
            case 2:
                text_color = strdup(line);
                break;
            case 3:
                text_underline = strdup(line);
                break;
        }
    }

    pthread_mutex_lock(&block->lock);

    free(block->text);
    block->text = text;
    free(block->text_color);
    block->text_color = text_color;
    free(block->underline_color);
    block->underline_color = text_underline;

    pthread_mutex_unlock(&block->lock);

    pclose(fp);
}

void update_block_and_draw_bar(int signal_id) {
    struct Block* block = get_block(signal_id);

    if (!block) return;

    update_block(block);
    draw_bar();
}

void* update_thread(void* signalid) {
    size_t id = (size_t) signalid;
    struct Block* block = get_block(id);
    size_t delay = block->delay;

    while (1) {
        update_block_and_draw_bar(id);
        sleep(delay);
    }

    pthread_exit(NULL);
}

void* update_continuous_thread(void* signalid) {
    size_t id = (size_t) signalid;
    struct Block* block = get_block(id);

    FILE* fp = popen(block->command, "r");

    if (fp == NULL) {
        fprintf(stderr, "Failed to run command: %s\n", block->command);
        return NULL;
    }

    char line[BUFF_SIZE];

    while (fgets(line, sizeof(line), fp) != NULL) {
        pthread_mutex_lock(&block->lock);

        free(block->text);
        block->text = strdup(line);
        block->text[strlen(block->text) - 1] = '\0';

        pthread_mutex_unlock(&block->lock);

        draw_bar();
    }

    pthread_exit(NULL);
}

void run_block(struct Block block) {
    int rc = 0;
    if (block.delay == ONCE) {
        update_block(&block);
        signal(block.id, update_block_and_draw_bar);
    } else if (block.delay > 0) {
        pthread_t thread;
        rc = pthread_create(&thread, NULL, update_thread, (void*) block.id);
        signal(block.id, update_block_and_draw_bar);
    } else if (block.delay == CONTINUOUS) {
        pthread_t thread;
        rc = pthread_create(&thread, NULL, update_continuous_thread, (void*) block.id);
    }

    if (rc) {
        printf("ERROR; return code from pthread_create() is %d\n", rc);
    }
}

void run_blocks() {
    for (size_t i = 0; i < BAR_STATE.right.n_blocks; i++) run_block(BAR_STATE.right.array[i]);
    for (size_t i = 0; i < BAR_STATE.center.n_blocks; i++) run_block(BAR_STATE.center.array[i]);
    for (size_t i = 0; i < BAR_STATE.left.n_blocks; i++) run_block(BAR_STATE.left.array[i]);
}

void insert_block(enum BAR_MODE bar_mode, char* comand, int delay) {
    static size_t last_id_right = 34;
    static size_t last_id_other = 64;

    char* block_comand = strdup(comand);

    size_t new_id = (bar_mode == right) ? last_id_right++ : last_id_other--;

    if (strstr(block_comand, "scripts/") == block_comand) {
        char* old = block_comand;
        asprintf(&block_comand, "~/.config/thonkbar/%s", block_comand);
        free(old);
    }

    if (delay == CONTINUOUS)
        printf("script: %s\n    CONTINUOUS\n\n", block_comand);
    else if (delay == ONCE)
        printf("script: %s\n    signal: %zu\n\n", block_comand, new_id);
    else
        printf(
            "script: %s\n    update frequency: %ds\n    signal: %zu\n\n",
            block_comand,
            delay,
            new_id);

    struct Block block = {.command = block_comand, .delay = delay, .id = new_id};
    pthread_mutex_init(&block.lock, NULL);

    insert(get_block_array(bar_mode), &block);
}

int parse_config(char* config_file) {
    FILE* f = fopen(config_file, "r");

    if (!f) {
        fprintf(stderr, "%s: \033[31merror:\033[0m Could not open config file\n", config_file);
        return 0;
    }

    char line[BUFF_SIZE];

    enum BAR_MODE bar_mode = -1;

    for (size_t n_lines = 1; fgets(line, BUFF_SIZE, f); n_lines++) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        if (line[0] == '[' && line[strlen(line) - 2] == ']') {
            if (strcmp("[config]\n", line) == 0) {
                bar_mode = config;
            } else if (strcmp("[left]\n", line) == 0) {
                bar_mode = left;
            } else if (strcmp("[right]\n", line) == 0) {
                bar_mode = right;
            } else if (strcmp("[center]\n", line) == 0) {
                bar_mode = center;
            } else {
                fprintf(
                    stderr,
                    "config:%zu: \033[31merror:\033[0m invalid config section\n"
                    "Can only be left, right, center or config\n",
                    n_lines);
                return 0;
            }

        } else if (bar_mode == (enum BAR_MODE) - 1) {
            fprintf(stderr, "config:%zu: \033[31merror:\033[0m section must be defined\n", n_lines);
            return 0;

        } else if (bar_mode == config) {
            char k[128], v[128];
            if (sscanf(line, "%127[^=]=%127[^\n]%*c", k, v) != 2) {
                fprintf(
                    stderr,
                    "config:%zu: \033[31merror:\033[0m invalid config line format\n"
                    "    Must be in the format:\n"
                    "        <property> = \"<string>\"\n"
                    "        <property> = <integer>\n",
                    n_lines);
                return 0;
            }
            char key[128] = {0}, value[128] = {0};
            trim(k, key, '"');
            trim(v, value, '"');

            if (strstr(key, "delimiter_color") != NULL) {
                BAR_CONFIG.delimiter_color = strdup(value);
            } else if (strstr(key, "delimiter") != NULL) {
                BAR_CONFIG.delimiter = strdup(value);
            } else if (strstr(key, "font") != NULL) {
                BAR_CONFIG.font = strdup(value);
            } else if (strstr(key, "background_color") != NULL) {
                BAR_CONFIG.background_color = strdup(value);
            } else if (strstr(key, "foreground_color") != NULL) {
                BAR_CONFIG.foreground_color = strdup(value);
            } else if (strstr(key, "position") != NULL) {
                if (strstr(value, "top") != NULL) {
                    BAR_CONFIG.bar_position = top;
                } else if (strstr(value, "bottom") != NULL) {
                    BAR_CONFIG.bar_position = bottom;
                } else {
                    fprintf(
                        stderr,
                        "config:%zu: \033[31merror:\033[0m invalid bar position\n"
                        "Can only be top or bottom\n",
                        n_lines);
                    return 0;
                }
            } else if (strstr(key, "docking_mode") != NULL) {
                if (strstr(value, "normal") != NULL) {
                    BAR_CONFIG.docking_mode = normal;
                } else if (strstr(value, "force") != NULL) {
                    BAR_CONFIG.docking_mode = force;
                } else {
                    fprintf(
                        stderr,
                        "config:%zu: \033[31merror:\033[0m invalid docking mode\n"
                        "Can only be normal or force\n",
                        n_lines);
                    return 0;
                }
            } else if (strstr(key, "text_offset") != NULL) {
                strtol(value, NULL, 10);
                if (errno == EINVAL) {
                    fprintf(
                        stderr,
                        "config:%zu: \033[31merror:\033[0m invalid text offset\n"
                        "Can only be an integer\n",
                        n_lines);
                    return 0;
                }
                BAR_CONFIG.underline_width = strdup(value);

            } else if (strstr(key, "underline_width") != NULL) {
                long lvalue = strtol(value, NULL, 10);
                if (errno == EINVAL || lvalue < 0) {
                    fprintf(
                        stderr,
                        "config:%zu: \033[31merror:\033[0m invalid underline width\n"
                        "Can only be an integer greater than 0\n",
                        n_lines);
                    return 0;
                }
                BAR_CONFIG.underline_width = strdup(value);

            } else {
                fprintf(
                    stderr,
                    "config:%zu: \033[31merror:\033[0m invalid bar configuration option\n",
                    n_lines);
                return 0;
            }
        } else {
            char k[128], v[128];
            if (sscanf(line, "%127[^,],%127[^\n]%*c", k, v) != 2) {
                fprintf(
                    stderr,
                    "config:%zu: \033[31merror:\033[0m invalid block line format\n"
                    "Must be in the format: <command>, <duration>\n",
                    n_lines);
                return 0;
            }

            char key[128] = {0}, value[128] = {0};
            trim(k, key, '"');
            trim(v, value, '"');

            int update_time;
            if (strstr(value, "ONCE") != NULL) {
                update_time = ONCE;
            } else if (strstr(value, "CONTINUOUS") != NULL) {
                update_time = CONTINUOUS;
            } else {
                update_time = strtol(value, NULL, 10);
                if (errno == EINVAL || update_time < 0) {
                fprintf(
                    stderr,
                    "config:%zu: \033[31merror:\033[0m invalid block update time\n"
                    "Can only be ONCE, CONTINUOUS or an int greater than 0\n",
                    n_lines);
                return 0;
                }
            }
            insert_block(bar_mode, key, update_time);
        }
    }

    fclose(f);

    return 1;
}

int fork_lemonbar() {
    char* argv[16] = {NULL};
    char** iter = argv;

    *iter++ = "lemonbar";
    *iter++ = "-p";
    *iter++ = "-u";
    *iter++ = BAR_CONFIG.underline_width;

    if (BAR_CONFIG.font) {
        *iter++ = "-f";
        *iter++ = BAR_CONFIG.font;
    };
    if (BAR_CONFIG.background_color) {
        *iter++ = "-B";
        *iter++ = BAR_CONFIG.background_color;
    }
    if (BAR_CONFIG.foreground_color) {
        *iter++ = "-F";
        *iter++ = BAR_CONFIG.foreground_color;
    }
    if (BAR_CONFIG.text_offset){
        *iter++ = "-o";
        *iter++ = BAR_CONFIG.text_offset;
    }
    if (BAR_CONFIG.bar_position == bottom) *iter++ = "-b";
    if (BAR_CONFIG.docking_mode == force) *iter++ = "-d";

    if (pipe(LEMONBAR_PIPE) < 0) exit(1);

    switch (fork()) {
        case 0:
            dup2(LEMONBAR_PIPE[0], STDIN_FILENO);
            close(LEMONBAR_PIPE[0]);
            close(LEMONBAR_PIPE[1]);
            execvp("lemonbar", argv);
            break;
        case -1:
            exit(1);
            break;
        default:
            close(LEMONBAR_PIPE[0]);
            run_blocks();
            break;
    }

    return 0;
}

int main(void) {
    BAR_STATE.left = make(10);
    BAR_STATE.center = make(10);
    BAR_STATE.right = make(10);

    const char* HOME = getenv("HOME");
    if (!HOME) {
        HOME = getpwuid(getuid())->pw_dir;
    }

    char config_file[BUFF_SIZE];
    snprintf(config_file, BUFF_SIZE, "%s/.config/thonkbar/config", HOME);

    if (!parse_config(config_file)) return 1;

    fork_lemonbar();

    pthread_exit(NULL);
}
