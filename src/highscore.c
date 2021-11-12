#include "rhc/string.h"
#include "highscore.h"

#define TYPE HighscoreEntry
#define CLASS HE_Array
#define FN_NAME he_array
#include "rhc/dynarray.h"


//
// private
//

static Str_s eat_entry(Str_s s, HighscoreEntry *opt_entry) {

    return s;
}

//
// protected
//

Highscore highscore_decode(Str_s msg) {
    HE_Array array = he_array_new(8);
    while(!str_empty(msg)) {
        Str_s line;
        msg = str_eat_until(msg, '\n', &line);
        msg = str_eat(msg, 1);  // newline
        line = str_strip(line, ' ');
        // todo
    }
}

String highscore_encode(Highscore self) {

}


//
// public
//

Highscore highscore_new_receive(const char *topic, const char *address, uint16_t port) {

}

void highscore_kill(Highscore *self) {
    rhc_free(self->entries);
    *self = (Highscore) {0};
}

void highscore_send_entry(Highscore *self, HighscoreEntry send) {

}
