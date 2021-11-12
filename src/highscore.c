#include "rhc/string.h"
#include "highscore.h"



//
// protected
//

Highscore highscore_decode(Str_s msg) {

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
