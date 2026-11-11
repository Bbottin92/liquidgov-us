"""The map bar check (map_bar_check.py) on Linux CI. Same server start/stop as ci_phone_check.py (phone_serve.sh,
throwaway store, port 18140): its main() is reused as is, with map_bar_check.run in place of phone_check.run, so the
two runners cannot drift apart. ci_phone_check.py is a committed test file and is not edited.
Usage: LG_BROWSER=$(command -v google-chrome) python3 ci_map_bar_check.py <build dir> <liquidgov-platform dir> <label> <out dir>
Exit 0 only when every check passes; prints "VERDICT bar-<label>: ..." (ship.py requires every VERDICT to be ALL-PASS)."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ci_phone_check  # noqa: E402
import map_bar_check  # noqa: E402

if __name__ == "__main__":
    ci_phone_check.phone_check.run = map_bar_check.run
    sys.exit(ci_phone_check.main(sys.argv[1:]))
