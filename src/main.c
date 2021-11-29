#include <stdio.h>
#include <limits.h>

#include "rhc/rhc_impl.h"

#include "highscore.h"


#define HIGHSCORE_ENTRY_LENGTH 128
#define HIGHSCORE_MAX_ENTRIES 256

static const uint8_t HIGHSCORE_READ = 0;
static const uint8_t HIGHSCORE_WRITE_READ = 1;


/**
 * msg host to server description:
 * 1 byte:
 *      HIGHSCORE_READ: receive the highscore msg
 *      HIGHSCORE_WRITE_READ: send an entry and receive the highscore msg
 * HIGHSCORE_TOPIC_MAX_LENGTH bytes:
 *      topic name, zero terminated
 * opt if HIGHSCORE_WRITE_READ -> HIGHSCORE_ENTRY_MAX_LENGTH bytes:
 *      the entry to send, new line terminated
 */

/**
 * msg server to host description:
 * 4 byte:
 *      as uint32_t: msg size
 * msg size bytes:
 *      msg
 */

// protected functions:

HighscoreEntry_s highscore_entry_decode(Str_s entry);
void highscore_entry_encode(HighscoreEntry_s self, char *out_entry_buffer);
Highscore highscore_decode(Str_s msg);
String highscore_encode(Highscore self);


static void make_dirs(const char *topic) {
    char file[256];
    snprintf(file, 256, "topics/%s", topic);

    char syscall[256+32];
    snprintf(syscall, 256+32, "mkdir -p %s", file);

    system(syscall);
}

static int highscore_sort_compare (const void *a, const void *b) {
    const HighscoreEntry_s *entry_a = a;
    const HighscoreEntry_s *entry_b = b;
    return entry_b->score - entry_a->score;
}

static void highscore_sort(Highscore *self) {
    qsort(self->entries, self->entries_size, sizeof *self->entries, highscore_sort_compare);
}

static void highscore_remove_entry(Highscore *self, int idx) {
    for(int i=idx; i<self->entries_size-1; i++) {
        self->entries[i] = self->entries[i+1];
    }
    self->entries_size--;
}

static void highscore_add_new_entry(Highscore *self, HighscoreEntry_s add) {
    self->entries = rhc_realloc(self->entries, sizeof *self->entries * (self->entries_size+1));

    for(int i=0; i<self->entries_size; i++) {
        if(self->entries[i].score < add.score) {

            // move others down
            for(int j=self->entries_size-1; j>=i; j--) {
                self->entries[j+1] = self->entries[j];
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
    if(add.name[0] == '\0')
        return;

    HighscoreEntry_s *search = NULL;
    for(int i=0; i<self->entries_size; i++) {
        if(strcmp(self->entries[i].name, add.name) == 0) {
            if(search) {
                highscore_remove_entry(self, i);
                i--;
                continue;
            }
            search = &self->entries[i];
        }
    }

    if(search) {
        if(search->score < add.score) {
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

    if(highscore.entries_size > HIGHSCORE_MAX_ENTRIES) {
        highscore.entries_size = HIGHSCORE_MAX_ENTRIES;
    }

    highscore_sort(&highscore);

    String save = highscore_encode(highscore);
    highscore_kill(&highscore);

    if(!file_write(file, save.str, true)) {
        printf("failed to save topic file: %s\n", file);
    } else {
        puts("new highscore saved");
    }

    string_kill(&save);
}

static void send_highscore(Socket *client, const char *topic) {
    Stream_i stream = socket_get_stream(client);

    char file[256];
    snprintf(file, 256, "topics/%s.txt", topic);

    String msg = file_read(file, true);
    if(!string_valid(msg)) {
        printf("failed to read topic file: %s\n", file);
        return;
    }

    uint32_t size = msg.size;
    stream_write_msg(stream, &size, sizeof size);
    stream_write_msg(stream, msg.data, msg.size);
    if(!string_valid(msg)) {
        puts("failed to send highscore");
    } else {
        puts("highscore send");
    }

    string_kill(&msg);
}

static void handle_client(Socket *client) {
    Stream_i stream = socket_get_stream(client);

    uint8_t mode;
    char topic[HIGHSCORE_TOPIC_MAX_LENGTH];
    stream_read_msg(stream, &mode, sizeof mode);
    stream_read_msg(stream, topic, sizeof topic);

    if(!socket_valid(client)) {
        puts("failed to get mode and topic");
        return;
    }
    if(mode != HIGHSCORE_READ && mode != HIGHSCORE_WRITE_READ) {
        printf("unknown mode: %i\n", mode);
        return;
    }
    if(topic[0] == '\0' || topic[HIGHSCORE_TOPIC_MAX_LENGTH-1] != '\0') {
        puts("invalid topic");
        return;
    }


    if(mode == HIGHSCORE_WRITE_READ) {
        char entry[HIGHSCORE_ENTRY_LENGTH];
        stream_read_msg(stream, entry, HIGHSCORE_ENTRY_LENGTH);

        if(!socket_valid(client)) {
            puts("failed to get entry");
            return;
        }
        if(entry[0] == '\0' || entry[HIGHSCORE_ENTRY_LENGTH-1] != '\0') {
            puts("invalid entry");
            return;
        }

        save_entry(topic, entry);
    }

    send_highscore(client, topic);
}

int main(int argc, char **argv) {
    puts("Server start");
    SocketServer server = socketserver_new("0.0.0.0", 10000);
    if(!rhc_socketserver_valid(server)) {
        puts("failed to start the server");
        return 1;
    }

    for(;;) {
        puts("accepting client...");
        Socket *client = socketserver_accept(&server);
        if(socket_valid(client)) {
            puts("client connected");
            handle_client(client);
            puts("client connection end");
        }
        socket_kill(&client);
    }
}
