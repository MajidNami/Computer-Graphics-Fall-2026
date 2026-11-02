#!/usr/bin/env python3
"""Generate city/layout.json, the single source of truth for the city plan.

Staff-only. Edit the numbers below or city/places.json, run this script and
commit the JSON. Coordinates are metres; +x east, +y up, -z north. Every
rectangle is [xMin, xMax, zMin, zMax].

The city is a grid: six blocks of six lots around a central park. A lot is
LOT["width"] m of frontage by LOT["depth"] m of depth (80 x 100 m). Every topic
in city/places.json names the lot it is built on ("plot") and how many lots it
takes ("lots"); a topic with two lots also gets the next lot in the same row,
so the plot is twice as wide (160 x 100 m). Lots that no topic uses stay free
plots of their own, so a topic can be added or given more land later by
editing places.json only.

The size of a lot is the one number to change to make every plot bigger or
smaller: the blocks, the avenues, the park, the edge of the city and the
spawn point are all computed from it. Run gen_signs.py afterwards
only if a plot id or a title changed; the plot count and ids do not depend on
the size.
"""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import check_places  # noqa: E402  (the catalogue is checked before the plan is written)

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "city" / "layout.json"
PLACES = ROOT / "city" / "places.json"

LOT = {"width": 80, "depth": 100, "height": 40, "basement": 5, "gate": 10}
STREET = 30      # every street band, kerb to kerb including both sidewalks
SIDEWALK = 5

# Everything below follows from the lot. A block is three lots wide and two
# deep (the first row faces north, the second south). The park keeps its place
# in the middle (z from -140 to 60, the tram boulevard on its south side, z = 75),
# so a bigger lot makes the city grow to the north and to the south and the park wider.
W, D = LOT["width"], LOT["depth"]
BLOCK_W, BLOCK_D = 3 * W, 2 * D
PARK_Z = (-140, 60)
HALF = (3 * BLOCK_W + 4 * STREET) // 2              # x of the city's east edge; the west edge is -HALF
NORTH_ROW = (PARK_Z[0] - STREET - BLOCK_D, PARK_Z[0] - STREET)
SOUTH_ROW = (PARK_Z[1] + STREET, PARK_Z[1] + STREET + BLOCK_D)
CITY = [-HALF, HALF, NORTH_ROW[0] - STREET, SOUTH_ROW[1] + STREET]
Z0, Z1 = CITY[2], CITY[3]
A = (-HALF + STREET, -HALF + STREET + BLOCK_W)       # block x ranges: the western, middle and eastern blocks
COLUMNS = {"A": A, "B": (-BLOCK_W // 2, BLOCK_W // 2), "C": (-A[1], -A[0])}
ROWS = {"north": NORTH_ROW, "south": SOUTH_ROW}      # block z ranges
PARK = [-HALF + STREET, HALF - STREET, PARK_Z[0], PARK_Z[1]]

# district id, code, column, row, colour of its signs and markings, titles
DISTRICTS = [
    ("culture", "CU", "A", "north", [0.36, 0.42, 0.90], "فرهنگ و آموزش", "Culture & Education"),
    ("commercial", "CO", "B", "north", [0.95, 0.55, 0.20], "تجاری و اداری", "Business & Shopping"),
    ("sports", "SP", "C", "north", [0.20, 0.72, 0.38], "ورزش و تفریح", "Sports & Leisure"),
    ("transport", "TR", "A", "south", [0.15, 0.62, 0.85], "حمل‌ونقل", "Transport"),
    ("services", "SV", "B", "south", [0.88, 0.25, 0.30], "خدمات شهری", "City Services"),
    ("energy", "EN", "C", "south", [0.95, 0.75, 0.15], "انرژی و صنعت", "Energy & Industry"),
]

# Street bands. "x" bands run north-south, "z" bands east-west. The two inner
# avenues stop at the park, which they enter as paths.
INNER = (A[1], COLUMNS["B"][0])                      # the gap between the western and the middle block
STREETS = [
    {"id": "north", "rect": [-HALF, HALF, Z0, Z0 + STREET], "kind": "avenue"},
    {"id": "park-north", "rect": [-HALF, HALF, PARK_Z[0] - STREET, PARK_Z[0]], "kind": "avenue"},
    {"id": "tram", "rect": [-HALF, HALF, PARK_Z[1], PARK_Z[1] + STREET], "kind": "tram"},
    {"id": "south", "rect": [-HALF, HALF, Z1 - STREET, Z1], "kind": "avenue"},
    {"id": "west", "rect": [-HALF, -HALF + STREET, Z0, Z1], "kind": "avenue"},
    {"id": "east", "rect": [HALF - STREET, HALF, Z0, Z1], "kind": "avenue"},
    {"id": "inner-west-north", "rect": [INNER[0], INNER[1], Z0, PARK_Z[0]], "kind": "avenue"},
    {"id": "inner-east-north", "rect": [-INNER[1], -INNER[0], Z0, PARK_Z[0]], "kind": "avenue"},
    {"id": "inner-west-south", "rect": [INNER[0], INNER[1], PARK_Z[1], Z1], "kind": "avenue"},
    {"id": "inner-east-south", "rect": [-INNER[1], -INNER[0], PARK_Z[1], Z1], "kind": "avenue"},
]


def lot_center(column: str, row: str, k: int):
    """Centre of lot k (1..6) of a block: north row west to east 1..3, south row 4..6."""
    x0 = COLUMNS[column][0]
    z0 = ROWS[row][0]
    x = x0 + W // 2 + W * ((k - 1) % 3)
    z = z0 + D // 2 if k <= 3 else z0 + 3 * D // 2
    # North-row lots face north (yaw 180 turns local +z to -z), south-row lots south.
    yaw = 180 if k <= 3 else 0
    return x, z, yaw


def main() -> None:
    if check_places.main():
        raise SystemExit("places.json has problems (above); fix them and run this script again")
    places = json.loads(PLACES.read_text(encoding="utf-8"))["places"]
    by_lot = {p["plot"]: p for p in places}

    districts, plots, used = [], [], set()
    for did, code, column, row, colour, title, title_en in DISTRICTS:
        x0, x1 = COLUMNS[column]
        z0, z1 = ROWS[row]
        ids = []
        for k in range(1, 7):
            lot = f"{code}-{k}"
            if lot in used:
                continue
            place = by_lot.get(lot)
            x, z, yaw = lot_center(column, row, k)
            lots = [lot]
            if place and place.get("lots", 1) == 2:
                if k in (3, 6):
                    raise SystemExit(f"places.json: {place['id']} takes two lots but {lot} is the last in its row")
                lots.append(f"{code}-{k + 1}")
                if lots[1] in by_lot:
                    raise SystemExit(f"places.json: {place['id']} needs {lots[1]}, which {by_lot[lots[1]]['id']} uses")
                x += W // 2
            if place and place["district"] != did:
                raise SystemExit(f"places.json: {place['id']} is in district {place['district']} but {lot} is in {did}")
            used.update(lots)
            ids.append(lot)
            plots.append({
                "id": lot, "district": did, "center": [x, z], "yaw": yaw,
                "width": W * len(lots), "depth": LOT["depth"], "lots": lots,
                "place": place["id"] if place else "",
            })
        districts.append({"id": did, "code": code, "title": title, "title_en": title_en,
                          "rect": [x0, x1, z0, z1], "color": colour, "plots": ids})
    for p in places:
        if p["plot"] not in used:
            raise SystemExit(f"places.json: {p['id']} names unknown lot {p['plot']}")

    layout = {
        "version": 2,
        "about": "CityLab city plan. Metres; +x east, +y up, -z north. Rectangles are "
                 "[xMin, xMax, zMin, zMax]. Generated by tools/staff/gen_layout.py from city/places.json.",
        "lot": LOT,
        "spawn": {"position": [0, 0, Z0 + STREET // 2], "yaw": 180},
        "city": CITY,
        "sidewalk": SIDEWALK,
        "streets": STREETS,
        "park": {
            "rect": PARK,
            "tower": {"center": [0, -40], "height": 120},
            "lake": {"center": [-round(0.65 * (HALF - STREET) / 5) * 5, -40], "radius": [80, 55]},
            "fountain": {"center": [round(0.57 * (HALF - STREET) / 5) * 5, -40], "radius": 12},
        },
        "tram": {"z": 75, "width": 6},
        "districts": districts,
        "plots": plots,
    }
    OUT.write_text(json.dumps(layout, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    free = sum(1 for p in plots if not p["place"])
    print(f"wrote {OUT.relative_to(ROOT)}: {len(plots)} plots ({free} free) in {len(districts)} districts")


if __name__ == "__main__":
    main()
