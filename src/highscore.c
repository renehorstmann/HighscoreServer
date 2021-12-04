#include <stdlib.h>
#include "rhc/log.h"
#include "rhc/endian.h"
#include "rhc/string.h"
#include "highscore.h"

#define TYPE HighscoreEntry_s
#define CLASS HE_Array
#define FN_NAME he_array
#include "rhc/dynarray.h"

#define HIGHSCORE_MAX_ENTRY_LENGTH 128


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


//
// protected
//


HighscoreEntry_s highscore_entry_decode(Str_s entry) {
    if(entry.size > HIGHSCORE_MAX_ENTRY_LENGTH - 1) {
        log_warn("highscore_entry_decode failed, entry.size is to long");
        return (HighscoreEntry_s) {0};
    }
    Str_s splits[4];
    int splits_cnt = str_split(splits, 4, entry, '~');
    if(splits_cnt != 3) {
        log_warn("highscore_entry_decode failed to parse entry, splits_cnt!=3: %i", splits_cnt);
        return (HighscoreEntry_s) {0};
    }

    char *end;
    int score = (int) strtol(splits[0].data, &end, 10);

    if(end != splits[1].data-1 || splits[1].size == 0 || splits[1].size >= HIGHSCORE_NAME_MAX_LENGTH) {
        log_warn("highscore_entry_decode failed to parse entry, invalid score or name length");
        return (HighscoreEntry_s) {0};
    }

    HighscoreEntry_s self = {0};
    self.score = score;
    str_as_c(self.name, splits[1]);

    _Static_assert(sizeof(unsigned long long) >= sizeof(uint64_t), "wrong sizes");
    uint64_t checksum = (uint64_t) strtoull(splits[2].data, NULL, 10);
    if(highscore_entry_get_checksum(self) != checksum) {
        log_warn("highscore_entry_decode failed to parse entry, invalid checksum");
        return (HighscoreEntry_s) {0};
    }

    return self;
}


// out_entry_buffer should be HIGHSCORE_MAX_ENTRY_LENGTH big
void highscore_entry_encode(HighscoreEntry_s self, char *out_entry_buffer) {
    memset(out_entry_buffer, 0, HIGHSCORE_MAX_ENTRY_LENGTH);
    snprintf(out_entry_buffer, HIGHSCORE_MAX_ENTRY_LENGTH, "%i~%s~%llu", self.score, self.name,
             (unsigned long long) highscore_entry_get_checksum(self));
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

        HighscoreEntry_s push = highscore_entry_decode(line);
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
        char entry_buffer[HIGHSCORE_MAX_ENTRY_LENGTH];
        highscore_entry_encode(self.entries[i], entry_buffer);
        string_append(&s, strc(entry_buffer));
        string_push(&s, '\n');
    }
    return s;
}


//
// public
//

void highscore_kill(Highscore *self) {
    rhc_free(self->entries);
    *self = (Highscore) {0};
}

HighscoreEntry_s highscore_entry_new(const char *name, int score) {
    assume(strlen(name) < HIGHSCORE_NAME_MAX_LENGTH, "highscore_entry_new failed, name too long");
    HighscoreEntry_s self = {0};
    strcpy(self.name, name);
    self.score = score;
    return self;
}

#ifdef OPTION_FETCH
Highscore highscore_new_get(const char *address, const char *topic) {
    assume(strlen(address) < HIGHSCORE_ADDRESS_MAX_LENGTH, "highscore_new_get failed, invalid address: %s", address);
    assume(strlen(topic) < HIGHSCORE_TOPIC_MAX_LENGTH, "highscore_new_get failed, invalid topic: %s", topic);
    // todo
}

Highscore highscore_new_post(const char *address, const char *topic, HighscoreEntry_s send) {
    assume(strlen(address) < HIGHSCORE_ADDRESS_MAX_LENGTH, "highscore_new_post failed, invalid address: %s", address);
    assume(strlen(topic) < HIGHSCORE_TOPIC_MAX_LENGTH, "highscore_new_post failed, invalid topic: %s", topic);
    // todo
}
#endif
