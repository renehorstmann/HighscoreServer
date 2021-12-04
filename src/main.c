#include <stdio.h>
#include <limits.h>
#include <microhttpd.h>
#include <pthread.h>

#include "rhc/rhc_impl.h"

#include "highscore.h"


#define HIGHSCORE_MAX_ENTRY_LENGTH 128
#define HIGHSCORE_MAX_ENTRIES 256

#define SERVER_PORT 10000

//#define DEBUG_MODE

/**
 * HTTP Server API:
 * GET /path/to/topic
 *      returns the topic file, if available
 * POST /path/to/topic
 *      "Content-Type: plain/text" (is ignored)
 *      data="<SCORE>~<NAME>~<CHECKSUM>"
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


static struct {
    pthread_mutex_t lock;
} L = {PTHREAD_MUTEX_INITIALIZER};

static void make_dirs(const char *topic) {
#ifdef DEBUG_MODE
    puts("MAKE_DIRS not performed, in DEBUG_MODE");
#else
    char file[256];
    snprintf(file, 256, "topics/%s", topic);

    char syscall[256 + 32];
    snprintf(syscall, 256 + 32, "mkdir -p %s", file);

    system(syscall);
#endif
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

// topic and entry must be 0 terminated!
// returns false if the entry was not valid
static bool save_entry(Str_s topic, Str_s entry) {
    HighscoreEntry_s add = highscore_entry_decode(entry);
    if (add.name[0] == '\0')
        return false;

    make_dirs(topic.data);
    char file[256];
    snprintf(file, 256, "topics/%s.txt", topic.data);

    pthread_mutex_lock(&L.lock);
    {
        String msg = file_read(file, true);

        Highscore highscore = highscore_decode(msg.str);
        string_kill(&msg);

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
    pthread_mutex_unlock(&L.lock);

    return true;
}

static int http_send_highscore(struct MHD_Connection *connection, const char *topic) {
    puts("http_send_highscore");
    char file[256];
    snprintf(file, 256, "topics/%s.txt", topic);

    String msg;

    pthread_mutex_lock(&L.lock);
    {
        msg = file_read(file, true);
    }
    pthread_mutex_unlock(&L.lock);

    if (!string_valid(msg)) {
        printf("failed to read topic file: %s\n", topic);
        return MHD_NO;
    }

    struct MHD_Response *response = MHD_create_response_from_buffer(msg.size, msg.data, MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/plain");
    MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    if (!ret)
        printf("http_send_highscore failed to queue response");
    MHD_destroy_response(response);
    string_kill(&msg);
    return ret;
}

static int http_request(void *cls,
                        struct MHD_Connection *connection,
                        const char *url,
                        const char *method,
                        const char *version,
                        const char *upload_data, size_t *upload_data_size, void **ptr) {
    printf("http_request: %s, method: %s\n", url, method);
    Str_s topic = str_eat_str(strc(url), strc("/api/"));

    if (str_empty(topic) || str_count(topic, '.') > 0) {
        puts("http_request stopped, topic invalid");
        return MHD_NO;
    }

    if (topic.size >= HIGHSCORE_TOPIC_MAX_LENGTH) {
        puts("http_request stopped, topic to large");
        return MHD_NO;
    }

    if (strcmp(method, "GET") == 0) {
        return http_send_highscore(connection, topic.data);
    }

    if (strcmp(method, "POST") == 0) {
        if (!*ptr) {
            *ptr = (void *) 1;
            puts("http_request POST start");
            if (*upload_data_size > 0) {
                puts("http_request POST start failed, got data?");
                return MHD_NO;
            }
            // here could be checked for Content-Type == plain/text

            // request not finished yet
            return MHD_YES;
        }

        if (upload_data) {
            puts("http_request POST got data");
            String entry = string_new_clone((Str_s) {(char *) upload_data, *upload_data_size});
            bool ok = save_entry(topic, entry.str);
            string_kill(&entry);
            if (!ok)
                return MHD_NO;

            // upload_data_size consumed (this is important!)
            *upload_data_size = 0;
            // request not finished yet
            return MHD_YES;
        }

        puts("http_request POST end");
        return http_send_highscore(connection, topic.data);
    }

    // unexpected method
    puts("unexpected method");
    return MHD_NO;
}

int main(int argc, char **argv) {
    puts("Server start");
    struct MHD_Daemon *d = MHD_start_daemon(
            0 | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION,
            SERVER_PORT,
            NULL, NULL, &http_request, NULL,
            MHD_OPTION_END);
    if (!d) {
        puts("failed to start the server");
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG_MODE
    // wait for key
    getchar();
#else
    // wait for ever
    system("tail -f /dev/null");
#endif
    puts("Server closed");
    return 0;
}
