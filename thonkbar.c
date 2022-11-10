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
#include <sys/wait.h>
#include <unistd.h>

#define CONTINUOUS -1
#define ONCE 0

#define BUFF_SIZE 1024
#define MAX_FONTS 5

int LEMONBAR_PIPE_STDIN[2];
int LEMONBAR_PIPE_STDOUT[2];

int trim(char* org, char* dest, char surround_char) {
    int r = 0;
    size_t beg = 0;
    size_t size_org = strlen(org);
    if (size_org == 0) {
        dest = NULL;
        return 0;
    }
    size_t end = size_org - 1;
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
    char* button_command;
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

void insert(struct Block_Array* blocks, struct Block* block) {
    if (blocks->n_blocks >= blocks->max_blocks) {
        blocks->array = realloc(blocks->array, sizeof(struct Block) * blocks->max_blocks * 2);
        blocks->max_blocks *= 2;
    }

    blocks->array[blocks->n_blocks] = *block;
    blocks->n_blocks++;
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
    size_t n_fonts;
    char* fonts[MAX_FONTS];
    char* underline_width;
    char* background_color;
    char* foreground_color;
    char* text_offset;
    size_t right_padding;
    size_t left_padding;
    enum BAR_POSITON bar_position;
    enum DOCKING_MODE docking_mode;
};

struct Config BAR_CONFIG = {
    .delimiter = " | ",
    .delimiter_color = "#FFFFFF",
    .n_fonts = 0,
    .fonts = {NULL, NULL, NULL, NULL, NULL},
    .underline_width = "2",
    .background_color = NULL,
    .foreground_color = NULL,
    .text_offset = NULL,
    .right_padding = 0,
    .left_padding = 0,
    .bar_position = top,
    .docking_mode = normal};

struct Block* get_block(size_t signal_id) {
    for (size_t i = 0; i < BAR_STATE.right.n_blocks; i++) {
        if (BAR_STATE.right.array[i].id == signal_id) {
            return &BAR_STATE.right.array[i];
        }
    }

    for (size_t i = 0; i < BAR_STATE.center.n_blocks; i++) {
        if (BAR_STATE.center.array[i].id == signal_id) {
            return &BAR_STATE.center.array[i];
        }
    }

    for (size_t i = 0; i < BAR_STATE.left.n_blocks; i++) {
        if (BAR_STATE.left.array[i].id == signal_id) {
            return &BAR_STATE.left.array[i];
        }
    }

    return NULL;
}

int draw_side(
    char* buffer, struct Block_Array blocks, char marker, int left_padding, int right_padding) {
    int size = sprintf(buffer, "%%{%c}", marker);

    for (int i = 0; i < left_padding; i++) {
        size += sprintf(buffer + size, " ");
    }

    int print_delimiter = 1;
    for (size_t i = 0; i < blocks.n_blocks; i++) {

        struct Block block = blocks.array[i];

        pthread_mutex_lock(&block.lock);

        if (block.text && strlen(block.text) > 0) {

            if (block.button_command) {
                size += sprintf(buffer + size, " ");
                for (size_t button_id = 1; button_id < 6; button_id++) {
                    size +=
                        sprintf(buffer + size, "%%{A%zu:%zu %zu:}", button_id, block.id, button_id);
                }
            }

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

            if (block.button_command) {
                for (size_t button_id = 1; button_id < 6; button_id++) {
                    size += sprintf(buffer + size, "%%{A}");
                }
            }

        } else {
            print_delimiter = 0;
        }

        pthread_mutex_unlock(&blocks.array[i].lock);

        if (i < blocks.n_blocks - 1 && print_delimiter) {
            size += sprintf(
                buffer + size, "%%{F%s}%s", BAR_CONFIG.delimiter_color, BAR_CONFIG.delimiter);
        }

        print_delimiter = 1;
    }

    for (int i = 0; i < right_padding; i++) {
        size += sprintf(buffer + size, " ");
    }

    size += sprintf(buffer + size, "%%{F-}");

    return size;
}

void draw_bar() {
    char buffer[4096];
    int offset = 0;
    offset += draw_side(buffer, BAR_STATE.left, 'l', BAR_CONFIG.left_padding, 0);
    offset += draw_side(buffer + offset, BAR_STATE.center, 'c', 0, 0);
    offset += draw_side(buffer + offset, BAR_STATE.right, 'r', 0, BAR_CONFIG.right_padding);

    buffer[offset] = '\n';
    write(LEMONBAR_PIPE_STDIN[1], buffer, offset + 1);
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

void run_block(struct Block* block, const pthread_attr_t* attr) {
    int rc = 0;
    pthread_t thread;
    switch (block->delay) {
        case ONCE:
            update_block(block);
            signal(block->id, update_block_and_draw_bar);
            break;
        case CONTINUOUS:
            rc = pthread_create(&thread, attr, update_continuous_thread, (void*) block->id);
            break;
        default:
            rc = pthread_create(&thread, attr, update_thread, (void*) block->id);
            signal(block->id, update_block_and_draw_bar);
            break;
    }

    if (rc) {
        fprintf(
            stderr,
            "\033[31merror:\033[0m Could not create pthread\n"
            "return code from pthread_create(): %d\n",
            rc);
    }
}

void run_blocks() {
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 128);

    for (size_t i = 0; i < BAR_STATE.right.n_blocks; i++) {
        run_block(&BAR_STATE.right.array[i], &at);
    }

    for (size_t i = 0; i < BAR_STATE.center.n_blocks; i++) {
        run_block(&BAR_STATE.center.array[i], &at);
    }

    for (size_t i = 0; i < BAR_STATE.left.n_blocks; i++) {
        run_block(&BAR_STATE.left.array[i], &at);
    }

    pthread_attr_destroy(&at);
}

char* parsed_dir(char* dir){
    char* parsed_dir = NULL;

    if (strstr(dir, "scripts/") == dir) {
        asprintf(&parsed_dir, "%s/.config/thonkbar/%s", getpwuid(getuid())->pw_dir, dir);
    } else {
        parsed_dir = strdup(dir);
    }

    return parsed_dir;
}

void insert_block(enum BAR_MODE bar_mode, char* block_command, char* button_command, int delay) {
    static size_t last_id_right = 34;
    static size_t last_id_other = 64;

    size_t new_id = (bar_mode == right) ? last_id_right++ : last_id_other--;

    char* block_command_owned = parsed_dir(block_command);

    printf("script: %s\n", block_command_owned);

    char* button_command_owned = NULL;
    if (button_command){
        button_command_owned = parsed_dir(button_command);
        printf("    button handler: %s\n", button_command_owned);
    }


    if (delay == CONTINUOUS)
        printf("    CONTINUOUS\n");
    else if (delay == ONCE)
        printf("    signal: %zu\n", new_id);
    else
        printf( "    update frequency: %ds\n    signal: %zu\n", delay, new_id);

    printf("\n");

    struct Block block = {
        .command = block_command_owned,
        .button_command = button_command_owned,
        .delay = delay,
        .id = new_id};

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

    for (size_t n_lines = 1; fgets(line, sizeof(line), f); n_lines++) {

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
                if (BAR_CONFIG.n_fonts > 4) {
                    fprintf(
                        stderr,
                        "config:%zu: \033[31merror:\033[0m too many diferent fonts\n"
                        "    Must be less than %d:\n",
                        n_lines,
                        MAX_FONTS);
                    return 0;
                }
                BAR_CONFIG.fonts[BAR_CONFIG.n_fonts++] = strdup(value);
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

            } else if (strstr(key, "left_padding") != NULL) {
                long lvalue = strtol(value, NULL, 10);
                if (errno == EINVAL || lvalue < 0) {
                    fprintf(
                        stderr,
                        "config:%zu: \033[31merror:\033[0m invalid left_padding\n"
                        "Can only be an integer greater than 0\n",
                        n_lines);
                    return 0;
                }
                BAR_CONFIG.left_padding = lvalue;
            } else if (strstr(key, "right_padding") != NULL) {
                long lvalue = strtol(value, NULL, 10);
                if (errno == EINVAL || lvalue < 0) {
                    fprintf(
                        stderr,
                        "config:%zu: \033[31merror:\033[0m invalid right_padding\n"
                        "Can only be an integer greater than 0\n",
                        n_lines);
                    return 0;
                }
                BAR_CONFIG.right_padding = lvalue;
            } else {
                fprintf(
                    stderr,
                    "config:%zu: \033[31merror:\033[0m invalid bar configuration option\n",
                    n_lines);
                return 0;
            }
        } else {
            char c[128] = {0}, t[128] = {0}, b[128] = {0};

            if (sscanf(line, "%127[^,],%127[^,],%127[^\n]%*c", c, t, b) == 3) {
            } else if (sscanf(line, "%127[^,],%127[^\n]%*c", c, t) != 2) {
                fprintf(
                    stderr,
                    "config:%zu: \033[31merror:\033[0m invalid block line format\n"
                    "Must be in the format:"
                    "    <command>, <duration>\n"
                    "    <command>, <duration>, <button handler script>\n",
                    n_lines);
                return 0;
            }

            char command[128] = {0}, time[128] = {0}, button[128] = {0};
            trim(c, command, '"');
            trim(t, time, '"');
            trim(b, button, '"');

            int update_time;
            if (strstr(time, "ONCE") != NULL) {
                update_time = ONCE;
            } else if (strstr(time, "CONTINUOUS") != NULL) {
                update_time = CONTINUOUS;
            } else {
                update_time = strtol(time, NULL, 10);
                if (errno == EINVAL || update_time <= 0) {
                    fprintf(
                        stderr,
                        "config:%zu: \033[31merror:\033[0m invalid block update time\n"
                        "Can only be ONCE, CONTINUOUS or an int greater than 0\n",
                        n_lines);
                    return 0;
                }
            }
            insert_block(bar_mode, command, strlen(button) > 0 ? button : NULL, update_time);
        }
    }

    fclose(f);

    return 1;
}

int button_handler() {
    char line[BUFF_SIZE];
    FILE* lemonbar_output = fdopen(LEMONBAR_PIPE_STDOUT[0], "r");
    for (size_t n_lines = 1; fgets(line, sizeof(line), lemonbar_output); n_lines++) {
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
            printf("> invalid button id: %s\n", line);
            continue;
        }

        struct Block* block = get_block(block_id);
        if (!block) {
            printf("> invalid block id: %s\n", line);
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
                printf("failed\n");
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

int LEMONBAR_ID = -1;

int fork_lemonbar() {
    char* argv[32] = {NULL};
    char** iter = argv;

    *iter++ = "lemonbar";
    *iter++ = "-p";
    *iter++ = "-u";
    *iter++ = BAR_CONFIG.underline_width;
    *iter++ = "-a";
    char max_clickable[10];
    sprintf(
        max_clickable,
        "%zu",
        (BAR_STATE.left.n_blocks + BAR_STATE.right.n_blocks + BAR_STATE.center.n_blocks) * 5);
    *iter++ = max_clickable;

    for (size_t i = 0; i < BAR_CONFIG.n_fonts; i++) {
        *iter++ = "-f";
        *iter++ = BAR_CONFIG.fonts[i];
    };
    if (BAR_CONFIG.background_color) {
        *iter++ = "-B";
        *iter++ = BAR_CONFIG.background_color;
    }
    if (BAR_CONFIG.foreground_color) {
        *iter++ = "-F";
        *iter++ = BAR_CONFIG.foreground_color;
    }
    if (BAR_CONFIG.text_offset) {
        *iter++ = "-o";
        *iter++ = BAR_CONFIG.text_offset;
    }
    if (BAR_CONFIG.bar_position == bottom) *iter++ = "-b";
    if (BAR_CONFIG.docking_mode == force) *iter++ = "-d";

    if (pipe(LEMONBAR_PIPE_STDIN) < 0) exit(1);
    if (pipe(LEMONBAR_PIPE_STDOUT) < 0) exit(1);

    switch (LEMONBAR_ID = fork()) {
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
            break;
        // parent process
        default: {
            break;
        }
    }

    return 0;
}

void exit_handler(int signal) {
    if (LEMONBAR_ID > 0) {
        kill(LEMONBAR_ID, signal);
    }
    exit(0);
}

int main(void) {
    signal(SIGTERM, exit_handler);

    BAR_STATE.left = make(10);
    BAR_STATE.center = make(10);
    BAR_STATE.right = make(10);

    const char* HOME = getenv("HOME");
    if (!HOME) HOME = getpwuid(getuid())->pw_dir;

    char* config_file;
    asprintf(&config_file, "%s/.config/thonkbar/config", HOME);

    if (!parse_config(config_file)) return 1;

    free(config_file);

    fork_lemonbar();

    run_blocks();

    button_handler();

    sleep(10);

    pthread_exit(NULL);
}
