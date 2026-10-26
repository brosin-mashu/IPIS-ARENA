# Cock Fight: Ipis Edition - local combat demo

This milestone implements the game rules independently of sockets. Two people
share one terminal and submit three-slot plans. The engine calculates the
interactions, damage, AP, replacement cards, and match outcome.

## Run on Ubuntu

Open a terminal in this folder:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic main.c game.c -o ipis_logic
./ipis_logic
```

No ports, server, clients, or extra terminals are required for this milestone.
If GCC is missing, install it with `sudo apt install build-essential`.

For a repeatable presentation that uses the actual engine:

```sh
./ipis_logic --demo
```

This mode supplies predetermined player decisions. It still calculates all
results through `game_resolve`; it does not print hardcoded HP or winners.
Use interactive play as the main evidence that choices affect the match.

## Play

1. Read the rules and hand. The other player looks away during planning.
2. Enter three hand positions, such as `1 3 0`, then press Enter.
3. The numbers refer to the CURRENT hand positions, not fixed card IDs.
4. `0` leaves that slot empty. You may pass with `0 0 0`.
5. A hand position can be used once per plan. Different positions holding the
   same card type can both be used. The total energy cost must not exceed 4.
6. Pass the keyboard. After both plans are locked, all three slots resolve.
7. The screen shows both cards, damage taken, HP changes and AP changes.
8. Play continues with refilled hands until a win or draw. A completed match
   can be restarted from the prompt; `q` or end-of-input exits without a winner.

At an interactive terminal, input is not echoed. Screen clearing separates
planning phases but does not erase scrollback. This is an honor-based local
demonstration, not secure private multiplayer. Redirected input remains usable
for verification. On Ubuntu while hidden input is waiting, use `q` then Enter
to quit; Ctrl+C may require Enter because terminal signals are temporarily
disabled to prevent leaving echo turned off.

## Implemented from the proposal

- Both players start with 50 HP and identical 15-card decks (three of each type).
- Five-card starting hands; fixed draw order; unused cards stay in hand.
- Four energy per round, up to three selected cards, and corresponding slots.
- Kagat: 1 energy, 5 damage; blocked by Sagang, not dodged by Dulag.
- Layog+Kagat: 2 energy, 10 damage; blockable or dodgeable/counterable.
- Sagang: 1 energy; blocks Kagat and Layog+Kagat.
- Dulag sabay sapak: 1 energy; dodges Layog+Kagat and counters for 5 damage.
- Ulti kagat: 2 energy; deals 5 damage through either defense.
- A successful block/dodge grants 1 AP, capped at 2.
- Molt spends 2 AP to discard one hand card and draw a replacement.
- Played cards are discarded; hands refill to five after ongoing rounds.
- HP never displays below zero. One KO gives a winner; two KOs in the same
  round give a draw. Invalid plans do not change the battle state.

## Explicit demo decisions where the proposal is incomplete

These are implementation choices to review with the group, not additional
professor-approved requirements:

| Question | Current decision |
| --- | --- |
| The Dulag description calls its target "Wing Lunge" | Treat that as Layog+Kagat, the listed 10-damage attack. |
| Exact predetermined deck order | Kagat, Layog+Kagat, Sagang, Dulag, Ulti; repeated three times. |
| What happens when the draw deck runs out? | Recycle the discard pile in its existing order; never shuffle. |
| When is Molt available? | Before committing a plan, at most once per player per round; no energy cost. |
| What happens to later slots after lethal damage? | Resolve all committed slots, then apply round damage and judge the outcome together. |
| Must players fill all slots? | No; empty slots can appear anywhere, including an all-pass plan. |

Two players repeatedly choosing no effective attacks can continue indefinitely.
No turn timer or forced stalemate rule has been added.

## How the source is organized

| File | Responsibility |
| --- | --- |
| `game.h` | Game state, player state, card IDs, plans and function declarations. |
| `game.c` | Rules and state changes. No keyboard input, terminal output or sockets. |
| `main.c` | Local keyboard input, private planning handoff and result display. |
| `test_game.c` | Interaction matrix, validation, state and deck checks. |
| `test_cli.py` | Optional end-to-end input and presentation checks (Python 3). |
| `PRESENTATION.md` | Short live-demo sequence and explanation for the professor. |

`game_init` creates a match. `validate_plan` checks a player's selection.
`game_molt` changes one card at the cost of AP. `game_resolve` validates BOTH
plans before changing state, then returns a report for the interface to display.

Later, the networking layer can obtain plans from sockets and call the same
functions. The local keyboard interface is simply replaced as the input source.
The eventual one-server/one-client arrangement can let the server user play P1
and the client user play P2; this demo does not implement that transport yet.

## Verification

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic test_game.c game.c -o test_game
./test_game
```

Or use `make` and `make test` on Ubuntu.

If Python 3 is available, also run `python3 test_cli.py ./ipis_logic` to check
the actual keyboard-input flow, a complete match, and restarting the match.

The engine checks cover all 36 card/empty-slot pairs, counters, defensive AP,
energy limits, duplicate hand selection, invalid-plan atomicity, Molt, AP cap,
both winners, same-round KO, refills and 300 rounds of card conservation.

The sources were compiled with MinGW on Windows using C11, Wall, Wextra,
Wpedantic and Werror. Automated terminal-input scenarios and the guided demo
were run there. Linux execution and actual Ubuntu terminal privacy behavior
still need verification on your Ubuntu before the presentation.

This is a local game-logic milestone. It does not claim a measured completion
percentage or predict the grade. Network integration, two-machine testing,
disconnect handling and final terminal presentation remain separate work.
