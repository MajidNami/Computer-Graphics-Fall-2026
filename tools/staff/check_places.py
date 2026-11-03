#!/usr/bin/env python3
"""Check city/places.json (and the Persian titles in the team.json files).

Staff-only. gen_layout.py runs it before it writes the plan, so a mistake in
the catalogue stops there; run it by hand after editing:

    python tools/staff/check_places.py

What it checks:

* Persian is written the Persian way. The signs of Arabic (vowel marks such as
  fatha, damma, kasra, shadda and sukun; the hamza on a word's last heh, as in
  "خانهٔ"; the letters ء أ إ ؤ ۀ; the Arabic yeh, kaf and teh marbuta) are not
  used, unless the mark is inseparable from the word itself. Those few words
  are listed in KEEP. A last heh before an ezafe is written ه‌ی ("خانه‌ی").
* The catalogue is consistent: unique ids and plots, and English and Persian
  titles for every topic.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]

# Words with a mark that cannot be taken out of them (the spelling needs it).
KEEP = {"رأس", "تئاتر"}

ARABIC = {
    "ً": "tanwin", "ٌ": "tanwin", "ٍ": "tanwin",
    "َ": "fatha", "ُ": "damma", "ِ": "kasra", "ّ": "shadda", "ْ": "sukun",
    "ٓ": "madda mark", "ٔ": "hamza mark", "ٕ": "hamza mark", "ٰ": "dagger alef",
    "ء": "hamza", "أ": "alef with hamza", "إ": "alef with hamza", "ؤ": "waw with hamza",
    "ئ": "yeh with hamza", "ۀ": "heh with yeh above",
    "ي": "Arabic yeh (write ی)", "ك": "Arabic kaf (write ک)", "ة": "teh marbuta", "ى": "alef maksura",
}
WORD = re.compile("[؀-ۿ‌]+")


def persian_problems(text: str, where: str):
    """Every word of `text` that has a mark of Arabic in it, unless it is a KEEP word."""
    problems = []
    for word in WORD.findall(text):
        marks = sorted({ARABIC[c] for c in word if c in ARABIC})
        if not marks:
            continue
        plain = word.replace("‌", "")
        if any(k.replace("‌", "") in plain for k in KEEP):
            continue
        problems.append(f"{where}: '{word}' has {', '.join(marks)}")
    return problems


def check(places_file=None, teams_dir=None):
    places_file = places_file or ROOT / "city" / "places.json"
    teams_dir = teams_dir or ROOT / "teams"
    problems = []
    data = json.loads(places_file.read_text(encoding="utf-8"))
    ids, plots = set(), set()
    for p in data["places"]:
        pid = p["id"]
        if pid in ids:
            problems.append(f"{pid}: id used twice")
        ids.add(pid)
        if p["plot"] in plots:
            problems.append(f"{pid}: plot {p['plot']} is used by another topic")
        plots.add(p["plot"])
        for key in ("title", "title_en"):
            if not str(p.get(key, "")).strip():
                problems.append(f"{pid}: '{key}' is empty")
        problems += persian_problems(p.get("title", ""), f"{pid}.title")
    for team_file in sorted(teams_dir.glob("*/team.json")):
        team = json.loads(team_file.read_text(encoding="utf-8"))
        problems += persian_problems(team.get("title", ""), f"{team_file.parent.name} title")
    return problems


def main() -> int:
    problems = check()
    for line in problems:
        print("places:", line)
    if not problems:
        print("places: catalogue ok (titles and Persian spelling)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
