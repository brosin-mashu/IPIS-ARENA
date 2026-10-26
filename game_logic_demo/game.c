#include "game.h"
#include <stdio.h>
#include <string.h>

static int fail(char *error, int size, const char *message) {
    if (size > 0) snprintf(error, (size_t)size, "%s", message);
    return 0;
}

const char *card_name(int card) {
    switch (card) {
        case EMPTY: return "Empty";
        case KAGAT: return "Kagat";
        case LAYOG_KAGAT: return "Layog+Kagat";
        case SAGANG: return "Sagang";
        case DULAG_SAPAK: return "Dulag sabay sapak";
        case ULTI_KAGAT: return "Ulti kagat";
        default: return "Unknown";
    }
}

int card_cost(int card) {
    if (card == LAYOG_KAGAT || card == ULTI_KAGAT) return 2;
    if (card >= KAGAT && card <= ULTI_KAGAT) return 1;
    return 0;
}

/* FIFO recycling is a demo policy: no shuffle, identical initial draw order. */
static int draw_card(Player *player) {
    int card, i;
    if (player->deck_count == 0) {
        for (i = 0; i < player->discard_count; i++)
            player->deck[i] = player->discard[i];
        player->deck_count = player->discard_count;
        player->discard_count = 0;
    }
    if (player->deck_count == 0) return EMPTY;
    card = player->deck[0];
    for (i = 1; i < player->deck_count; i++)
        player->deck[i - 1] = player->deck[i];
    player->deck_count--;
    return card;
}

static void refill_hand(Player *player) {
    while (player->hand_count < HAND_SIZE) {
        int card = draw_card(player);
        if (card == EMPTY) break;
        player->hand[player->hand_count++] = card;
    }
}

void game_init(Game *game) {
    int p, i;
    memset(game, 0, sizeof(*game));
    game->round = 1;
    for (p = 0; p < 2; p++) {
        Player *player = &game->players[p];
        player->hp = START_HP;
        player->deck_count = DECK_SIZE;
        for (i = 0; i < DECK_SIZE; i++)
            player->deck[i] = KAGAT + i % 5;
        refill_hand(player);
    }
}

int validate_plan(const Player *player, const Plan *plan,
                  char *error, int error_size) {
    int used[HAND_SIZE] = {0};
    int energy = 0, slot;
    for (slot = 0; slot < SLOTS; slot++) {
        int pick = plan->picks[slot];
        if (pick == 0) continue;
        if (pick < 1 || pick > player->hand_count)
            return fail(error, error_size, "Choose an existing hand position or 0.");
        if (used[pick - 1])
            return fail(error, error_size, "The same hand position cannot be used twice.");
        used[pick - 1] = 1;
        energy += card_cost(player->hand[pick - 1]);
    }
    if (energy > ROUND_ENERGY)
        return fail(error, error_size, "That plan costs more than 4 energy.");
    return 1;
}

int game_molt(Game *game, int player_index, int hand_position,
              char *error, int error_size) {
    Player *player;
    int i;
    if (game->outcome != ONGOING)
        return fail(error, error_size, "The match has ended.");
    if (player_index < 0 || player_index > 1)
        return fail(error, error_size, "Invalid player.");
    player = &game->players[player_index];
    if (player->ap < 2)
        return fail(error, error_size, "Molt requires 2 AP.");
    if (hand_position < 1 || hand_position > player->hand_count)
        return fail(error, error_size, "Choose an existing hand position.");
    player->discard[player->discard_count++] = player->hand[hand_position - 1];
    for (i = hand_position; i < player->hand_count; i++)
        player->hand[i - 1] = player->hand[i];
    player->hand_count--;
    player->ap -= 2;
    refill_hand(player);
    return 1;
}

/* Each slot is evaluated from BOTH cards, before either HP value is changed. */
static void resolve_slot(int first, int second, int damage[2], int defended[2]) {
    int cards[2] = {first, second};
    int attacker;
    damage[0] = damage[1] = 0;
    defended[0] = defended[1] = 0;
    for (attacker = 0; attacker < 2; attacker++) {
        int defender = 1 - attacker;
        int attack = cards[attacker];
        int defense = cards[defender];
        if (attack == ULTI_KAGAT) {
            damage[defender] += 5;
        } else if (attack == KAGAT || attack == LAYOG_KAGAT) {
            if (defense == SAGANG) {
                defended[defender] = 1;
            } else if (attack == LAYOG_KAGAT && defense == DULAG_SAPAK) {
                defended[defender] = 1;
                damage[attacker] += 5;
            } else {
                damage[defender] += attack == KAGAT ? 5 : 10;
            }
        }
    }
}

int game_resolve(Game *game, const Plan plans[2], RoundReport *report,
                 char *error, int error_size) {
    int p, slot;
    int total_damage[2] = {0, 0};
    if (game->outcome != ONGOING)
        return fail(error, error_size, "The match has ended.");
    /* Validate both plans first. An invalid plan must not mutate the match. */
    for (p = 0; p < 2; p++) {
        if (!validate_plan(&game->players[p], &plans[p], error, error_size))
            return 0;
    }
    memset(report, 0, sizeof(*report));
    for (p = 0; p < 2; p++) {
        Player *player = &game->players[p];
        int selected[HAND_SIZE] = {0};
        int kept = 0, i;
        report->hp_before[p] = player->hp;
        report->ap_before[p] = player->ap;
        /* Save card identities before compacting the remaining hand. */
        for (slot = 0; slot < SLOTS; slot++) {
            int pick = plans[p].picks[slot];
            int card = pick ? player->hand[pick - 1] : EMPTY;
            report->cards[p][slot] = card;
            report->energy[p] += card_cost(card);
            if (pick) {
                selected[pick - 1] = 1;
                player->discard[player->discard_count++] = card;
            }
        }
        for (i = 0; i < player->hand_count; i++) {
            if (!selected[i]) player->hand[kept++] = player->hand[i];
        }
        player->hand_count = kept;
    }
    for (slot = 0; slot < SLOTS; slot++) {
        int damage[2], defended[2];
        resolve_slot(report->cards[0][slot], report->cards[1][slot], damage, defended);
        for (p = 0; p < 2; p++) {
            report->damage[p][slot] = damage[p];
            report->defended[p][slot] = defended[p];
            total_damage[p] += damage[p];
            if (defended[p] && game->players[p].ap < MAX_AP)
                game->players[p].ap++;
        }
    }
    /* Resolve all committed slots, then judge the round together. */
    for (p = 0; p < 2; p++) {
        game->players[p].hp -= total_damage[p];
        if (game->players[p].hp < 0) game->players[p].hp = 0;
    }
    if (game->players[0].hp == 0 && game->players[1].hp == 0)
        game->outcome = DRAW;
    else if (game->players[0].hp == 0) game->outcome = PLAYER_TWO_WINS;
    else if (game->players[1].hp == 0) game->outcome = PLAYER_ONE_WINS;
    else {
        for (p = 0; p < 2; p++) refill_hand(&game->players[p]);
        game->round++;
    }
    return 1;
}
