# Verification record

Date: 26 October 2026.

Environment: Windows, MinGW GCC. The same source includes a POSIX terminal
input branch for Ubuntu, but Ubuntu execution has not been verified here.

Both C targets compiled successfully with:

```text
-std=c11 -Wall -Wextra -Wpedantic -Werror
```

Engine checks passed:

- All 36 pairs of five card types plus an empty slot, including damage,
  blocked attacks, dodge/counter, piercing and correct AP awards.
- Four-energy boundary, duplicate hand positions, invalid ranges and
  rejection without changing game state when either plan is invalid.
- Identical initial decks, Molt with/without AP, invalid Molt positions,
  refilling and conservation of three copies of each card over 300 rounds.
- P1 victory, P2 victory, same-round draw, zero-HP clamping, round advancement
  and preventing actions after a completed match.
- All three slots resolve; successful defense cannot raise AP above two.

End-to-end input checks passed:

- Start/help/exit menu, returning from help and invalid menu input.
- Compact and spaced plans produce identical results; malformed compact plans,
  repeated positions and excess energy are rejected; `000` passes correctly.

- The exact two-round presentation sequence, including rejected selections,
  earning two AP, spending AP on Molt and updated HP.
- Oversized lines, malformed numbers, extra numbers, quit and end-of-input.
- A complete interactive match with a winner, followed by a fresh match.
- The calculated six-round guided demonstration and the rules command.

The guided demonstration ended with P1 at 40 HP and P2 at 0 HP. These are
calculated results, not hardcoded output values.

Before presenting: compile in Ubuntu, run `./test_game`, rehearse the
interactive sequence in PRESENTATION.md and check terminal handoff behavior.
