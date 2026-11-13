#ifndef IPIS_GAME_H
#define IPIS_GAME_H

#define HAND_SIZE 5
#define NORMAL_CARD_TYPES 4
#define COPIES_PER_CARD 4
#define DECK_SIZE (NORMAL_CARD_TYPES * COPIES_PER_CARD)
#define SLOTS 3
#define START_HP 50
#define ROUND_ENERGY 5
#define DEMO_ROUNDS 3
#define TURN_SECONDS 30
#define MAX_AP 2

enum Card { EMPTY, KAGAT, LAYOG_KAGAT, SAGANG, DULAG_SAPAK, ULTI_KAGAT };
enum Outcome { ONGOING, PLAYER_ONE_WINS, PLAYER_TWO_WINS, DRAW };

typedef struct {
    int hp;
    int ap;
    int hand[HAND_SIZE];
    int hand_count;
    int original[HAND_SIZE]; /* Original type of each hand card. */
    int converted_round;
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
typedef struct { Plan plan; int count; int converting; } Planning;

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
int game_convert(Game *game, int player_index, int hand_position,
              char *error, int error_size);
int game_resolve(Game *game, const Plan plans[2], RoundReport *report,
                 char *error, int error_size);

int parse_numbers(const char *text, int values[], int count);
/* 1 locks, 2 converts, 3 quits, 0 edits, -1 rejects. Server owns this state. */
int planning_key(Game *game, int player, Planning *draft, int key,
                 char *error, int size);
#endif
