#ifndef HIGHSCORESERVER_HIGHSCORE_H
#define HIGHSCORESERVER_HIGHSCORE_H

#include <stdint.h>
#include <stdbool.h>

#define HIGHSCORE_NAME_MAX_LENGTH 16
#define HIGHSCORE_TOPIC_MAX_LENGTH 64
#define HIGHSCORE_ADDRESS_MAX_LENGTH 128

typedef struct {
    char name[HIGHSCORE_NAME_MAX_LENGTH];   // null terminated
    int score;
} HighscoreEntry_s;

typedef struct {
    char topic[HIGHSCORE_TOPIC_MAX_LENGTH];   // null terminated
    char address[HIGHSCORE_ADDRESS_MAX_LENGTH];   // null terminated
    uint16_t port;
    HighscoreEntry_s *entries;
    int entries_size;
} Highscore;


//
// function prototype!
// this function implementation should not be made public available
// return the checksum for the given entry
//
uint64_t highscore_entry_get_checksum(HighscoreEntry_s self);


//
// public highscore stuff:
//

void highscore_kill(Highscore *self);

// create a new entry by name and score
HighscoreEntry_s highscore_entry_new(const char *name, int score);


#ifdef OPTION_FETCH
// blocks until the highscore is received (if possible)
// if an error occures, the highscore remains empty
Highscore highscore_new_get(const char *address, const char *topic);

// blocks until the highscore is updated and the new version is received (if possible)
// returns true if the transmission was successfully
Highscore highscore_new_post(const char *address, const char *topic, HighscoreEntry_s send);
#endif

#endif //HIGHSCORESERVER_HIGHSCORE_H
