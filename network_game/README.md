# IPIS Arena: revised network demo

The server is Player 1 and the client is Player 2. This version integrates
hands, decks and AP with sockets and the balancing rules approved November 8.
It remains a one-to-three-round progress demo, not a full match/lobby release.
The older game_logic_demo folder is a historical, separate version.

## Build on Ubuntu

Run inside network_game:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic server.c net.c game.c -o server
gcc -std=c11 -Wall -Wextra -Wpedantic client.c net.c game.c -o client
```

Or use `make`. Do not compile vendor/beej files into the game.
On Windows GCC, add `-lws2_32` to each networking build.
Both computers must use this version; the protocol is now IPIS-DEMO 2.

## Run on two computers

Host (Player 1):

```sh
./server 5001
```

Client (Player 2), using the host's actual IP:

```sh
./client 192.168.1.10 5001
```

For two terminals on one computer, use 127.0.0.1. Use `hostname -I` on Ubuntu
to find the host's address for a second computer. Both must reach port 5001.
For a one-round demonstration, use `./server 5001 --rounds 1`.
The default remains three rounds; the supported limit is 1, 2, or 3.
At the limit both programs print `demo round done`, with no winner if both live.

## Approved rules

- 50 HP, five energy per round, and three combat slots.
- Each player has 16 cards: four each of Kagat, Layog+Kagat, Sagang and Dulag.
  Start with five in hand. Ulti is never drawn; it is available only through AP conversion.
- Players plan one at a time. P1 starts odd rounds; P2 starts even rounds.
- Plans remain hidden from the opponent until both are locked.
- Matching slots resolve together. All committed slots resolve before KO checks.
- Successful blocking or dodging earns one AP, capped at two. Idle defense earns none.
- Spend two AP to convert one non-Ulti hand card to Ulti, once per round.
- A converted card remains Ulti while held. After play, its original type is
  discarded, so conversion does not permanently alter the deck.
- Converted Ulti still costs two energy, deals five damage through defense,
  and uses a slot. There is no Molt/draw replacement ability in this version.
- Unplayed cards stay. Refill to five after a continuing round. Empty decks
  recycle discards in order. Draw order is deterministic, without shuffling.

| Card | Energy | Effect |
| --- | --- | --- |
| Kagat | 1 | 5 damage; blocked by Sagang, cannot be dodged |
| Layog+Kagat | 2 | 10 damage; blocked by Sagang or countered by Dulag |
| Sagang | 1 | Blocks Kagat/Layog; does not deal damage |
| Dulag sabay sapak | 1 | Dodges Layog and counters for 5; cannot dodge Kagat |
| Ulti kagat | 2 | 5 damage through defense |

## Controls and timer

Choose HAND POSITIONS, not permanent card IDs. Positions change after refills.

- Press 0-5 to fill the next slot. Zero deliberately leaves a slot empty.
- Each digit registers immediately and prints the current valid selection.
- Spaces are optional. `210` and `2 1 0` have the same meaning.
- Backspace removes the last selection.
- Press U, then a position 1-5 to convert that hand card. U again cancels the
  conversion prompt. A successful conversion spends AP immediately and cannot
  be undone; removing the card from the plan does not refund AP.
- Enter locks the plan early. Unfilled slots remain empty.
- Q exits, including while waiting. Ctrl+C also exits and restores terminal mode.

Each player gets 30 seconds for their entire planning turn, including conversion.
The server enforces a monotonic deadline. Remaining-time notices appear at 20,
10, and the last five seconds. Invalid edits do not restart the clock.
At timeout, the latest server-accepted valid selections are submitted automatically.
No selection means passing. An unfinished conversion prompt spends no AP.
A conversion that would make the current plan cost more than five energy is
rejected without spending AP. Remove a selection first and try again.

Only active-turn input is accepted. The client sends key events labelled with
the round. Late or wrong-round events cannot edit a locked plan or spend AP.
Characters typed while waiting are ignored. Plans and hands are only displayed
on their owner's terminal; card choices are revealed in the common round report.
This is game-interface privacy, not protection against a host inspecting process memory.

## Quick two-round demonstration

Starting positions: 1 Kagat, 2 Layog, 3 Sagang, 4 Dulag, 5 Kagat.
The fixed draw order gives both players two Kagats initially; there is no shuffle.

1. P1 enters 210 and Enter; P2 then enters 430 and Enter.
2. P1 takes five counter damage; P2 earns two AP. HP is 45/50.
3. P2 plans first in round 2. Press U then 1: their remaining Kagat converts.
4. P2 enters 100 and Enter. P1 enters 100 and Enter (their first card is Sagang).
5. The converted Ulti bypasses Sagang. HP is 40/50, and P2 spent their AP.
6. To show timeout, select one card and wait without pressing Enter.


```