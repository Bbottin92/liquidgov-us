"""The map sticky-bars check (map_sticky_check.py) on Linux CI. Same server start/stop as ci_phone_check.py, reused as
is (its main() with map_sticky_check.run in place of phone_check.run); ci_phone_check.py is not edited.
Usage: LG_BROWSER=$(command -v google-chrome) python3 ci_map_sticky_check.py <build dir> <liquidgov-platform dir> <label> <out dir>
Prints "VERDICT sticky-<label>: ..." (ship.py requires every VERDICT platform.yml should print)."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ci_phone_check  # noqa: E402
import map_sticky_check  # noqa: E402

if __name__ == "__main__":
    ci_phone_check.phone_check.run = map_sticky_check.run
    sys.exit(ci_phone_check.main(sys.argv[1:]))
