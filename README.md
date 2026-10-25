# IPIS Arena: first progress prototype

This is a separate project prototype based on the socket structure of your LE7
chat program. Keep your original LE7 files for the laboratory submission.

## What this version does

- One server accepts two clients and assigns Player 1 and Player 2.
- The server starts the round with both players at 50 HP.
- Each player privately submits one card: 1 for Kagat or 2 for Sagang.
- Either player can submit first. The server locks each valid choice and waits
  for both before revealing cards or calculating the result.
- Invalid input prompts that player to try again without losing the other choice.
- Both clients receive the same round result and updated HP.
- A disconnect or `quit` ends the session and notifies the connected peer.
- After one round, all programs exit. Restart them for another demonstration.

This milestone uses only one card per player for one round. It does not yet
implement the full approved game. Energy, three-slot sequences, hands, decks,
draw order, AP, Molt, additional card types, and match victory/draw detection
remain pending. Sagang's successful block does not award AP in this version.

## Files

| File | Purpose |
| --- | --- |
| server.c | Accept two players, validate and lock choices, calculate HP, send results. |
| client.c | Connect, display server messages, read and send the player's choice. |
| socket_io.h | Shared socket definitions, complete sends, line-based receives, port checks. |

The Linux code uses socket, bind, listen, accept, connect, select, send, recv,
and close. A conditional Windows branch in the shared header permits local
testing on Windows. It is not needed when compiling on Linux.

## Compile on Linux

Extract the archive, open a terminal in the `ipis_demo` folder, and run:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic server.c -o server
gcc -std=c11 -Wall -Wextra -Wpedantic client.c -o client
```

Keep socket_io.h beside both source files. It is included automatically.

## Run in three Linux terminals

Open all three terminals in the same folder. Start them in this order.

Terminal 1, server:

```sh
./server 5001
```

Terminal 2, Player 1:

```sh
./client 127.0.0.1 5001
```

Terminal 3, Player 2:

```sh
./client 127.0.0.1 5001
```

127.0.0.1 means all three programs run on one computer. For another computer
on the same reachable network, enter the server computer's IPv4 address instead.
The client expects a numeric IPv4 address, not a hostname.

Type `1` or `2` and press Enter when prompted. The server has no interactive
prompt; both players use the clients. Type `quit` at a client prompt to leave.

## Demonstration sequence

1. Connect Player 1 and show that it waits for Player 2.
2. Connect Player 2; both clients display the round and choices.
3. Enter `9` in one client to demonstrate rejection and a retry.
4. Submit `1` in Player 2 first. Show the waiting message and that Player 1 has
   not received Player 2's card.
5. Submit `1` in Player 1. Both clients should show both players at 45 HP.
6. Restart the programs and try Kagat against Sagang. Both remain at 50 HP.

| Player 1 | Player 2 | P1 final HP | P2 final HP |
| --- | --- | --- | --- |
| Kagat | Kagat | 45 | 45 |
| Kagat | Sagang | 50 | 50 |
| Sagang | Kagat | 50 | 50 |
| Sagang | Sagang | 50 | 50 |

Sagang deals no damage. In this demonstration it blocks the opponent's Kagat
in the single combat slot.

## How it differs from LE7

Your chat program accepted one client and alternated client/server messages.
This server accepts twice while keeping the first connection open. It then
uses select to watch both connections, so either player can submit first.

Messages end in a newline because TCP is a byte stream: one send does not
necessarily correspond to one recv. INPUT asks the client for terminal input;
END ends the session. The server stores choices privately until both are valid.

## Verification and limits

Both C programs were compiled with MinGW on Windows using C11, Wall, Wextra,
Wpedantic, and Werror. Actual C server/client socket tests covered all four
card combinations with matching client results. Additional socket tests covered
invalid and oversized input, a fragmented submission, choice locking, Player 2
submitting first, privacy before resolution, and disconnect notification.

Linux execution has not been verified here because WSL access was denied.
Compile and run the three-terminal demonstration on your Linux environment
before presenting. This prototype is progress evidence, not a guarantee of a
particular completion percentage or grade.

There is no turn timer or reconnect support. A client waiting for keyboard
input reads server messages after the user submits or exits that prompt.
Use Ctrl+C if needed to close a client stuck at its input prompt.

For a progress report, describe this as a two-client networking prototype with
one simplified combat round. Claim it works in your Linux environment only
after you have run it there successfully.
