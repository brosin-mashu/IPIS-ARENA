#include "net.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The server owns hands, AP, drafts and the planning deadline. */
static int player_message(Socket peer, int p, const char *text) {
    if (p == 1) return net_info(peer, text);
    puts(text);
    fflush(stdout);
    return 0;
}

static int show_hand(Socket peer, const Game *game, int p) {
    char text[200];
    int i;
    const Player *player = &game->players[p];
    snprintf(text, sizeof(text), "Round %d | HP P1=%d P2=%d | Your AP=%d/2 | Energy=5",
             game->round, game->players[0].hp, game->players[1].hp, player->ap);
    if (player_message(peer, p, text) < 0) return -1;
    for (i = 0; i < player->hand_count; i++) {
        snprintf(text, sizeof(text), "[%d] %-19s cost %d%s", i + 1,
                 card_name(player->hand[i]), card_cost(player->hand[i]),
                 player->original[i] != player->hand[i] ? " (converted)" : "");
        if (player_message(peer, p, text) < 0) return -1;
    }
    return player_message(peer, p, "Choose hand positions 0-5; 0 skips. Backspace edits. U then position converts (2 AP). Enter locks. Q quits.");
}

static int show_draft(Socket peer, int p, const Player *player, const Planning *draft) {
    char text[160];
    int i, cost = 0;
    for (i = 0; i < SLOTS; i++)
        if (draft->plan.picks[i]) cost += card_cost(player->hand[draft->plan.picks[i] - 1]);
    snprintf(text, sizeof(text), "Selected: %d %d %d | Energy %d/5%s",
             draft->plan.picks[0], draft->plan.picks[1], draft->plan.picks[2], cost,
             draft->converting ? " | Convert which position? (U cancels)" : "");
    return player_message(peer, p, text);
}

/* Receive one player's edits, but continue detecting quit/disconnect from both.
   A frame begun outside P2's turn cannot become valid by finishing later. */
static int take_turn(Socket peer, Game *game, int p, Planning *draft,
                     Line *remote, int *frame_round, int *frame_allowed) {
    char text[200];
    double deadline;
    int last_seconds = TURN_SECONDS;
    keyboard_clear();
    snprintf(text, sizeof(text), "TURN %d %d %d\n", game->round, p + 1, TURN_SECONDS);
    if (net_send(peer, text) < 0) return -1;
    printf("\nRound %d: Player %d planning (%d seconds).\n", game->round, p + 1, TURN_SECONDS);
    if (p == 1) puts("Waiting for Player 2; their choices stay hidden.");
    fflush(stdout);
    if (show_hand(peer, game, p) < 0 || show_draft(peer, p, &game->players[p], draft) < 0)
        return -1;
    deadline = net_now() + TURN_SECONDS;
    for (;;) {
        int events, key = -1;
        int remaining = (int)(deadline - net_now() + 0.999);
        if (net_now() >= deadline) {
            if (player_message(peer, p, "Time expired. Current valid selections submitted; empty slots pass.") < 0)
                return -1;
            break;
        }
        if (remaining != last_seconds) {
            last_seconds = remaining;
            if (remaining % 5 == 0 || remaining <= 5) {
                snprintf(text, sizeof(text), "%d seconds remaining.", remaining);
                if (player_message(peer, p, text) < 0) return -1;
            }
        }
        events = net_wait(peer, 1);
        if (events < 0) return -1;
        /* Deadline wins even if a key/socket became ready on the final poll. */
        if (net_now() >= deadline) continue;
        if (events & PEER_READY) {
            char ch;
            int complete, values[2];
            if (net_byte(peer, &ch) <= 0) return -1;
            if (remote->used == 0 && !remote->invalid) {
                *frame_round = game->round;
                *frame_allowed = p == 1;
            }
            complete = line_add(remote, ch);
            if (complete > 0 && strcmp(remote->text, "QUIT") == 0) return -1;
            if (complete != 0) {
                if (complete < 0 || strncmp(remote->text, "KEY ", 4) != 0 ||
                    !parse_numbers(remote->text + 4, values, 2) || values[1] > 127) {
                    if (net_info(peer, "Invalid input message.") < 0) return -1;
                } else if (p != 1 || !*frame_allowed || *frame_round != game->round ||
                           values[0] != game->round) {
                    if (net_info(peer, "Not your active planning turn, or stale round.") < 0) return -1;
                } else key = values[1];
            }
        }
        if (events & KEYBOARD_READY) {
            char ch;
            if (keyboard_byte(&ch) <= 0) return -1;
            if (ch == 'q' || ch == 'Q' || ch == 3) return -1;
            if (p == 0) key = (unsigned char)ch;
            /* Ignore host typing while the client is planning. */
        }
        if (key >= 0) {
            int result = planning_key(game, p, draft, key, text, sizeof(text));
            if (result == 3) return -1;
            if (result == 1) break;
            if (result < 0 && player_message(peer, p, text) < 0) return -1;
            if (result == 2 && show_hand(peer, game, p) < 0) return -1;
            if (show_draft(peer, p, &game->players[p], draft) < 0) return -1;
        }
    }
    snprintf(text, sizeof(text), "LOCKED %d %d\n", game->round, p + 1);
    if (net_send(peer, text) < 0) return -1;
    printf("Player %d plan locked.\n", p + 1);
    fflush(stdout);
    return 0;
}

static int show_report(Socket peer, const Game *game, const RoundReport *r, int round) {
    char text[200];
    int slot;
    snprintf(text, sizeof(text), "Round %d result", round);
    puts(text);
    if (net_info(peer, text) < 0) return -1;
    for (slot = 0; slot < SLOTS; slot++) {
        snprintf(text, sizeof(text), "Slot %d: P1 %s / P2 %s | damage taken P1=%d P2=%d",
                 slot + 1, card_name(r->cards[0][slot]), card_name(r->cards[1][slot]),
                 r->damage[0][slot], r->damage[1][slot]);
        puts(text);
        if (net_info(peer, text) < 0) return -1;
    }
    snprintf(text, sizeof(text), "HP: P1 %d -> %d | P2 %d -> %d", r->hp_before[0],
             game->players[0].hp, r->hp_before[1], game->players[1].hp);
    puts(text);
    if (net_info(peer, text) < 0) return -1;
    snprintf(text, sizeof(text), "AP: P1 %d -> %d | P2 %d -> %d", r->ap_before[0],
             game->players[0].ap, r->ap_before[1], game->players[1].ap);
    puts(text);
    fflush(stdout);
    return net_info(peer, text);
}

int main(int argc, char *argv[]) {
    Socket listener = NO_SOCKET, peer = NO_SOCKET;
    Game game;
    Line remote = {{0}, 0, 0};
    int frame_round = 0, frame_allowed = 0, rounds = DEMO_ROUNDS, status = 1;
    char error[128];
    if ((argc != 2 && argc != 4) || !valid_port(argv[1])) {
        fprintf(stderr, "Usage: %s port [--rounds 1|2|3]\n", argv[0]);
        return 1;
    }
    if (argc == 4) {
        if (strcmp(argv[2], "--rounds") || strlen(argv[3]) != 1 ||
            argv[3][0] < '1' || argv[3][0] > '3') return 1;
        rounds = argv[3][0] - '0';
    }
    if (net_start() != 0) return 1;
    listener = net_listen(argv[1]);
    if (listener == NO_SOCKET) { puts("Could not listen on that port."); goto cleanup; }
    printf("IPIS ARENA | %d-round demo | 5 energy | 30-second planning\n", rounds);
    puts("You are Player 1 (server). Waiting for Player 2...");
    fflush(stdout);
    peer = net_accept(listener);
    if (peer == NO_SOCKET) goto cleanup;
    net_close(listener);
    listener = NO_SOCKET;
    if (keyboard_start() < 0) goto cancelled;
    if (net_send(peer, "IPIS-DEMO 2\n") < 0) goto cancelled;
    puts("Player 2 connected.");
    game_init(&game);
    while (game.outcome == ONGOING && game.round <= rounds) {
        Planning drafts[2] = {{{{0, 0, 0}}, 0, 0}, {{{0, 0, 0}}, 0, 0}};
        Plan plans[2];
        RoundReport report;
        int turn, round = game.round, first = (round - 1) % 2;
        for (turn = 0; turn < 2; turn++) {
            int p = (first + turn) % 2;
            if (take_turn(peer, &game, p, &drafts[p], &remote,
                          &frame_round, &frame_allowed) < 0) goto cancelled;
            plans[p] = drafts[p].plan;
        }
        if (!game_resolve(&game, plans, &report, error, sizeof(error))) goto cancelled;
        if (show_report(peer, &game, &report, round) < 0) goto cancelled;
    }
    {
        const char *ending = "Round limit reached. No winner is declared.";
        if (game.outcome == DRAW) ending = "Both players reached 0 HP: draw in this demo.";
        if (game.outcome == PLAYER_ONE_WINS) ending = "Player 1 wins this demo.";
        if (game.outcome == PLAYER_TWO_WINS) ending = "Player 2 wins this demo.";
        puts(ending);
        puts("demo round done");
        if (net_info(peer, ending) < 0 || net_send(peer, "INFO demo round done\nDONE\n") < 0)
            goto cancelled;
    }
    status = 0;
    goto cleanup;
cancelled:
    puts("Session cancelled: a player left or a connection failed. No winner awarded.");
    if (peer != NO_SOCKET) net_send(peer, "INFO Session cancelled. No winner awarded.\nABORT\n");
cleanup:
    keyboard_stop();
    if (peer != NO_SOCKET) net_close(peer);
    if (listener != NO_SOCKET) net_close(listener);
    net_stop();
    return status;
}

