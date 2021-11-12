#ifndef HIGHSCORESERVER_HIGHSCORE_H
#define HIGHSCORESERVER_HIGHSCORE_H

#include <stdint.h>

#define HIGHSCORE_NAME_MAX_LENGTH 16
#define HIGHSCORE_TOPIC_MAX_LENGTH 64
#define HIGHSCORE_ADDRESS_MAX_LENGTH 16

typedef struct {
    char name[HIGHSCORE_NAME_MAX_LENGTH];
    int score;
} HighscoreEntry_s;

typedef struct {
    char topic[HIGHSCORE_TOPIC_MAX_LENGTH];
    char address[HIGHSCORE_ADDRESS_MAX_LENGTH];
    uint16_t port;
    HighscoreEntry_s *entries;
    int entries_size;
} Highscore;

// blocks until the highscore is received (if possible)
// if an error occures, the highscore remains empty
Highscore highscore_new_receive(const char *topic, const char *address, uint16_t port);

void highscore_kill(Highscore *self);

HighscoreEntry_s highscore_entry_new(const char *name, int score);

// blocks until the highscore is updated and the new version is received (if possible)
// returns true if the transmission was successfully
bool highscore_send_entry(Highscore *self, HighscoreEntry_s send);

#endif //HIGHSCORESERVER_HIGHSCORE_H
