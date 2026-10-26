#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#define IS_TERMINAL() _isatty(_fileno(stdin))
#else
#include <unistd.h>
#include <termios.h>
#define IS_TERMINAL() isatty(STDIN_FILENO)
#endif

static void show_rules(void) {
    puts("\nCOCK FIGHT: IPIS EDITION -- local game-logic demo");
    puts("50 HP | 4 energy each round | 5 cards in hand | 3 combat slots");
    puts("\nCard                 Energy  Effect");
    puts("Kagat                   1    5 damage; cannot be dodged.");
    puts("Layog+Kagat             2    10 damage; blockable and counterable.");
    puts("Sagang                  1    Blocks Kagat and Layog+Kagat.");
    puts("Dulag sabay sapak       1    Dodges Layog+Kagat; counters for 5.");
    puts("Ulti kagat              2    5 damage through any defense.");
    puts("\nSuccessful block/dodge: +1 AP (maximum 2).");
    puts("Molt: spend 2 AP before planning to replace one hand card.");
    puts("\nPLANNING");
    puts("Enter three hand positions: 130 or 1 3 0. Zero leaves a slot empty.");
    puts("A hand position can be used once per round; total cost must be <= 4.");
    puts("Both plans resolve together, one corresponding slot at a time.");
    puts("Played cards are discarded; unused cards stay. Refill to five.");
    puts("Demo policy: recycle discards in order when the draw deck is empty.");
    puts("All three slots resolve before checking a win or simultaneous KO.");
    puts("\nLocal play: pass the keyboard and look away during the other plan.");
    puts("Plans are not echoed at an interactive terminal, but terminal history");
    puts("and a shared screen are not secure multiplayer privacy.");
    puts("Type q at any input to end the demo. No winner is awarded for quitting.");
}

/* Read and drain a complete line; oversized input is rejected, not split. */
static int read_line(char *line, int size) {
    int ch, used = 0, overflow = 0;
#ifdef _WIN32
    int hidden = IS_TERMINAL();
#else
    struct termios old_settings, settings;
    int hidden = IS_TERMINAL() && tcgetattr(STDIN_FILENO, &old_settings) == 0;
    if (hidden) {
        settings = old_settings;
        /* Keep canonical editing. Disable terminal-generated signals while
           echo is off so Ctrl+C cannot leave the terminal in a hidden state.
           A literal Ctrl+C is handled below after settings are restored. */
        settings.c_lflag &= (tcflag_t)~(ECHO | ISIG);
        if (tcsetattr(STDIN_FILENO, TCSANOW, &settings) != 0) hidden = 0;
    }
#endif
    for (;;) {
#ifdef _WIN32
        ch = hidden ? _getch() : getchar();
        if (hidden && (ch == 0 || ch == 224)) { (void)_getch(); continue; }
        if (hidden && ch == '\b') { if (used > 0) used--; continue; }
        if (hidden && ch == 3) { ch = EOF; break; }
#else
        ch = getchar();
#endif
        if (ch == EOF || ch == '\n' || ch == '\r') break;
        if (ch == 3 || ch == 4) { ch = EOF; break; }
        if (used < size - 1) line[used++] = (char)ch;
        else overflow = 1;
    }
#ifndef _WIN32
    if (hidden) tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
#endif
    if (hidden) putchar('\n');
    line[used] = '\0';
    if (ch == EOF) return 0;
    if (overflow) return -1;
    return 1;
}

static int is_quit(const char *line) {
    while (isspace((unsigned char)*line)) line++;
    if (*line != 'q' && *line != 'Q') return 0;
    line++;
    while (isspace((unsigned char)*line)) line++;
    return *line == '\0';
}

/* Strict bounded parsing: reject '1abc', extra numbers, and huge integers. */
static int parse_numbers(const char *line, int values[], int count, int max) {
    int i;
    for (i = 0; i < count; i++) {
        int value = 0;
        while (isspace((unsigned char)*line)) line++;
        if (!isdigit((unsigned char)*line)) return 0;
        while (isdigit((unsigned char)*line)) {
            value = value * 10 + (*line++ - '0');
            if (value > max) return 0;
        }
        if (*line && !isspace((unsigned char)*line)) return 0;
        values[i] = value;
    }
    while (isspace((unsigned char)*line)) line++;
    return *line == '\0';
}

static int prompt(const char *label, char *line, int size) {
    int result;
    printf("%s", label);
    fflush(stdout);
    result = read_line(line, size);
    if (result == 0 || (result == 1 && is_quit(line))) return 0;
    if (result < 0) puts("Input is too long. Try again.");
    return result;
}

static int parse_plan(const char *line, Plan *plan) {
    int slot = 0;
    while (*line) {
        if (isspace((unsigned char)*line)) { line++; continue; }
        if (*line < '0' || *line > '5' || slot == SLOTS) return 0;
        plan->picks[slot++] = *line++ - '0';
    }
    return slot == SLOTS;
}

static int start_menu(void) {
    char line[128];
    int choice[1], result;
    for (;;) {
        puts("\nIPIS ARENA\n");
        puts("1. Start match");
        puts("2. How to play");
        puts("3. Exit\n");
        result = prompt("Choose 1-3: ", line, sizeof(line));
        if (result == 0) return 0;
        if (result < 0) continue;
        if (!parse_numbers(line, choice, 1, 3) || choice[0] == 0) {
            puts("Enter 1, 2 or 3.");
            continue;
        }
        if (choice[0] == 3) return 0;
        if (choice[0] == 1) {
            puts("\nChoose up to 3 cards. You have 4 energy per round.");
            puts("Reduce your opponent's HP to 0.");
            puts("\nEnter 123 or 1 2 3. Use 0 for an empty slot.");
            puts("Your choices stay hidden while typing. Press Enter to submit.");
            puts("Type q to leave.\n");
            return 1;
        }
        show_rules();
        if (prompt("\nPress Enter to return to the menu: ", line, sizeof(line)) == 0)
            return 0;
    }
}

static const char *card_effect(int card) {
    switch (card) {
        case KAGAT: return "5 damage; cannot be dodged";
        case LAYOG_KAGAT: return "10 damage; blockable/counterable";
        case SAGANG: return "Blocks normal attacks";
        case DULAG_SAPAK: return "Dodges Layog+Kagat; counters for 5";
        case ULTI_KAGAT: return "5 damage; bypasses defense";
        default: return "";
    }
}

static void show_hand(const Player *player) {
    int i;
    puts("\nYour hand (choose positions, not card type numbers):");
    for (i = 0; i < player->hand_count; i++)
        printf("  [%d] %-19s cost %d | %s\n", i + 1,
               card_name(player->hand[i]), card_cost(player->hand[i]),
               card_effect(player->hand[i]));
}

static void clear_screen(void) {
    if (IS_TERMINAL()) {
        /* Normal Ubuntu terminals support ANSI; this does not erase scrollback. */
        printf("\033[2J\033[H");
        fflush(stdout);
    }
}

static int collect_plan(Game *game, int p, Plan *plan) {
    char line[128], error[128];
    int answer[1], result;
    Player *player = &game->players[p];
    for (;;) {
        printf("\nPass the keyboard to Player %d. Other player, look away.\n", p + 1);
        result = prompt("Press Enter when ready (q to leave): ", line, sizeof(line));
        if (result == 0) return 0;
        if (result > 0) break;
    }
    clear_screen();
    printf("ROUND %d -- PLAYER %d PLANNING\n", game->round, p + 1);
    printf("HP: P1 %d | P2 %d   Your AP: %d/2   Energy: 4\n",
           game->players[0].hp, game->players[1].hp, player->ap);
    show_hand(player);
    if (player->ap == 2) {
        puts("\nMolt is available: spend 2 AP to replace one card, or keep your AP.");
        for (;;) {
            result = prompt("Molt: position to replace, or 0 to keep hand: ", line, sizeof(line));
            if (result == 0) return 0;
            if (result < 0) continue;
            if (!parse_numbers(line, answer, 1, HAND_SIZE)) {
                puts("Enter one hand position, or 0.");
                continue;
            }
            if (answer[0] == 0) break;
            if (!game_molt(game, p, answer[0], error, sizeof(error))) {
                puts(error);
                continue;
            }
            puts("Molt used: 2 AP spent. Your updated hand:");
            show_hand(player);
            break;
        }
    }
    for (;;) {
        result = prompt("\nThree slots (123 or 1 2 3; 0 = empty): ", line, sizeof(line));
        if (result == 0) return 0;
        if (result < 0) continue;
        if (!parse_plan(line, plan)) {
            puts("Enter exactly three numbers from 0 to 5.");
            continue;
        }
        if (!validate_plan(player, plan, error, sizeof(error))) {
            puts(error);
            continue;
        }
        puts("Plan locked. It will be revealed after both players finish.");
        clear_screen();
        return 1;
    }
}

static void show_report(const Game *game, const RoundReport *report, int round) {
    int slot, p;
    printf("\n=== ROUND %d RESULTS ===\n", round);
    printf("Energy used: P1 %d/4 | P2 %d/4\n", report->energy[0], report->energy[1]);
    for (slot = 0; slot < SLOTS; slot++) {
        printf("\nSlot %d: P1 %-19s vs P2 %s\n", slot + 1,
               card_name(report->cards[0][slot]), card_name(report->cards[1][slot]));
        printf("  Damage taken: P1 %d | P2 %d\n",
               report->damage[0][slot], report->damage[1][slot]);
        for (p = 0; p < 2; p++) {
            if (report->defended[p][slot])
                printf("  P%d defended successfully (earns AP up to the cap of 2).\n", p + 1);
        }
    }
    for (p = 0; p < 2; p++)
        printf("P%d: HP %d -> %d | AP %d -> %d\n", p + 1,
               report->hp_before[p], game->players[p].hp,
               report->ap_before[p], game->players[p].ap);
    if (game->outcome == DRAW) puts("\nDRAW: both cockroaches reached 0 HP this round.");
    else if (game->outcome == PLAYER_ONE_WINS) puts("\nPLAYER 1 WINS!");
    else if (game->outcome == PLAYER_TWO_WINS) puts("\nPLAYER 2 WINS!");
}

/* Repeatable presentation: real engine calls, no prewritten combat results. */
static int guided_demo(void) {
    Game game;
    RoundReport report;
    char error[128];
    int round;
    Plan plans[2] = {{{2, 1, 0}}, {{4, 3, 0}}};
    game_init(&game);
    puts("GUIDED DEMO: calculated by the same engine as interactive play.");
    puts("Round 1: dodge/counter in slot 1, block in slot 2; P2 earns 2 AP.");
    if (!game_resolve(&game, plans, &report, error, sizeof(error))) return 1;
    show_report(&game, &report, 1);
    puts("\nP2 now spends 2 AP on Molt, replacing hand position 1.");
    if (!game_molt(&game, 1, 1, error, sizeof(error))) return 1;
    puts("Round 2: Ulti kagat bypasses Sagang; another pair of attacks trades damage.");
    plans[0] = (Plan){{3, 5, 0}};
    plans[1] = (Plan){{5, 2, 0}};
    if (!game_resolve(&game, plans, &report, error, sizeof(error))) return 1;
    show_report(&game, &report, 2);
    puts("\nThen P1 fills slots from the hand within 4 energy; P2 passes.");
    puts("This cooperative presentation script demonstrates refills and a win.");
    while (game.outcome == ONGOING && game.round < 100) {
        int i, slot = 0, energy = 0;
        memset(plans, 0, sizeof(plans));
        for (i = 0; i < game.players[0].hand_count && slot < SLOTS; i++) {
            int cost = card_cost(game.players[0].hand[i]);
            if (energy + cost <= ROUND_ENERGY) {
                plans[0].picks[slot++] = i + 1;
                energy += cost;
            }
        }
        round = game.round;
        if (!game_resolve(&game, plans, &report, error, sizeof(error))) return 1;
        show_report(&game, &report, round);
    }
    return game.outcome == ONGOING ? 1 : 0;
}

int main(int argc, char *argv[]) {
    Game game;
    Plan plans[2];
    RoundReport report;
    char line[128], error[128];
    if (argc == 2 && strcmp(argv[1], "--demo") == 0) return guided_demo();
    if (argc == 2 && strcmp(argv[1], "--rules") == 0) { show_rules(); return 0; }
    if (argc != 1) {
        fprintf(stderr, "Usage: %s [--demo | --rules]\n", argv[0]);
        return 1;
    }
    if (!start_menu()) return 0;
    for (;;) {
        game_init(&game);
        while (game.outcome == ONGOING) {
            int round = game.round;
            if (!collect_plan(&game, 0, &plans[0]) ||
                !collect_plan(&game, 1, &plans[1])) {
                puts("Demo ended. No winner awarded.");
                return 0;
            }
            if (!game_resolve(&game, plans, &report, error, sizeof(error))) {
                fprintf(stderr, "Could not resolve round: %s\n", error);
                return 1;
            }
            show_report(&game, &report, round);
        }
        for (;;) {
            int result = prompt("\nNew match? 1 = yes, 0 = exit: ", line, sizeof(line));
            int answer[1];
            if (result == 0) return 0;
            if (result < 0) continue;
            if (!parse_numbers(line, answer, 1, 1)) { puts("Enter 1 or 0."); continue; }
            if (answer[0] == 0) return 0;
            break;
        }
    }
}
