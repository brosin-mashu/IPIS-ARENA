"""Optional end-to-end terminal tests. Run after compiling the game."""
import pathlib
import subprocess
import sys

binary = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "./ipis_logic").resolve()


def run(text, *args):
    result = subprocess.run(
        [str(binary), *args], input=text, capture_output=True, text=True, timeout=10
    )
    assert result.returncode == 0, result.stderr
    return result.stdout


# The same handoff/input sequence described in PRESENTATION.md.
output = run("\n1 1 0\n2 5 1\n2 1 0\n\n4 3 0\n\n3 5 0\n\n1\n5 2 0\nq\n")
for expected in [
    "same hand position cannot be used twice",
    "costs more than 4 energy",
    "P1: HP 50 -> 45 | AP 0 -> 0",
    "P2: HP 50 -> 50 | AP 0 -> 2",
    "Molt used: 2 AP spent",
    "P1: HP 45 -> 40 | AP 0 -> 0",
    "P2: HP 50 -> 35 | AP 0 -> 0",
    "Demo ended. No winner awarded.",
]:
    assert expected in output, expected
print("PASS: interactive presentation, invalid selections, AP and Molt")

output = run("\n" + "9" * 500 + "\n1abc 0 0\n1 0 0 0\nq\n")
assert "Input is too long" in output
assert output.count("Enter exactly three numbers") == 2
assert "ROUND 1 RESULTS" not in output
assert "No winner awarded" in run("")
print("PASS: oversized input, malformed numbers, extra fields and EOF")

# First card cycles through the deterministic deck. P2 passes, so no Molt.
round_input = "\n1 0 0\n\n0 0 0\n"
output = run(round_input * 12 + "1\n\nq\n")
assert "PLAYER 1 WINS!" in output
assert output.count("ROUND 1 -- PLAYER 1 PLANNING") == 2
print("PASS: interactive full match, winner and fresh-match restart")

output = run("", "--demo")
assert "PLAYER 1 WINS!" in output
assert "P2: HP 50 -> 35" in output
assert "4 energy" in run("", "--rules")
print("PASS: calculated guided demo and rules command")
