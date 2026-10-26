#ifndef IPIS_GAME_H
#define IPIS_GAME_H

#define HAND_SIZE 5
#define DECK_SIZE 15
#define SLOTS 3
#define START_HP 50
#define ROUND_ENERGY 4
#define MAX_AP 2

enum Card { EMPTY, KAGAT, LAYOG_KAGAT, SAGANG, DULAG_SAPAK, ULTI_KAGAT };
enum Outcome { ONGOING, PLAYER_ONE_WINS, PLAYER_TWO_WINS, DRAW };

typedef struct {
    int hp;
    int ap;
    int hand[HAND_SIZE];
    int hand_count;
    int deck[DECK_SIZE];
    int deck_count;
    int discard[DECK_SIZE];
    int discard_count;
} Player;

typedef struct {
    Player players[2];
    int round;
    int outcome;
} Game;

/* Picks refer to hand positions 1..5; 0 is an empty combat slot. */
typedef struct { int picks[SLOTS]; } Plan;

typedef struct {
    int cards[2][SLOTS];
    int damage[2][SLOTS]; /* Damage TAKEN by each player in each slot. */
    int defended[2][SLOTS];
    int hp_before[2];
    int ap_before[2];
    int energy[2];
} RoundReport;

const char *card_name(int card);
int card_cost(int card);
void game_init(Game *game);
int validate_plan(const Player *player, const Plan *plan,
                  char *error, int error_size);
int game_molt(Game *game, int player_index, int hand_position,
              char *error, int error_size);
int game_resolve(Game *game, const Plan plans[2], RoundReport *report,
                 char *error, int error_size);

#endif
