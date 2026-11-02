#!/usr/bin/env python3
"""Generate the city's name boards: city/signs/*.png.

Staff-only tool (needs Pillow built with libraqm for Persian shaping). The
PNGs are committed, so students never run this. Run it again after a team
is added to city/registry.json or changes the title in its team.json:

    python tools/staff/gen_signs.py            # all signs
    python tools/staff/gen_signs.py --check    # exit 1 if any sign is out of date

Every board is 1024 x 256 (4:1), the shape of the boards at the plot gates
and the district signs: a white panel with a bar in the district colour, a
Persian name and a small English line under it.
"""
import argparse
import io
import json
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont, features

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "city" / "signs"
FONT = pathlib.Path(__file__).resolve().parent / "fonts" / "Vazirmatn-Bold.ttf"

W, H = 1024, 256
INK = (34, 40, 52)
GREY = (108, 116, 128)
PANEL = (250, 250, 248)
NEUTRAL = (150, 156, 166)


def load_json(path: pathlib.Path):
    return json.loads(path.read_text(encoding="utf-8"))


def rgb(colour) -> tuple:
    return tuple(int(round(c * 255)) for c in colour)


def background(accent: tuple) -> Image.Image:
    img = Image.new("RGB", (W, H), PANEL)
    d = ImageDraw.Draw(img)
    # The district colour as a bar on the right (where Persian text starts)
    # and a thin line along the bottom.
    d.rectangle([W - 56, 0, W - 1, H - 1], fill=accent)
    d.rectangle([0, H - 14, W - 1, H - 1], fill=accent)
    d.rectangle([0, 0, W - 1, H - 1], outline=(220, 222, 226), width=2)
    return img

def fit_font(text: str, max_width: int, size: int, rtl: bool) -> ImageFont.FreeTypeFont:
    while size > 18:
        font = ImageFont.truetype(str(FONT), size)
        box = font.getbbox(text, direction="rtl" if rtl else "ltr", language="fa" if rtl else "en")
        if box[2] - box[0] <= max_width:
            return font
        size -= 4
    return ImageFont.truetype(str(FONT), size)


def board(title: str, subtitle: str, accent: tuple) -> Image.Image:
    img = background(accent)
    d = ImageDraw.Draw(img)
    text_width = W - 2 * 110
    big = fit_font(title, text_width, 108, rtl=True)
    small = fit_font(subtitle, text_width, 38, rtl=False)
    centre = (W - 56) / 2
    d.text((centre, 104), title, font=big, fill=INK, anchor="mm", direction="rtl", language="fa")
    d.text((centre, 196), subtitle, font=small, fill=GREY, anchor="mm")
    return img


def team_title(team: dict) -> str:
    path = ROOT / "teams" / team["id"] / "team.json"
    if path.exists():
        title = load_json(path).get("title", "").strip()
        if title:
            return title
    return team["id"]


def signs() -> dict:
    layout = load_json(ROOT / "city" / "layout.json")
    registry = load_json(ROOT / "city" / "registry.json")
    places = {p["id"]: p for p in load_json(ROOT / "city" / "places.json")["places"]}
    districts = {d["id"]: d for d in layout["districts"]}
    # A team's plot is the one of its topic, unless the registry names one.
    plot_of_place = {p["place"]: p["id"] for p in layout["plots"] if p["place"]}
    by_plot = {}
    for t in registry.get("teams", []):
        plot = t.get("plot") or plot_of_place.get(t.get("place", ""))
        if plot:
            by_plot[plot] = t

    out = {}
    for d in layout["districts"]:
        out[f"district-{d['id']}"] = board(d["title"], d["title_en"], rgb(d["color"]))
    for p in layout["plots"]:
        accent = rgb(districts[p["district"]]["color"]) if p["district"] in districts else NEUTRAL
        team, place = by_plot.get(p["id"]), places.get(p["place"])
        if team:
            out[f"plot-{p['id']}"] = board(team_title(team), f"{p['id']}  ·  {team['id']}", accent)
        elif place:
            out[f"plot-{p['id']}"] = board(place["title"], f"{p['id']}  ·  {place['title_en']}  ·  coming soon", accent)
        else:
            out[f"plot-{p['id']}"] = board("زمین آزاد", f"{p['id']}  ·  free plot", accent)
    return out


def png_bytes(img: Image.Image) -> bytes:
    buffer = io.BytesIO()
    img.save(buffer, format="PNG", optimize=True)
    return buffer.getvalue()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="only report signs that are missing or out of date")
    args = parser.parse_args()
    if not features.check("raqm"):
        print("Pillow was built without libraqm, so Persian text would not be shaped. "
              "Install libraqm (e.g. apt install libraqm0) and reinstall Pillow.", file=sys.stderr)
        return 1

    OUT.mkdir(parents=True, exist_ok=True)
    stale = []
    wanted = signs()
    for name, img in wanted.items():
        path = OUT / f"{name}.png"
        data = png_bytes(img)
        if path.exists() and path.read_bytes() == data:
            continue
        stale.append(name)
        if not args.check:
            path.write_bytes(data)
    extra = sorted(p.stem for p in OUT.glob("*.png") if p.stem not in wanted)
    if args.check:
        for name in stale:
            print(f"out of date: city/signs/{name}.png")
        for name in extra:
            print(f"not used: city/signs/{name}.png")
        return 1 if stale or extra else 0
    for name in extra:
        (OUT / f"{name}.png").unlink()
    print(f"city/signs: {len(wanted)} signs, {len(stale)} written, {len(extra)} removed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
