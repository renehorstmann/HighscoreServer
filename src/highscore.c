#include <stdlib.h>
#include "rhc/log.h"
#include "rhc/endian.h"
#include "rhc/string.h"
#include "rhc/stream.h"
#include "rhc/socket.h"
#include "highscore.h"

#define TYPE HighscoreEntry
#define CLASS HE_Array
#define FN_NAME he_array
#include "rhc/dynarray.h"

#define HIGHSCORE_ENTRY_MAX_LENGTH 32

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
 *      the entry to send
 */

/**
 * msg server to host description:
 * 4 byte:
 *      as uint32_t: msg size
 * msg size bytes:
 *      msg
 */



//
// protected
//


HighscoreEntry highscore_entry_decode(Str_s entry) {
    Str_s splits[3];
    int splits_cnt = str_split(splits, 3, entry, '~');
    if(splits_cnt != 2) {
        log_warn("highscore_entry_decode failed to parse entry, splits_cnt!=2: %i", splits_cnt);
        return (HighscoreEntry) {0};
    }

    char *end;
    int score = (int) strtol(splits[0].data, &end, 10);

    if(end != splits[1].data-1 || splits[1].size == 0 || splits[1].size >= HIGHSCORE_NAME_MAX_LENGTH) {
        log_warn("highscore_entry_decode failed to parse entry, invalid score or name length");
        return (HighscoreEntry) {0};
    }

    HighscoreEntry self = {0};
    self.score = score;
    str_as_c(self.name, splits[1]);
    return self;
}


// out_entry_buffer should be HIGHSCORE_ENTRY_MAX_LENGTH big
void highscore_entry_encode(HighscoreEntry self, char *out_entry_buffer) {
    snprintf(out_entry_buffer, HIGHSCORE_ENTRY_MAX_LENGTH, "%i~%s\n", self.score, self.name);
}


Highscore highscore_decode(Str_s msg) {
    HE_Array array = he_array_new(8);
    while(!str_empty(msg)) {
        Str_s line;
        msg = str_eat_until(msg, '\n', &line);
        msg = str_eat(msg, 1);  // newline
        line = str_strip(line, ' ');
        if(str_empty(line))
            continue;

        HighscoreEntry push = highscore_entry_decode(line);
        if(push.name[0] == '\0')
            continue;

        he_array_push(&array, push);
    }
    return (Highscore) {
            .entries = array.array,
            .entries_size = (int) array.size
    };
}

String highscore_encode(Highscore self) {
    String s = string_new(1024);
    for(int i=0; i<self.entries_size; i++) {
        char entry_buffer[HIGHSCORE_ENTRY_MAX_LENGTH];
        highscore_entry_encode(self.entries[i], entry_buffer);
        string_append(&s, strc(entry_buffer));
    }
    return s;
}


//
// public
//

Highscore highscore_new_receive(const char *topic, const char *address, uint16_t port) {
    assume(strlen(topic) < HIGHSCORE_TOPIC_MAX_LENGTH, "highscore failed, invalid topic: %s", topic);
    assume(strlen(address) < HIGHSCORE_ADDRESS_MAX_LENGTH, "highscore failed, invalid address: %s", address);

    Socket *so = socket_new(address, port);
    if(!socket_valid(so))
        return (Highscore) {0};

    Stream_i stream = socket_get_stream(so);

    // write to server

    stream_write_msg(stream, &HIGHSCORE_READ, sizeof HIGHSCORE_READ);

    char topic_buffer[HIGHSCORE_TOPIC_MAX_LENGTH] = {0};
    strcpy(topic_buffer, topic);
    stream_write_msg(stream, topic_buffer, sizeof topic_buffer);

    // recv from server

    uint32_t size;
    stream_read_msg(stream, &size, sizeof size);
    if(!socket_valid(so))
        return (Highscore) {0};

    size = endian_le_to_host32(size);

    String msg = string_new(size);
    msg.size = size;
    stream_read_msg(stream, msg.data, size);
    if(!socket_valid(so)) {
        string_kill(&msg);
        return (Highscore) {0};
    }
    socket_kill(&so);

    Highscore self = highscore_decode(msg.str);
    strcpy(self.topic, topic);
    strcpy(self.address, address);
    self.port = port;
    string_kill(&msg);
    return self;
}

void highscore_kill(Highscore *self) {
    rhc_free(self->entries);
    *self = (Highscore) {0};
}

bool highscore_send_entry(Highscore *self, HighscoreEntry send) {
    Socket *so = socket_new(self->address, self->port);
    if(!socket_valid(so))
        return false;

    Stream_i stream = socket_get_stream(so);

    // write to server

    stream_write_msg(stream, &HIGHSCORE_WRITE_READ, sizeof HIGHSCORE_WRITE_READ);

    char topic_buffer[HIGHSCORE_TOPIC_MAX_LENGTH] = {0};
    strcpy(topic_buffer, self->topic);
    stream_write_msg(stream, topic_buffer, sizeof topic_buffer);

    char entry_buffer[HIGHSCORE_ENTRY_MAX_LENGTH];
    highscore_entry_encode(send, entry_buffer);
    stream_write_msg(stream, entry_buffer, sizeof entry_buffer);

    // recv from server

    uint32_t size;
    stream_read_msg(stream, &size, sizeof size);
    if(!socket_valid(so))
        return false;

    size = endian_le_to_host32(size);

    String msg = string_new(size);
    msg.size = size;
    stream_read_msg(stream, msg.data, size);
    if(!socket_valid(so)) {
        string_kill(&msg);
        return false;
    }
    socket_kill(&so);

    Highscore new_hs = highscore_decode(msg.str);
    string_kill(&msg);

    rhc_free(self->entries);
    self->entries = new_hs.entries;
    self->entries_size = new_hs.entries_size;
    return true;
}
