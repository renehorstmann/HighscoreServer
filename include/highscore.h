#ifndef HIGHSCORESERVER_HIGHSCORE_H
#define HIGHSCORESERVER_HIGHSCORE_H

#include "s/s.h"
#include "s/str.h"
#include "s/string.h"


#define HIGHSCORE_TOPIC_MAX_LENGTH 64

#define HIGHSCORE_NAME_MAX_LENGTH 16 // 16 chars, excl null terminator (so buf size = 17)
#define HIGHSCORE_NAME_BUF_SIZE 17
#define HIGHSCORE_MAX_ENTRY_LENGTH 128

#define HIGHSCORE_PACK_MAX_LENGTH 127 // 127 chars, excl null terminator (so buf size = 128)
#define HIGHSCORE_PACK_BUF_SIZE 128
#define HIGHSCORE_PACK_MAX_ENTRY_LENGTH 256

typedef struct {
    char name[HIGHSCORE_NAME_BUF_SIZE];   // + null terminated
    int score;
} HighscoreEntry_s;

typedef struct {
    HighscoreEntry_s *entries;
    int entries_size;
} Highscore;


typedef struct {
    char text[HIGHSCORE_PACK_BUF_SIZE];
} HighscorePackEntry_s;

typedef struct {
    HighscorePackEntry_s *entries;
    int entries_size;
} HighscorePack;


//
// public highscore stuff:
//

Highscore highscore_new_msg(sStr_s highscore_msg);

void highscore_kill(Highscore *self);

// create a new entry by name and score
HighscoreEntry_s highscore_entry_new(const char *name, int score);

sString *highscore_entry_to_string(HighscoreEntry_s self);

// buffer should be at least HIGHSCORE_MAX_ENTRY_LENGTH big
// returns buffer with the new size
sStr_s highscore_entry_into_buffer(HighscoreEntry_s self, sStr_s buffer);

//
// Packs are stored and returned in a FIFO ring buffer
//

HighscorePack highscorepack_new_msg(sStr_s highscorepack_msg);

void highscorepack_kill(HighscorePack *self);

// create a new entry by name and score
HighscorePackEntry_s highscorepack_entry_new(const char *text);

sString *highscorepack_entry_to_string(HighscorePackEntry_s self);

// buffer should be at least HIGHSCORE_PACK_MAX_ENTRY_LENGTH big
// returns buffer with the new size
sStr_s highscorepack_entry_into_buffer(HighscorePackEntry_s self, sStr_s buffer);


#endif //HIGHSCORESERVER_HIGHSCORE_H
