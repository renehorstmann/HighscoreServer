#include <stdio.h>
#include <limits.h>
#include <microhttpd.h>
#include <pthread.h>

#include "s/s_impl.h"

#include "highscore.h"


// so the number score position ranges from 1:999
#define HIGHSCORE_MAX_ENTRIES 999

// max ring buffer size
#define HIGHSCORE_PACK_MAX_ENTRIES 128



#define SERVER_PORT 10000


//#define DEBUG_MODE

/**
 * Highscores: (all topics that does NOT start with /pack/ )
 */

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

/**
 * Packs: (all topics that does start with /pack/ )
 */

/**
 * HTTP Server API:
 * GET /pack/path/to/topic
 *      returns the topic file, if available
 * POST /pack/path/to/topic
 *      "Content-Type: plain/text" (is ignored)
 *      data="<CHECKSUM>~<TEXT>"
 *      saves the entry under the topic and returns the topic file
 *      saves and returns in a FIFO ring buffer
 */

/**
 * entry is sens as:
 * uint64_t as ascii
 * ~
 * text
 * padding to end with '\0'
 */

// protected functions:

HighscoreEntry_s highscore_entry_decode(sStr_s entry);

void highscore_entry_encode(HighscoreEntry_s self, char *out_entry_buffer);

Highscore highscore_decode(sStr_s msg);

sString *highscore_encode(Highscore self);


uint64_t highscorepack_entry_get_checksum(HighscorePackEntry_s self);

HighscorePackEntry_s highscorepack_entry_decode(sStr_s entry);

void highscorepack_entry_encode(HighscorePackEntry_s self, char *out_entry_buffer);

HighscorePack highscorepack_decode(sStr_s msg);

sString *highscorepack_encode(HighscorePack self);



static struct {
    pthread_mutex_t lock;
} L = {PTHREAD_MUTEX_INITIALIZER};

static void make_dirs(const char *topic) {
#ifdef DEBUG_MODE
    s_log("MAKE_DIRS not performed, in DEBUG_MODE");
#else
    char file[256];
    snprintf(file, 256, "topics/%s", topic);

    char syscall[256 + 32];
    snprintf(syscall, 256 + 32, "mkdir -p %s", file);

    system(syscall);
#endif
}


static bool check_sorted(void *array, int n, size_t item_size, int (*comp_fun)(const void *a, const void *b)) {
    for(int i=0; i<n-1; i++) {
        void *a = ((char*) array)+i*item_size;
        void *b = ((char*) array)+(i+1)*item_size;
        if (comp_fun(a, b) > 0) {
            return false;
        }
    }
    return true;
}

// bubble sort
// not used, but here for... stuff... ... ...
static void bsort(void *array, int n, size_t item_size, int (*comp_fun)(const void *a, const void *b)) {
    void *tmp = malloc(item_size);
    for (int i = 1; i < n; i++){
        for (int j = 0; j < n - 1 ; j++){
            void *a = ((char*) array)+i*item_size;
            void *b = ((char*) array)+j*item_size;
            if (comp_fun(a, b) < 0) {
                memcpy(tmp, b, item_size);
                memcpy(b, a, item_size);
                memcpy(a, tmp, item_size);
            }
        }
    }
    free(tmp);
}

// sorted bubble sort
// if the a and b are equal, they are not swapped!
static void sbsort(void *array, int n, size_t item_size, int (*comp_fun)(const void *a, const void *b)) {
    void *tmp = malloc(item_size);
    for (int i = 1; i < n; i++){
        for (int j = 0; j < n-1 ; j++){
            void *a = ((char*) array)+j*item_size;
            void *b = ((char*) array)+(j+1)*item_size;
            if (comp_fun(a, b) > 0) {
                memcpy(tmp, b, item_size);
                memcpy(b, a, item_size);
                memcpy(a, tmp, item_size);
            }
        }
    }
    free(tmp);
}
static int highscore_sort_compare(const void *a, const void *b) {
    const HighscoreEntry_s *entry_a = a;
    const HighscoreEntry_s *entry_b = b;
    return entry_b->score - entry_a->score;
}


static void highscore_sort(Highscore *self) {
    if(!check_sorted(self->entries, self->entries_size, sizeof *self->entries, highscore_sort_compare)) {
        sbsort(self->entries, self->entries_size, sizeof *self->entries, highscore_sort_compare);
        s_log("highscore sorted...?!?");
    }
}

static void highscore_remove_entry(Highscore *self, int idx) {
    for (int i = idx; i < self->entries_size - 1; i++) {
        self->entries[i] = self->entries[i + 1];
    }
    self->entries_size--;
}

static void highscore_add_new_entry(Highscore *self, HighscoreEntry_s add) {
    self->entries = s_renew(HighscoreEntry_s , self->entries, self->entries_size + 1);

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

    int search = -1;
    for (int i = 0; i < self->entries_size; i++) {
        if (strcmp(self->entries[i].name, add.name) == 0) {
            if (search>=0) {
                highscore_remove_entry(self, i);
                i--;    // retry the new entry on i, cause the old has been removed
                continue;
            }
            search = i;
        }
    }

    if (search>=0) {
        if (self->entries[search].score < add.score) {
            highscore_remove_entry(self, search);
            highscore_add_new_entry(self, add);
        }
    } else {
        highscore_add_new_entry(self, add);
    }
}

static void highscorepack_add_entry(HighscorePack *self, HighscorePackEntry_s add) {
    if (add.text[0] == '\0')
        return;

    int entries_size = self->entries_size + 1;
    if(entries_size>HIGHSCORE_PACK_MAX_ENTRIES) {
        entries_size = HIGHSCORE_PACK_MAX_ENTRIES;
    }

    HighscorePackEntry_s *entries = s_new(HighscorePackEntry_s, entries_size);

    // first is the new in the fifo ring
    entries[0] = add;

    // copy rest (could be a memcpy, but I was to lazy to calc the bytes, so let the compiler optimize it...)
    for(int i=1; i<entries_size; i++) {
        entries[i] = self->entries[i-1];
    }

    // move
    free(self->entries);
    self->entries = entries;
    self->entries_size = entries_size;
}

// topic and entry must be 0 terminated!
// returns false if the entry was not valid
static bool save_entry(sStr_s topic, sStr_s entry) {
    HighscoreEntry_s add = highscore_entry_decode(entry);
    if (add.name[0] == '\0')
        return false;

    make_dirs(topic.data);
    char file[256];
    snprintf(file, 256, "topics/%s.txt", topic.data);

    pthread_mutex_lock(&L.lock);
    {
        sString *msg = s_file_read(file, true);

        Highscore highscore = highscore_decode(s_string_get_str(msg));
        s_string_kill(&msg);

        highscore_add_entry(&highscore, add);

        if (highscore.entries_size > HIGHSCORE_MAX_ENTRIES) {
            highscore.entries_size = HIGHSCORE_MAX_ENTRIES;
        }

        highscore_sort(&highscore);

        sString *save = highscore_encode(highscore);
        highscore_kill(&highscore);

        if (!s_file_write(file, s_string_get_str(save), true)) {
            s_log("failed to save topic file: %s", file);
        } else {
            s_log("new highscore saved");
        }

        s_string_kill(&save);
    }
    pthread_mutex_unlock(&L.lock);

    return true;
}

// topic and entry must be 0 terminated!
// returns false if the entry was not valid
static bool save_pack_entry(sStr_s topic, sStr_s entry) {
    HighscorePackEntry_s add = highscorepack_entry_decode(entry);
    if (add.text[0] == '\0')
        return false;

    make_dirs(topic.data);
    char file[256];
    snprintf(file, 256, "topics/%s.txt", topic.data);

    pthread_mutex_lock(&L.lock);
    {
        sString *msg = s_file_read(file, true);

        HighscorePack highscore = highscorepack_decode(s_string_get_str(msg));
        s_string_kill(&msg);

        highscorepack_add_entry(&highscore, add);

        sString *save = highscorepack_encode(highscore);
        highscorepack_kill(&highscore);

        if (!s_file_write(file, s_string_get_str(save), true)) {
            s_log("failed to save topic file: %s", file);
        } else {
            s_log("new highscore saved");
        }

        s_string_kill(&save);
    }
    pthread_mutex_unlock(&L.lock);

    return true;
}

static int http_send_highscore(struct MHD_Connection *connection, const char *topic) {
    s_log("http_send_highscore");
    char file[256];
    snprintf(file, 256, "topics/%s.txt", topic);

    sString *msg;

    pthread_mutex_lock(&L.lock);
    {
        msg = s_file_read(file, true);
    }
    pthread_mutex_unlock(&L.lock);

    if (!s_string_valid(msg)) {
        s_log("failed to read topic file: %s", topic);
        return MHD_NO;
    }

    struct MHD_Response *response = MHD_create_response_from_buffer(msg->size, msg->data, MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/plain");
    MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    if (!ret)
        s_log("http_send_highscore failed to queue response");
    MHD_destroy_response(response);
    s_string_kill(&msg);
    return ret;
}


// the ubuntu server is ok with int, but wsl needs HMD_RESULT?
#ifdef DEBUG_MODE
static enum MHD_Result
#else
static int
#endif
        http_request(void *cls,
                        struct MHD_Connection *connection,
                        const char *url,
                        const char *method,
                        const char *version,
                        const char *upload_data, size_t *upload_data_size, void **ptr) {
    s_log("http_request: %s, method: %s", url, method);
    sStr_s topic = s_str_eat_str(s_strc(url), s_strc("/api/"));

    bool is_pack = s_str_begins_with(topic, s_strc("pack/"));

    if (s_str_empty(topic) || s_str_count(topic, '.') > 0) {
        s_log("http_request stopped, topic invalid");
        return MHD_NO;
    }

    if (topic.size >= HIGHSCORE_TOPIC_MAX_LENGTH) {
        s_log("http_request stopped, topic to large");
        return MHD_NO;
    }

    if (strcmp(method, "GET") == 0) {
        // in both cases (Highscore and HighscorePack), just the file is sent back
        return http_send_highscore(connection, topic.data);
    }

    if (strcmp(method, "POST") == 0) {
        if (!*ptr) {
            *ptr = (void *) 1;
            s_log("http_request POST start");
            if (*upload_data_size > 0) {
                s_log("http_request POST start failed, got data?");
                return MHD_NO;
            }
            // here could be checked for Content-Type == plain/text

            // request not finished yet
            return MHD_YES;
        }

        if (upload_data) {
            s_log("http_request POST got data");
            sString *entry = s_string_new_clone((sStr_s) {(char *) upload_data, *upload_data_size});
            bool ok;

            if(is_pack) {
                ok = save_pack_entry(topic, s_string_get_str(entry));
            } else {
                ok = save_entry(topic, s_string_get_str(entry));
            }
            s_string_kill(&entry);
            if (!ok)
                return MHD_NO;

            // upload_data_size consumed (this is important!)
            *upload_data_size = 0;
            // request not finished yet
            return MHD_YES;
        }

        s_log("http_request POST end");

        // in both cases (Highscore and HighscorePack), just the file is sent back
        return http_send_highscore(connection, topic.data);
    }

    // unexpected method
    s_log("unexpected method");
    return MHD_NO;
}

int main(int argc, char **argv) {
    s_log("Server start");
    struct MHD_Daemon *d = MHD_start_daemon(
            0 | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION,
            SERVER_PORT,
            NULL, NULL, &http_request, NULL,
            MHD_OPTION_END);
    if (!d) {
        s_log("failed to start the server");
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG_MODE

    {
        HighscoreEntry_s data;
        data.score = 12345;
        snprintf(data.name, sizeof data.name, "Hello World");
        sString *example_entry = highscore_entry_to_string(data);
        s_log("example score: <%s>", example_entry->data);
        s_string_kill(&example_entry);
    }
    {
        HighscorePackEntry_s data;
        snprintf(data.text, sizeof data.text, "Hello World");
        sString *example_entry = highscorepack_entry_to_string(data);
        s_log("example pack: <%s>", example_entry->data);
        s_string_kill(&example_entry);
    }

    // wait for key
    getchar();
#else
    // wait for ever
    system("tail -f /dev/null");
#endif
    s_log("Server closed");
    return 0;
}
