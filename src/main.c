#include <stdio.h>
#include <limits.h>
#include <microhttpd.h>

#include "rhc/rhc_impl.h"

#include "highscore.h"


#define HIGHSCORE_MAX_ENTRY_LENGTH 128
#define HIGHSCORE_MAX_ENTRIES 256

#define SERVER_PORT 10000


/**
 * HTTP Server API:
 * GET /path/to/topic
 *      returns the topic file, if available
 * POST /path/to/topic  entry=<SCORE>~<NAME>~<CHECKSUM>
 *      saves the entry under the topic and returns the topic file
 */

/**
 * entry is sens as:
 * score as ascii
 * ~
 * name
 * ~
 * uint64_t as ascii
 * padding to end with '\0'
 */

// protected functions:

HighscoreEntry_s highscore_entry_decode(Str_s entry);

void highscore_entry_encode(HighscoreEntry_s self, char *out_entry_buffer);

Highscore highscore_decode(Str_s msg);

String highscore_encode(Highscore self);


static void make_dirs(const char *topic) {
    char file[256];
    snprintf(file, 256, "topics/%s", topic);

    char syscall[256 + 32];
    snprintf(syscall, 256 + 32, "mkdir -p %s", file);

    system(syscall);
}

static int highscore_sort_compare(const void *a, const void *b) {
    const HighscoreEntry_s *entry_a = a;
    const HighscoreEntry_s *entry_b = b;
    return entry_b->score - entry_a->score;
}

static void highscore_sort(Highscore *self) {
    qsort(self->entries, self->entries_size, sizeof *self->entries, highscore_sort_compare);
}

static void highscore_remove_entry(Highscore *self, int idx) {
    for (int i = idx; i < self->entries_size - 1; i++) {
        self->entries[i] = self->entries[i + 1];
    }
    self->entries_size--;
}

static void highscore_add_new_entry(Highscore *self, HighscoreEntry_s add) {
    self->entries = rhc_realloc(self->entries, sizeof *self->entries * (self->entries_size + 1));

    for (int i = 0; i < self->entries_size; i++) {
        if (self->entries[i].score < add.score) {

            // move others down
            for (int j = self->entries_size - 1; j >= i; j--) {
                self->entries[j + 1] = self->entries[j];
            }

            self->entries[i] = add;
            self->entries_size++;
            return;
        }
    }

    // add is the last
    self->entries[self->entries_size++] = add;
}

static void highscore_add_entry(Highscore *self, HighscoreEntry_s add) {
    if (add.name[0] == '\0')
        return;

    HighscoreEntry_s *search = NULL;
    for (int i = 0; i < self->entries_size; i++) {
        if (strcmp(self->entries[i].name, add.name) == 0) {
            if (search) {
                highscore_remove_entry(self, i);
                i--;
                continue;
            }
            search = &self->entries[i];
        }
    }

    if (search) {
        if (search->score < add.score) {
            search->score = add.score;
        }
    } else {
        highscore_add_new_entry(self, add);
    }
}

static void save_entry(const char *topic, const char *entry) {
    make_dirs(topic);

    char file[256];
    snprintf(file, 256, "topics/%s.txt", topic);

    String msg = file_read(file, true);

    Highscore highscore = highscore_decode(msg.str);
    string_kill(&msg);

    HighscoreEntry_s add = highscore_entry_decode(strc(entry));

    highscore_add_entry(&highscore, add);

    if (highscore.entries_size > HIGHSCORE_MAX_ENTRIES) {
        highscore.entries_size = HIGHSCORE_MAX_ENTRIES;
    }

    highscore_sort(&highscore);

    String save = highscore_encode(highscore);
    highscore_kill(&highscore);

    if (!file_write(file, save.str, true)) {
        printf("failed to save topic file: %s\n", file);
    } else {
        puts("new highscore saved");
    }

    string_kill(&save);
}

static int http_send_highscore(struct MHD_Connection *connection, const char *topic) {
    char file[256];
    snprintf(file, 256, "topics/%s.txt", topic);
    String msg = file_read(file, true);
    if (!string_valid(msg)) {
        printf("failed to read topic file: %s\n", topic);
        return MHD_NO;
    }

    struct MHD_Response *response = MHD_create_response_from_buffer(msg.size, msg.data, MHD_RESPMEM_MUST_COPY);

    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    string_kill(&msg);
    return ret;
}


typedef struct {
    struct MHD_PostProcessor *postprocessor;
    char topic[HIGHSCORE_TOPIC_MAX_LENGTH];
    bool got_entry;
} HttpPostMsg;

static int http_iterate_post(void *postmsg_cls, enum MHD_ValueKind kind, const char *key,
             const char *filename, const char *content_type,
             const char *transfer_encoding, const char *data,
             uint64_t off, size_t size) {
    printf("iterate POST data: %s\n", key);
    HttpPostMsg *post_msg = postmsg_cls;
    if(strcmp(key, "entry") == 0) {
        if(strlen(data) >= HIGHSCORE_MAX_ENTRY_LENGTH) {
            printf("post entry failed, entry to large");
            return MHD_YES;
        }
        save_entry(post_msg->topic, data);
        post_msg->got_entry = true;
        return MHD_NO;  // stop iterating the post message
    }
    return MHD_YES;
}

static int http_request(void *cls,
                        struct MHD_Connection *connection,
                        const char *url,
                        const char *method,
                        const char *version,
                        const char *upload_data, size_t *upload_data_size, void **ptr) {
    printf("request: %s, method: %s\n", url, method);
    Str_s topic = str_eat_str(strc(url), strc("/api/"));;
    if (str_empty(topic) || str_count(topic, '.') > 0) {
        return MHD_NO;
    }

    if(topic.size>=HIGHSCORE_TOPIC_MAX_LENGTH) {
        log_warn("http request failed, topic to large");
        return MHD_NO;
    }

    if (!*ptr) {
        HttpPostMsg *post_msg = rhc_calloc(sizeof *post_msg);
        str_as_c(post_msg->topic, topic);
        if (strcmp(method, "POST") == 0) {
            post_msg->postprocessor = MHD_create_post_processor(connection, 65536, http_iterate_post, post_msg);
            if (!post_msg->postprocessor) {
                rhc_free(post_msg);
                return MHD_NO;
            }
        }
        *ptr = post_msg;
        return MHD_YES;
    }

    if (strcmp(method, "POST") == 0) {
        HttpPostMsg *post_msg = *ptr;
        if (*upload_data_size != 0) {
            MHD_post_process(post_msg->postprocessor, upload_data, *upload_data_size);
            *upload_data_size = 0;
            return MHD_YES;
        } else if (1 || post_msg->got_entry) {
            return http_send_highscore(connection, topic.data);
        }
    }

    if (strcmp(method, "GET") == 0) {
        return http_send_highscore(connection, topic.data);
    }

    // unexpected method
    return MHD_NO;
}

static void http_request_completed(void *cls, struct MHD_Connection *connection,
                       void **post_msg_cls, enum MHD_RequestTerminationCode toe) {
    HttpPostMsg *post_msg = *post_msg_cls;
    if(!post_msg) return;
    if(post_msg->postprocessor)
        MHD_destroy_post_processor(post_msg->postprocessor);

    rhc_free(post_msg);
    *post_msg_cls = NULL;
}

int main(int argc, char **argv) {
    puts("Server start");
    struct MHD_Daemon *d = MHD_start_daemon(
            MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION,
            SERVER_PORT,
            NULL, NULL, &http_request, NULL,
            MHD_OPTION_NOTIFY_COMPLETED, http_request_completed, NULL,
            MHD_OPTION_END);
    if (!d) {
        log_error("failed to start the server");
        exit(EXIT_FAILURE);
    }

    // wait for ever
    system("tail -f /dev/null");
    puts("Server closed (keyboard interrupt?)");
    return 0;
}
