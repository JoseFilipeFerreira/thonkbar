#ifndef _GNU_SOURCE
#    define _GNU_SOURCE 1
#endif

#include <ctype.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define CONFIG -2
#define CONTINUOUS -1
#define ONCE 0

#define BUFF_SIZE 1024

char* DELIMITER = NULL;
char* DELIMITER_COLOR = NULL;

char* triml(char* str) {
    while (isspace(*++str))
        ;
    return str;
}

struct Block {
    pthread_mutex_t lock;
    char* command;
    char* text;
    char* text_color;
    char* underline_color;
    size_t delay;
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
        block_arr->array = realloc(
            block_arr->array, sizeof(struct Block) * block_arr->max_blocks * 2);
        block_arr->max_blocks *= 2;
    }

    block_arr->array[block_arr->n_blocks] = *block;
    block_arr->n_blocks++;
}

struct Bar {
    struct Block_Array right;
    struct Block_Array center;
    struct Block_Array left;
};

struct Bar BAR_STATE;

enum BAR_MODE { left, right, center, config };

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

struct Block* get_block(size_t signal_id) {
    for (size_t i = 0; i < BAR_STATE.right.n_blocks; i++)
        if (BAR_STATE.right.array[i].id == signal_id)
            return &BAR_STATE.right.array[i];

    for (size_t i = 0; i < BAR_STATE.center.n_blocks; i++)
        if (BAR_STATE.center.array[i].id == signal_id)
            return &BAR_STATE.center.array[i];

    for (size_t i = 0; i < BAR_STATE.left.n_blocks; i++)
        if (BAR_STATE.left.array[i].id == signal_id)
            return &BAR_STATE.left.array[i];

    return NULL;
}

void draw_side(struct Block_Array block_arr, char* marker) {
    printf("%%{%s}", marker);

    int print_delimiter = 1;
    for (size_t i = 0; i < block_arr.n_blocks; i++) {

        struct Block block = block_arr.array[i];

        pthread_mutex_lock(&block.lock);

        if (block.text) {
            if (block.underline_color) {
                printf(
                    "%%{+u}%%{F%s}%%{U%s}%s%%{U-}%%{-u}",
                    block.text_color ? block.text_color : "-",
                    block.underline_color ? block.underline_color : "-",
                    block.text);

            } else {
                printf(
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
            printf("  %%{F%s}%s  ", DELIMITER_COLOR, DELIMITER);
        }

        print_delimiter = 1;
    }

    printf("%%{F-}");
}

void draw_bar() {
    if (BAR_STATE.right.n_blocks > 0) draw_side(BAR_STATE.right, "r");
    if (BAR_STATE.center.n_blocks > 0) draw_side(BAR_STATE.center, "c");
    if (BAR_STATE.left.n_blocks > 0) draw_side(BAR_STATE.left, "l");

    putchar('\n');

    fflush(stdout);
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
        switch (n_lines_read) {
            case 1:
                text = strdup(line);
                strtok(text, "\n");
                break;
            case 2:
                text_color = strdup(line);
                strtok(text_color, "\n");
                break;
            case 3:
                text_underline = strdup(line);
                strtok(text_underline, "\n");
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
        sleep(delay);
        update_block_and_draw_bar(id);
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
        strtok(block->text, "\n");

        pthread_mutex_unlock(&block->lock);

        draw_bar();
    }

    pthread_exit(NULL);
}

void insert_block(enum BAR_MODE bar_mode, char* comand, int delay) {
    static size_t last_id_right = 34;
    static size_t last_id_other = 64;

    char* block_comand = strdup(comand);

    size_t new_id = (bar_mode == right) ? last_id_right++ : last_id_other--;

    if (strstr(block_comand, "scripts/") == block_comand) {
        asprintf(&block_comand, "~/.config/thonkbar/%s", block_comand);
    }

    struct Block block = {
        .command = block_comand, .delay = delay, .id = new_id};

    pthread_mutex_init(&block.lock, NULL);

#ifdef NDEBUG
    printf("BLOCK: %s\n", comand);
#endif

    if (delay > 0 || delay == ONCE) {
        update_block(&block);

        signal(new_id, update_block_and_draw_bar);

#ifdef NDEBUG
        printf("DELAY: %d\nSIGNAL: %zu\n", delay, new_id);
#endif
    }

#ifdef NDEBUG
    putchar('\n');
#endif

    insert(get_block_array(bar_mode), &block);

    if (delay > 0) {
        pthread_t thread;
        int rc = pthread_create(&thread, NULL, update_thread, (void*) new_id);

        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
        }
    }

    if (delay == CONTINUOUS) {
        pthread_t thread;
        int rc = pthread_create(
            &thread, NULL, update_continuous_thread, (void*) new_id);

        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
        }
    }
}

int parse_config(char* config_file) {
    FILE* f = fopen(config_file, "r");

    if (!f) {
        fprintf(
            stderr,
            "%s: \033[31merror:\033[0m Could not open config file",
            config_file);
        return 0;
    }

    char line[BUFF_SIZE];

    enum BAR_MODE bar_mode = -1;

    for (size_t n_lines = 0; fgets(line, BUFF_SIZE, f); n_lines++) {

        line[strlen(line) - 1] = '\0';

        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        if (line[0] == '[' && line[strlen(line) - 1] == ']') {
            if (strcmp("[config]", line) == 0) {
                bar_mode = config;
            } else if (strcmp("[left]", line) == 0) {
                bar_mode = left;
            } else if (strcmp("[right]", line) == 0) {
                bar_mode = right;
            } else if (strcmp("[center]", line) == 0) {
                bar_mode = center;
            } else {
                fprintf(
                    stderr,
                    "config:%zu: \033[31merror:\033[0m invalid block mode\n"
                    "Can only be left, right, center or config\n",
                    n_lines);
                return 0;
            }

        } else if (strchr(line, ',') == NULL) {
            fprintf(
                stderr,
                "config:%zu: \033[31merror:\033[0m invalid line format\n",
                n_lines);
            return 0;

        } else {
            char* key = strtok(line, ",");
            char* value = strtok(NULL, ",");
            int update_time;

            if (!(key && value)) {
                fprintf(
                    stderr,
                    "config:%zu: \033[31merror:\033[0m invalid line format\n",
                    n_lines);
                return 0;
            }

            if (bar_mode == config) {
                if (strstr(key, "delimiter_color") != NULL) {
                    DELIMITER_COLOR = strdup(triml(value));
                } else if (strstr(key, "delimiter") != NULL) {
                    DELIMITER = strdup(triml(value));
                } else {
                    fprintf(
                        stderr,
                        "config:%zu: \033[31merror:\033[0m invalid bar config\n"
                        "Can only be delimiter or delimiter_color\n",
                        n_lines);
                    return 0;
                }
            } else {

                if (strstr(value, "ONCE") != NULL) {
                    update_time = ONCE;
                } else if (strstr(value, "CONTINUOUS") != NULL) {
                    update_time = CONTINUOUS;
                } else if ((update_time = atoi(value)) <= 0) {
                    fprintf(
                        stderr,
                        "config:%zu: \033[31merror:\033[0m invalid block "
                        "update "
                        "time\nCan only be ONCE, CONTINUOUS or an int greater "
                        "than "
                        "0\n",
                        n_lines);
                    return 0;
                }

                if (bar_mode == (enum BAR_MODE) - 1) {
                    fprintf(
                        stderr,
                        "config:%zu: \033[31merror:\033[0m no block location "
                        "defined\n",
                        n_lines);
                    return 0;
                }

                insert_block(bar_mode, key, update_time);
            }
        }
    }

    if (!(DELIMITER && DELIMITER_COLOR)) {
        fprintf(
            stderr,
            "config: \033[31merror:\033[0m delimiter and delimiter_color are "
            "not defined\n");
        return 0;
    }

    fclose(f);

    return 1;
}

int main(void) {
    BAR_STATE.left = make(10);
    BAR_STATE.center = make(10);
    BAR_STATE.right = make(10);

    const char* HOME;
    if ((HOME = getenv("HOME")) == NULL) {
        HOME = getpwuid(getuid())->pw_dir;
    }

    char* config_file;
    asprintf(&config_file, "%s/.config/thonkbar/config", HOME);

    if (!parse_config(config_file)) return 1;

    free(config_file);

    draw_bar();

    pthread_exit(NULL);
}
