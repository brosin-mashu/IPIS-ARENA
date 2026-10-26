#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char error[128];

static void check_inventory(const Player *player) {
    int counts[6] = {0};
    int i;
    assert(player->hand_count >= 0 && player->hand_count <= HAND_SIZE);
    assert(player->deck_count >= 0 && player->deck_count <= DECK_SIZE);
    assert(player->discard_count >= 0 && player->discard_count <= DECK_SIZE);
    assert(player->hand_count + player->deck_count + player->discard_count == DECK_SIZE);
    for (i = 0; i < player->hand_count; i++) counts[player->hand[i]]++;
    for (i = 0; i < player->deck_count; i++) counts[player->deck[i]]++;
    for (i = 0; i < player->discard_count; i++) counts[player->discard[i]]++;
    for (i = 1; i <= 5; i++) assert(counts[i] == 3);
}

static void test_all_interactions(void) {
    /* Expected damage to the opposing player: row attacks column.
       Includes the damage from Dulag's successful counterattack. */
    const int expected[6][6] = {
        {0, 0, 0, 0, 0, 0},
        {5, 5, 5, 0, 5, 5},
        {10, 10, 10, 0, 0, 10},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 5, 0, 0, 0},
        {5, 5, 5, 5, 5, 5}
    };
    int first, second;
    for (first = EMPTY; first <= ULTI_KAGAT; first++) {
        for (second = EMPTY; second <= ULTI_KAGAT; second++) {
            Game game;
            RoundReport report;
            Plan plans[2] = {{{0, 0, 0}}, {{0, 0, 0}}};
            int defended_first, defended_second;
            game_init(&game);
            if (first) { game.players[0].hand[0] = first; plans[0].picks[0] = 1; }
            if (second) { game.players[1].hand[0] = second; plans[1].picks[0] = 1; }
            assert(game_resolve(&game, plans, &report, error, sizeof(error)));
            assert(game.players[0].hp == START_HP - expected[second][first]);
            assert(game.players[1].hp == START_HP - expected[first][second]);
            defended_first = (first == SAGANG && (second == KAGAT || second == LAYOG_KAGAT)) ||
                             (first == DULAG_SAPAK && second == LAYOG_KAGAT);
            defended_second = (second == SAGANG && (first == KAGAT || first == LAYOG_KAGAT)) ||
                              (second == DULAG_SAPAK && first == LAYOG_KAGAT);
            assert(game.players[0].ap == defended_first);
            assert(game.players[1].ap == defended_second);
        }
    }
    puts("PASS: all 36 slot pairings, damage, counters and defensive AP");
}

static void test_validation(void) {
    Game game, before;
    RoundReport report;
    Plan plans[2] = {{{1, 3, 0}}, {{1, 1, 0}}};
    Plan expensive = {{2, 5, 1}};
    Plan invalid = {{-1, 0, 0}};
    Plan outside = {{6, 0, 0}};
    Plan maximum = {{2, 1, 3}};
    game_init(&game);
    before = game;
    assert(!game_resolve(&game, plans, &report, error, sizeof(error)));
    assert(memcmp(&before, &game, sizeof(game)) == 0);
    assert(!validate_plan(&game.players[0], &expensive, error, sizeof(error)));
    assert(!validate_plan(&game.players[0], &invalid, error, sizeof(error)));
    assert(!validate_plan(&game.players[0], &outside, error, sizeof(error)));
    assert(validate_plan(&game.players[0], &maximum, error, sizeof(error)));
    puts("PASS: energy, duplicate positions, range checks and atomic rejection");
}

static void test_molt_and_recycling(void) {
    Game game, before;
    RoundReport report;
    Plan plans[2] = {{{1, 0, 0}}, {{1, 0, 0}}};
    int round, p;
    game_init(&game);
    assert(memcmp(&game.players[0], &game.players[1], sizeof(Player)) == 0);
    before = game;
    assert(!game_molt(&game, 0, 1, error, sizeof(error)));
    assert(memcmp(&before, &game, sizeof(game)) == 0);
    game.players[0].ap = 2;
    before = game;
    assert(!game_molt(&game, 0, 6, error, sizeof(error)));
    assert(memcmp(&before, &game, sizeof(game)) == 0);
    assert(game_molt(&game, 0, 1, error, sizeof(error)));
    assert(game.players[0].ap == 0);
    assert(game.players[0].hand[0] == LAYOG_KAGAT);
    assert(game.players[0].hand[4] == KAGAT);
    for (round = 0; round < 300; round++) {
        /* Reset HP only in this stress test so many deck cycles can run. */
        for (p = 0; p < 2; p++) game.players[p].hp = START_HP;
        assert(game_resolve(&game, plans, &report, error, sizeof(error)));
        for (p = 0; p < 2; p++) check_inventory(&game.players[p]);
    }
    puts("PASS: identical starting decks, Molt, refills and 300 rounds of card conservation");
}

static void test_outcomes_and_rounds(void) {
    Game game, before;
    RoundReport report;
    Plan plans[2] = {{{1, 0, 0}}, {{1, 0, 0}}};
    game_init(&game);
    game.players[0].hp = game.players[1].hp = 5;
    assert(game_resolve(&game, plans, &report, error, sizeof(error)));
    assert(game.outcome == DRAW);
    before = game;
    assert(!game_resolve(&game, plans, &report, error, sizeof(error)));
    assert(!game_molt(&game, 0, 1, error, sizeof(error)));
    assert(memcmp(&before, &game, sizeof(game)) == 0);
    game_init(&game);
    game.players[1].hp = 3;
    plans[1].picks[0] = 0;
    assert(game_resolve(&game, plans, &report, error, sizeof(error)));
    assert(game.outcome == PLAYER_ONE_WINS && game.players[1].hp == 0);
    game_init(&game);
    game.players[0].hp = 3;
    plans[0].picks[0] = 0;
    plans[1].picks[0] = 1;
    assert(game_resolve(&game, plans, &report, error, sizeof(error)));
    assert(game.outcome == PLAYER_TWO_WINS && game.players[0].hp == 0);
    game_init(&game);
    plans[0].picks[0] = 0;
    plans[1].picks[0] = 0;
    assert(game_resolve(&game, plans, &report, error, sizeof(error)));
    assert(game.round == 2 && game.outcome == ONGOING);
    assert(game.players[0].hp == START_HP);
    /* Both committed sequences finish even if the first slot is lethal. */
    game_init(&game);
    game.players[0].hp = game.players[1].hp = 5;
    plans[0] = (Plan){{1, 0, 0}};
    plans[1] = (Plan){{0, 1, 0}};
    assert(game_resolve(&game, plans, &report, error, sizeof(error)));
    assert(game.outcome == DRAW);
    puts("PASS: both winners, simultaneous KO, round advancement and ended-match rejection");
}

static void test_ap_cap_and_slots(void) {
    Game game;
    RoundReport report;
    Plan plans[2] = {{{1, 2, 3}}, {{1, 2, 3}}};
    int i;
    game_init(&game);
    for (i = 0; i < 3; i++) {
        game.players[0].hand[i] = SAGANG;
        game.players[1].hand[i] = KAGAT;
    }
    assert(game_resolve(&game, plans, &report, error, sizeof(error)));
    assert(game.players[0].ap == MAX_AP);
    assert(game.players[0].hp == START_HP);
    for (i = 0; i < 3; i++) assert(report.defended[0][i]);
    puts("PASS: three-slot resolution and AP cap");
}

int main(void) {
    test_all_interactions();
    test_validation();
    test_molt_and_recycling();
    test_outcomes_and_rounds();
    test_ap_cap_and_slots();
    puts("All combat engine checks passed.");
    return 0;
}
