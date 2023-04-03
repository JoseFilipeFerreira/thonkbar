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

char* trim(char* str, char surround_char) {
    size_t size_str = strlen(str);

    while (isspace(str[size_str - 1])) {
        str[size_str-- - 1] = '\0';
        if (size_str < 1) return NULL;
    }

    if (str[size_str - 1] == surround_char) {
        str[size_str-- - 1] = '\0';
        if (size_str < 1) return NULL;
    }

    while (isspace(*str)) {
        str++;
        size_str--;
        if (size_str < 1) return NULL;
    }
    if (*str == surround_char) {
        str++;
        size_str--;
        if (size_str < 1) return NULL;
    }

    return str;
}

struct Block {
    pthread_mutex_t lock;
    char* command;
    char* button_command;
    char* text;
    char* text_color;
    char* underline_color;
    int delay;
    int essential;
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

enum BAR_MODE { left, right, center };

struct {
    int display_all;
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
    size_t right_padding;
    size_t left_padding;
    enum BAR_POSITON bar_position;
    enum DOCKING_MODE docking_mode;
};

struct Config BAR_CONFIG;

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

        if (!(BAR_STATE.display_all || block.essential)) {
            pthread_mutex_unlock(&block.lock);
            continue;
        }

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

        pthread_mutex_unlock(&block.lock);

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
        ERROR_FMT("Failed to run command: %s\n", block->command);
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
        ERROR_FMT("Failed to run command: %s\n", block->command);
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
        ERROR_FMT(
            "Could not create pthread\n"
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

char* parsed_dir(const char* dir) {
    char* parsed_dir = NULL;

    if (strstr(dir, "scripts/") == dir) {
        asprintf(&parsed_dir, "%s/.config/thonkbar/%s", HOME(), dir);
    } else {
        parsed_dir = strdup(dir);
    }

    return parsed_dir;
}

void insert_block(enum BAR_MODE bar_mode, const char* block_command, const char* button_command, int delay) {
    static size_t last_id_right = 34;
    static size_t last_id_other = 64;

    size_t new_id = (bar_mode == right) ? last_id_right++ : last_id_other--;

    char* block_command_owned = parsed_dir(block_command);

    printf("script: %s\n", block_command_owned);

    char* button_command_owned = NULL;
    if (button_command) {
        button_command_owned = parsed_dir(button_command);
        printf("    button handler: %s\n", button_command_owned);
    }

    if (delay == CONTINUOUS)
        printf("    CONTINUOUS\n");
    else if (delay == ONCE)
        printf("    signal: %zu\n", new_id);
    else
        printf("    update frequency: %ds\n    signal: %zu\n", delay, new_id);

    printf("\n");

    struct Block block = {
        .command = block_command_owned,
        .button_command = button_command_owned,
        .delay = delay,
        .essential = 1,
        .id = new_id};

    pthread_mutex_init(&block.lock, NULL);

    insert(get_block_array(bar_mode), &block);
}



char* safe_strdup(const char* str) {
    return str ? strdup(str) : NULL;
}

const char* iniparser_getsecstring(const dictionary* d, const char* s, const char* key, char* def){
    char * search;
    asprintf(&search, "%s:%s", s, key);
    const char* r = iniparser_getstring(d, search, def);
    free(search);
    return r;
}

int parse_config(char* config_file) {

    dictionary* ini = iniparser_load(config_file);

    if (!ini) {
        ERROR_FMT("Could not open config file: %s\n", config_file);
    }

    BAR_CONFIG.delimiter = safe_strdup(iniparser_getstring(ini, "config:delimiter", " | "));
    BAR_CONFIG.delimiter_color =
        safe_strdup(iniparser_getstring(ini, "config:delimiter_color", "#FFFFFF"));
    BAR_CONFIG.font = safe_strdup(iniparser_getstring(ini, "config:font", NULL));
    BAR_CONFIG.underline_width =
        safe_strdup(iniparser_getstring(ini, "config:underline_width", "2"));
    BAR_CONFIG.background_color =
        safe_strdup(iniparser_getstring(ini, "config:background_color", NULL));
    BAR_CONFIG.foreground_color =
        safe_strdup(iniparser_getstring(ini, "config:foreground_color", NULL));
    BAR_CONFIG.text_offset = safe_strdup(iniparser_getstring(ini, "config:text_offset", NULL));
    BAR_CONFIG.right_padding = iniparser_getint(ini, "config:right_padding", 0);
    BAR_CONFIG.left_padding = iniparser_getint(ini, "config:left_padding", 0);

    const char* bar_position = iniparser_getstring(ini, "config:bar_position", "top");

    if (bar_position) {
        if (strstr(bar_position, "top") != NULL) {
            BAR_CONFIG.bar_position = top;
        } else if (strstr(bar_position, "bottom") != NULL) {
            BAR_CONFIG.bar_position = bottom;
        } else {
            ERROR("config: invalid field: config:bar_position\n"
                  "Can only be top or bottom\n")
            return 0;
        }
    }

    const char* docking_mode = iniparser_getstring(ini, "config:docking_mode", "normal");

    if (docking_mode) {
        if (strstr(docking_mode, "normal") != NULL) {
            BAR_CONFIG.docking_mode = normal;
        } else if (strstr(docking_mode, "force") != NULL) {
            BAR_CONFIG.docking_mode = force;
        } else {
            ERROR("config: invalid field: config:docking_mode\n"
                  "Can only be normal or force\n");
            return 0;
        }
    }

    for (int i = 0; i < iniparser_getnsec(ini); i++) {
        char const* section_name = iniparser_getsecname(ini, i);

        if (strstr(section_name, "config") != NULL) {
            continue;
        }


        const char* side = iniparser_getsecstring(ini, section_name, "side", NULL);
        enum BAR_MODE bar_mode;
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
            return 0;
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
                return 0;
            }
        }

        const char* event = iniparser_getsecstring(ini, section_name, "event", NULL);

        insert_block(bar_mode, cmd, event, update_time);
    }

    iniparser_freedict(ini);

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
            ERROR_FMT("invalid button id: %s\n", line);
            continue;
        }

        struct Block* block = get_block(block_id);
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
    if (LEMONBAR_ID > 0) {
        kill(LEMONBAR_ID, signal);
    }
    exit(0);
}

void toggle_bar_drawing(int signal) {
    BAR_STATE.display_all = !BAR_STATE.display_all;
    printf("draw all (%d): %d\n", signal, BAR_STATE.display_all);
    draw_bar();
}

int main(void) {
    signal(SIGTERM, exit_handler);
    signal(SIGUSR2, toggle_bar_drawing);

    BAR_STATE.display_all = 1;
    BAR_STATE.left = make(10);
    BAR_STATE.center = make(10);
    BAR_STATE.right = make(10);

    char* config_file;
    asprintf(&config_file, "%s/.config/thonkbar/config.ini", HOME());

    if (!parse_config(config_file)) return 1;

    free(config_file);

    fork_lemonbar();

    run_blocks();

    button_handler();

    sleep(10);

    pthread_exit(NULL);
}
