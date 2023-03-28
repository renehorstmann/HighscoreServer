#include <stdlib.h>
#include "s/s.h"
#include "s/endian.h"
#include "s/str.h"
#include "s/string.h"
#include "highscore.h"

#define TYPE HighscoreEntry_s
#define CLASS HE_Array
#define FN_NAME he_array

#include "s/dynarray.h"

#define TYPE HighscorePackEntry_s
#define CLASS HEP_Array
#define FN_NAME hep_array

#include "s/dynarray.h"


#ifndef HIGHSCORE_SECRET_KEY
#define HIGHSCORE_SECRET_KEY 12345
#endif


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

uint64_t highscore_entry_get_checksum(HighscoreEntry_s self) {
    uint64_t hash = HIGHSCORE_SECRET_KEY; 
    hash *= self.score;
    
    const char *str = self.name;
    
    int c;
    while ((c = *str++)) 
        hash = ((hash << 5) + hash) + c;
    
    
    return hash;
}

HighscoreEntry_s highscore_entry_decode(sStr_s entry) {
    if (entry.size > HIGHSCORE_MAX_ENTRY_LENGTH - 1) {
        s_log_warn("highscore_entry_decode failed, entry.size is to long");
        return (HighscoreEntry_s) {0};
    }
    sStr_s splits[4];
    int splits_cnt = s_str_split(splits, 4, entry, '~');
    if (splits_cnt != 3) {
        s_log_warn("highscore_entry_decode failed to parse entry, splits_cnt!=3: %i", splits_cnt);
        return (HighscoreEntry_s) {0};
    }

    char *end;
    int score = (int) strtol(splits[0].data, &end, 10);

    if (end != splits[1].data - 1 || splits[1].size == 0 || splits[1].size > HIGHSCORE_NAME_MAX_LENGTH) {
        s_log_warn("highscore_entry_decode failed to parse entry, invalid score or name length");
        return (HighscoreEntry_s) {0};
    }

    HighscoreEntry_s self = {0};
    self.score = score;
    s_str_as_c(self.name, splits[1]);

    _Static_assert(sizeof(unsigned long long) >= sizeof(uint64_t), "wrong sizes");
    uint64_t checksum = (uint64_t) strtoull(splits[2].data, NULL, 10);
    if (highscore_entry_get_checksum(self) != checksum) {
        s_log_warn("highscore_entry_decode failed to parse entry, invalid checksum");
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


Highscore highscore_decode(sStr_s msg) {
    HE_Array array = he_array_new(8);
    while (!s_str_empty(msg)) {
        sStr_s line;
        msg = s_str_eat_until(msg, '\n', &line);
        msg = s_str_eat(msg, 1);  // newline
        line = s_str_strip(line, ' ');
        if (s_str_empty(line))
            continue;

        HighscoreEntry_s push = highscore_entry_decode(line);
        if (push.name[0] == '\0')
            continue;

        he_array_push(&array, push);
    }
    return (Highscore) {
            .entries = array.array,
            .entries_size = (int) array.size
    };
}

sString *highscore_encode(Highscore self) {
    sString *s = s_string_new(1024);
    for (int i = 0; i < self.entries_size; i++) {
        char entry_buffer[HIGHSCORE_MAX_ENTRY_LENGTH];
        highscore_entry_encode(self.entries[i], entry_buffer);
        s_string_append(s, s_strc(entry_buffer));
        s_string_push(s, '\n');
    }
    return s;
}



uint64_t highscorepack_entry_get_checksum(HighscorePackEntry_s self) {
    uint64_t hash = HIGHSCORE_SECRET_KEY;
    const char *str = self.text;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

HighscorePackEntry_s highscorepack_entry_decode(sStr_s entry) {
    if (entry.size > HIGHSCORE_PACK_MAX_ENTRY_LENGTH - 1) {
        s_log_warn("highscorepack_entry_decode failed, entry.size is to long");
        return (HighscorePackEntry_s) {0};
    }
    sStr_s splits[3];
    int splits_cnt = s_str_split(splits, 3, entry, '~');
    if (splits_cnt != 2) {
        s_log_warn("highscorepack_entry_decode failed to parse entry, splits_cnt!=2: %i", splits_cnt);
        return (HighscorePackEntry_s) {0};
    }


    if (splits[1].size == 0 || splits[1].size > HIGHSCORE_PACK_MAX_LENGTH) {
        s_log_warn("highscorepack_entry_decode failed to parse entry, invalid text length");
        return (HighscorePackEntry_s) {0};
    }

    HighscorePackEntry_s self = {0};
    s_str_as_c(self.text, splits[1]);

    _Static_assert(sizeof(unsigned long long) >= sizeof(uint64_t), "wrong sizes");
    uint64_t checksum = (uint64_t) strtoull(splits[0].data, NULL, 10);
    if (highscorepack_entry_get_checksum(self) != checksum) {
        s_log_warn("highscorepack_entry_decode failed to parse entry, invalid checksum");
        return (HighscorePackEntry_s) {0};
    }

    return self;
}


// out_entry_buffer should be HIGHSCORE_MAX_ENTRY_LENGTH big
void highscorepack_entry_encode(HighscorePackEntry_s self, char *out_entry_buffer) {
    memset(out_entry_buffer, 0, HIGHSCORE_PACK_MAX_ENTRY_LENGTH);
    snprintf(out_entry_buffer, HIGHSCORE_PACK_MAX_ENTRY_LENGTH, "%llu~%s",
             (unsigned long long) highscorepack_entry_get_checksum(self),
             self.text);
}


HighscorePack highscorepack_decode(sStr_s msg) {
    HEP_Array array = hep_array_new(8);
    while (!s_str_empty(msg)) {
        sStr_s line;
        msg = s_str_eat_until(msg, '\n', &line);
        msg = s_str_eat(msg, 1);  // newline
        line = s_str_strip(line, ' ');
        if (s_str_empty(line))
            continue;

        HighscorePackEntry_s push = highscorepack_entry_decode(line);
        if (push.text[0] == '\0')
            continue;

        hep_array_push(&array, push);
    }
    return (HighscorePack) {
            .entries = array.array,
            .entries_size = (int) array.size
    };
}

sString *highscorepack_encode(HighscorePack self) {
    sString *s = s_string_new(1024);
    for (int i = 0; i < self.entries_size; i++) {
        char entry_buffer[HIGHSCORE_MAX_ENTRY_LENGTH];
        highscorepack_entry_encode(self.entries[i], entry_buffer);
        s_string_append(s, s_strc(entry_buffer));
        s_string_push(s, '\n');
    }
    return s;
}


//
// public
//

Highscore highscore_new_msg(sStr_s highscore_msg) {
    return highscore_decode(highscore_msg);
}

void highscore_kill(Highscore *self) {
    s_free(self->entries);
    *self = (Highscore) {0};
}

HighscoreEntry_s highscore_entry_new(const char *name, int score) {
    s_assume(strlen(name) <= HIGHSCORE_NAME_MAX_LENGTH, "highscore_entry_new failed, name too long");
    HighscoreEntry_s self = {0};
    strcpy(self.name, name);
    self.score = score;
    return self;
}

sString *highscore_entry_to_string(HighscoreEntry_s self) {
    sString *res = s_string_new(HIGHSCORE_MAX_ENTRY_LENGTH);
    highscore_entry_encode(self, res->data);
    res->size = strlen(res->data);
    return res;
}

sStr_s highscore_entry_into_buffer(HighscoreEntry_s self, sStr_s buffer) {
    if (buffer.size < HIGHSCORE_MAX_ENTRY_LENGTH) {
        s_log_wtf("highscore_entry_into_buffer failed, buffer size to small");
        memset(buffer.data, 0, buffer.size);
    }
    highscore_entry_encode(self, buffer.data);
    return s_strc(buffer.data);
}

HighscorePack highscorepack_new_msg(sStr_s highscorepack_msg) {
    return highscorepack_decode(highscorepack_msg);
}

void highscorepack_kill(HighscorePack *self) {
    s_free(self->entries);
    *self = (HighscorePack) {0};
}

// create a new entry by name and score
HighscorePackEntry_s highscorepack_entry_new(const char *text) {
    s_assume(strlen(text) <= HIGHSCORE_PACK_MAX_LENGTH, "highscorepack_entry_new failed, text too long");
    HighscorePackEntry_s self = {0};
    strcpy(self.text, text);
    return self;
}

sString *highscorepack_entry_to_string(HighscorePackEntry_s self) {
    sString *res = s_string_new(HIGHSCORE_PACK_MAX_ENTRY_LENGTH);
    highscorepack_entry_encode(self, res->data);
    res->size = strlen(res->data);
    return res;
}

// buffer should be at least HIGHSCORE_PACK_MAX_ENTRY_LENGTH big
// returns buffer with the new size
sStr_s highscorepack_entry_into_buffer(HighscorePackEntry_s self, sStr_s buffer) {
    if (buffer.size < HIGHSCORE_PACK_MAX_ENTRY_LENGTH) {
        s_log_wtf("highscorepack_entry_into_buffer failed, buffer size to small");
        memset(buffer.data, 0, buffer.size);
    }
    highscorepack_entry_encode(self, buffer.data);
    return s_strc(buffer.data);
}
