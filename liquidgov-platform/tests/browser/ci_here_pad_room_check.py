"""The dots pad room check (here_pad_room_check.py) on Linux CI. Same server start/stop as
ci_phone_check.py, reused as is (its main() with here_pad_room_check.run in place of phone_check.run);
ci_phone_check.py is not edited.
Usage: LG_BROWSER=$(command -v google-chrome) python3 ci_here_pad_room_check.py <build dir> <liquidgov-platform dir> <label> <out dir>
Prints "VERDICT herepad-<label>: ..." (ship.py requires every VERDICT to be ALL-PASS)."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ci_phone_check  # noqa: E402
import here_pad_room_check  # noqa: E402

if __name__ == "__main__":
    ci_phone_check.phone_check.run = here_pad_room_check.run
    sys.exit(ci_phone_check.main(sys.argv[1:]))
