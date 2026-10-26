# Wednesday game-logic demonstration

## What to say first

"Our earlier prototype focused on sockets and message exchange. For this
milestone, we separated the game rules from networking. This local prototype
accepts both players' card sequences and calculates combat, resources and the
match outcome. The combat engine can later be called by the server."

## Live demonstration: about 4-5 minutes

Compile and start interactive play:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic main.c game.c -o ipis_logic
./ipis_logic
```

Choose **1. Start match** from the opening menu. Use **2. How to play** if
you want to show the full rules first. Sequences accept both `210` and `2 1 0`.

The first hand for both players is always:

| Position | Card | Energy |
| --- | --- | --- |
| 1 | Kagat | 1 |
| 2 | Layog+Kagat | 2 |
| 3 | Sagang | 1 |
| 4 | Dulag sabay sapak | 1 |
| 5 | Ulti kagat | 2 |

1. Press Enter for Player 1's handoff. Try `1 1 0`: the program rejects using
   the same hand position twice. HP and the round do not change.
2. Try `2 5 1`: it costs five energy, so it is rejected.
3. Enter P1's valid plan `2 1 0` (three energy).
4. Press Enter for P2's handoff. Enter `4 3 0` (two energy).
5. Explain the results: slot 1 dodges Layog+Kagat and counters P1 for five;
   slot 2 blocks Kagat. P1 is at 45 HP, P2 at 50 HP, and P2 has 2 AP.
6. Press Enter for P1's next handoff. Enter `3 5 0` (four energy).
7. Press Enter for P2's handoff. At the Molt prompt enter `1`. This spends
   2 AP and replaces a Kagat. The new hand is displayed. Enter `5 2 0`.
8. Explain round 2: P1's Ulti bypasses Sagang in slot 1. In slot 2,
   Layog+Kagat and Ulti both land. P1 ends at 40 HP, P2 at 35 HP; P2 has 0 AP.
9. Type `q` at the next handoff to finish the interactive portion.

Input is intentionally hidden while typing at an interactive terminal. Read
the planned choices aloud to the professor during this demonstration.

Then run the short repeatable match:

```sh
./ipis_logic --demo
```

It repeats those opening interactions, then gives P1 a cooperative opponent
so the engine can reach a winner quickly. Say clearly that the choices are
scripted for presentation; the HP and winner are calculated by the engine.

## Show the separation in code

- Open `game.h` and explain HP, AP, hand, deck and three-slot plans.
- Open `validate_plan` in `game.c`: duplicate positions and energy checks.
- Open `resolve_slot`: attack, block, dodge/counter and piercing rules.
- Open `game_resolve`: both plans validate before mutation; slots resolve;
  HP, AP, outcome and refills are updated.
- Explain that the game engine does not call `send`, `recv`, `printf` or `fgets`.
  Sockets will supply the plans and transport the results in a later step.

## Optional verification evidence

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic test_game.c game.c -o test_game
./test_game
```

## What remains

Network integration and a reliable two-machine match, review of the explicit
demo decisions in README.md, and final interface improvements. This milestone
shows working game logic; it does not claim that the full networked project is
complete or guarantee a particular grade.

Before Wednesday: compile and run the live sequence on the Ubuntu machine you
will present from. Rehearse the hand positions because they change after play.
