#!/usr/bin/env python3
"""CityLab helper: one command for everything a team does.

    python3 tools/citylab.py doctor              is this computer ready?
    python3 tools/citylab.py new team07          create your team folder from the template
    python3 tools/citylab.py run team07          build, then walk to your plot
    python3 tools/citylab.py check team07        the graders' checks, with screenshots
    python3 tools/citylab.py lint team07         the rules (classic OpenGL, your folder only, ...)
    python3 tools/citylab.py pack team07 --phase 1   the zip to upload to Courses (dist/)
    python3 tools/citylab.py plots               who builds on which plot
    python3 tools/citylab.py build               build the whole city

On Windows write  py tools\\citylab.py ...  (or python). Needs Python 3.8+.
Run it from the project folder. Every command prints what it runs.
"""
import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
TEAMS = ROOT / "teams"
TEMPLATES = TEAMS / "_templates"
TEAM_ID = re.compile(r"^team\d\d$")

MAX_FOLDER_MB = 15
ALLOWED_FILES = {".cpp", ".h", ".hpp", ".json", ".md", ".txt", ".png", ".jpg", ".jpeg", ".bmp", ".tga"}
SOURCE_FILES = {".cpp", ".h", ".hpp"}
IGNORED_NAMES = {".gitkeep", ".DS_Store", "Thumbs.db"}
IGNORED_DIRS = {".idea", ".vscode", ".vs"}
# teams/teamNN/extras/ may hold an optional modern-OpenGL demo. It is not part
# of the city build, so the classic-OpenGL rules do not apply there.
EXTRAS_FILES = {".glsl", ".vert", ".frag"}

# How to call this script on this system, for the hints it prints.
ME = "py tools\\citylab.py" if os.name == "nt" else "python3 tools/citylab.py"

USE_COLOUR = sys.stdout.isatty() and os.environ.get("NO_COLOR") is None
if USE_COLOUR and os.name == "nt":
    os.system("")  # turns on ANSI colours in the Windows console


def paint(text, code):
    return f"\033[{code}m{text}\033[0m" if USE_COLOUR else text


def say(text=""):
    print(text, flush=True)


def ok(text):
    say(f"  {paint('ok', '32')}    {text}")


def warn(text):
    say(f"  {paint('warn', '33')}  {text}")


def bad(text):
    say(f"  {paint('fail', '31')}  {text}")


def info(text):
    say(f"  {paint('info', '36')}  {text}")


class Failure(Exception):
    """A problem the user has to fix; printed without a traceback."""


def load_json(path):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise Failure(f"{path} does not exist")
    except json.JSONDecodeError as e:
        raise Failure(f"{path} is not valid JSON: line {e.lineno}, column {e.colno}: {e.msg}")


def team_folder(team):
    if not TEAM_ID.match(team or ""):
        raise Failure(f"'{team}' is not a team id; team ids look like team07")
    return TEAMS / team


def run(command, cwd=ROOT, check=True, capture=False, env=None):
    say(paint("$ " + " ".join(str(c) for c in command), "2"))
    try:
        result = subprocess.run([str(c) for c in command], cwd=str(cwd), env=env,
                                stdout=subprocess.PIPE if capture else None,
                                stderr=subprocess.STDOUT if capture else None,
                                universal_newlines=capture, errors="replace" if capture else None)
    except FileNotFoundError:
        raise Failure(f"{command[0]} was not found; run  {ME} doctor")
    if capture and result.stdout:
        sys.stdout.write(result.stdout)
    if check and result.returncode != 0:
        raise Failure(f"{command[0]} failed (exit code {result.returncode})")
    return result


# ---------------------------------------------------------------------------
# build / run / check
# ---------------------------------------------------------------------------

def executable():
    names = ["citylab.exe", "citylab"] if os.name == "nt" else ["citylab"]
    for folder in (BUILD, BUILD / "Release", BUILD / "Debug", BUILD / "RelWithDebInfo"):
        for name in names:
            if (folder / name).exists():
                return folder / name
    return None


def broken_teams(output):
    """Team folders named in compiler errors."""
    found = set()
    for line in output.splitlines():
        if re.search(r"error|Error|ERROR", line):
            found.update(re.findall(r"teams[/\\](team\d\d)[/\\]", line))
    return found


def cmake_command():
    """The CMake to run, or None. On Windows with Visual Studio it is the copy that comes with
    Visual Studio: it always knows that Visual Studio, and it works from any terminal."""
    if platform.system() == "Windows":
        studio = visual_studio_folder()
        if studio:
            bundled = (Path(studio) / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "CMake" /
                       "bin" / "cmake.exe")
            if bundled.exists():
                return str(bundled)
    return shutil.which("cmake")


def build(team=None, debug=False):
    cmake = cmake_command()
    if cmake is None:
        raise Failure(f"CMake was not found; run  {ME} doctor")
    config = "Debug" if debug else "Release"
    skipped = set()
    generator = []
    # Without Visual Studio, Windows builds with a recent MinGW g++ if there is one.
    if (platform.system() == "Windows" and not (BUILD / "CMakeCache.txt").exists() and visual_studio() is None
            and shutil.which("g++") and shutil.which("mingw32-make") and not os.environ.get("CMAKE_GENERATOR")):
        generator = ["-G", "MinGW Makefiles"]
    for _attempt in range(8):
        configure = [cmake, "-S", ROOT, "-B", BUILD, f"-DCMAKE_BUILD_TYPE={config}",
                     f"-DCITYLAB_TEAMS={team or 'all'}", f"-DCITYLAB_SKIP_TEAMS={';'.join(sorted(skipped))}"] + generator
        run(configure, capture=True)
        result = run([cmake, "--build", BUILD, "--config", config, "--parallel"], check=False, capture=True)
        if result.returncode == 0:
            for t in sorted(skipped):
                warn(f"{t} does not compile, so it was left out (its plot stays empty)")
            return
        broken = broken_teams(result.stdout) - skipped
        if team or not broken:
            raise Failure("the build failed; the first error above is the one to fix")
        say()
        for t in sorted(broken):
            warn(f"{t} does not compile; building the city without it")
        skipped |= broken
    raise Failure("the build kept failing")


def needs_display():
    return sys.platform.startswith("linux") and not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY")


def launch(arguments, check=True):
    exe = executable()
    if exe is None:
        raise Failure(f"the program is not built yet; run  {ME} build")
    command = [exe] + list(arguments)
    if needs_display():
        if shutil.which("xvfb-run"):
            command = ["xvfb-run", "-a", "-s", "-screen 0 1280x720x24"] + command
        else:
            warn("no display found (DISPLAY is not set); the window may not open")
    return run(command, check=check)


def cmd_build(args):
    build(args.team, args.debug)
    say(paint(f"built {executable()}", "32"))


def cmd_run(args):
    if not args.no_build:
        build(args.team, args.debug)
    extra = (["--team", args.team] if args.team else []) + args.rest
    result = launch(extra, check=False)
    return result.returncode


def cmd_check(args):
    team_folder(args.team)
    if not args.no_build:
        build(args.team)
    result = launch(["--check", args.team], check=False)
    report = BUILD / "check" / f"{args.team}.json"
    if report.exists():
        say(f"report: {report.relative_to(ROOT)}   screenshots: {(BUILD / 'check' / args.team).relative_to(ROOT)}")
    lint_ok = lint(args.team) == 0
    return 1 if result.returncode != 0 or not lint_ok else 0


# ---------------------------------------------------------------------------
# lint: the rules of the project, checked without running anything
# ---------------------------------------------------------------------------

CPP_RULES = [
    # (pattern, error?, message)
    (r"\bglGen(Buffers|VertexArrays|Framebuffers)\b|\bglBind(Buffer|VertexArray|Framebuffer)\b|\bglBufferData\b",
     True, "buffer and vertex-array objects are modern OpenGL; this project uses classic OpenGL only"),
    (r"\bgl(CreateShader|ShaderSource|CompileShader|CreateProgram|UseProgram|VertexAttribPointer|EnableVertexAttribArray)\b",
     True, "shaders are modern OpenGL; this project uses classic OpenGL only"),
    (r"\bglLoadIdentity\s*\(", True,
     "glLoadIdentity() throws away the city's camera and your plot's position; use glPushMatrix()/glPopMatrix()"),
    (r"\bgl(Frustum|Ortho|Viewport)\s*\(|\bgluPerspective\b|\bgluLookAt\b|GL_PROJECTION", True,
     "the projection, camera and viewport belong to the city"),
    (r"\bglClear\s*\(", True, "glClear() would wipe the whole city off the screen"),
    (r"\bglFog[fi]v?\s*\(", True, "fog belongs to the city"),
    (r"\bglLight[fi]v?\s*\(\s*GL_LIGHT0\b|\bglDisable\s*\(\s*GL_LIGHT0\b", True, "GL_LIGHT0 is the sun; use GL_LIGHT1 to GL_LIGHT4"),
    (r"\bGL_LIGHT[5-7]\b", True, "zones may use GL_LIGHT1 to GL_LIGHT4 (GL_LIGHT7 is the sky light)"),
    (r"\bGL_CLIP_PLANE[0-4]\b", True, "GL_CLIP_PLANE0..4 keep your zone inside its plot; use GL_CLIP_PLANE5"),
    (r"#\s*include\s*[<\"](GL/glew|GLFW|GL/glut|GL/freeglut|glad|SDL)", True,
     "the city already owns the window and OpenGL; include only <citylab/CityLab.h>"),
    (r"#\s*include\s*\"(\.\./)+|#\s*include\s*[<\"](core|app|city|engine|zones|framework)/", True,
     "include only the SDK (<citylab/...>) and files in your own folder"),
    (r"\bglLightModel", False, "the light model is set by the city; changing it affects only your plot"),
    (r"\bglDrawArrays\b|\bglVertexPointer\b", False, "vertex arrays work, but the course uses glBegin/glEnd and display lists"),
    (r"\bstd::thread\b|\bsystem\s*\(|\bpopen\s*\(", True, "zones may not start threads or programs"),
]

def strip_comments(text):
    """Blanks out comments (keeping line numbers) so rules do not fire on them."""
    def blank(match):
        return re.sub(r"[^\n]", " ", match.group(0))
    return re.sub(r"//[^\n]*|/\*[\s\S]*?\*/", blank, text)


def lint(team, quiet=False):
    """Returns the number of errors."""
    folder = team_folder(team)
    errors = 0

    def error(where, message):
        nonlocal errors
        errors += 1
        bad(f"{where}: {message}")

    say(f"\nlint {team}")
    if not folder.is_dir():
        error(f"teams/{team}", f"this folder does not exist ({ME} new {team})")
        return errors

    # team.json
    try:
        info = load_json(folder / "team.json")
        if info.get("id") != team:
            error("team.json", f'"id" must be "{team}", the name of the folder')
        if info.get("language", "cpp") != "cpp":
            error("team.json", 'zones are written in C++; remove "language" or set it to "cpp"')
        if not info.get("title") or not info.get("title_en"):
            warn('team.json: fill in "title" (Persian) and "title_en"; they go on your plot\'s sign')
        members = info.get("members") or []
        if len([m for m in members if m.get("name")]) < 1:
            warn('team.json: list your team in "members"')
    except Failure as e:
        error("team.json", str(e))

    # files
    total = 0
    sources = []
    for path in sorted(folder.rglob("*")):
        relative = path.relative_to(ROOT).as_posix()
        if any(part in IGNORED_DIRS for part in path.parts):
            continue
        if path.is_symlink():
            error(relative, "symbolic links are not allowed")
            continue
        if not path.is_file() or path.name in IGNORED_NAMES:
            continue
        total += path.stat().st_size
        suffix = path.suffix.lower()
        extras = path.relative_to(folder).parts[0] == "extras"
        if suffix not in ALLOWED_FILES and not (extras and suffix in EXTRAS_FILES):
            error(relative, f"'{suffix or path.name}' files do not belong in the repository "
                            "(allowed: sources, json/md/txt and png/jpg/bmp/tga images)")
        if extras:
            continue
        if suffix in SOURCE_FILES:
            sources.append(path)
    if total > MAX_FOLDER_MB * 1024 * 1024:
        error(f"teams/{team}", f"the folder holds {total / 1048576:.1f} MB; the limit is {MAX_FOLDER_MB} MB "
                               "(make images smaller, at most 1024 x 1024)")

    for path in sources:
        text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        if path.suffix == ".cpp" and "CITYLAB_REGISTER_ZONE" in text:
            if not re.search(rf"\bnamespace\s+{team}\b", text):
                warn(f"{path.relative_to(ROOT).as_posix()}: put your classes in  namespace {team} {{ ... }}")
        for number, line in enumerate(text.splitlines(), start=1):
            for pattern, is_error, message in CPP_RULES:
                if re.search(pattern, line):
                    where = f"{path.relative_to(ROOT).as_posix()}:{number}"
                    if is_error:
                        error(where, message)
                    elif not quiet:
                        warn(f"{where}: {message}")
    if errors == 0:
        ok(f"{len(sources)} source files, {total / 1048576:.1f} MB, no rule broken")
    return errors


def cmd_lint(args):
    teams = [args.team] if args.team else sorted(p.name for p in TEAMS.iterdir() if TEAM_ID.match(p.name))
    errors = sum(lint(t) for t in teams)
    return 1 if errors else 0


# ---------------------------------------------------------------------------
# boundary: a pull request may only change its own team's folder (CI)
# ---------------------------------------------------------------------------

def annotate(path, message):
    if os.environ.get("GITHUB_ACTIONS"):
        print(f"::error file={path}::{message}")


def cmd_boundary(args):
    registry = load_json(ROOT / "city" / "registry.json")
    login = (args.author or "").lower()
    staff = {s.lower() for s in registry.get("staff", [])}
    owned = {t["id"] for t in registry.get("teams", []) if login in {g.lower() for g in t.get("github", [])}}
    diff = run(["git", "diff", "--name-only", f"{args.base}...HEAD"], capture=True).stdout.split()
    say(f"\nboundary: {len(diff)} changed files, author {args.author or '?'}")
    if login in staff:
        ok("staff may change any file")
        return 0
    if not owned:
        bad(f"{args.author} is not in city/registry.json; ask a TA to add your GitHub login to your team")
        return 1
    touched = set()
    outside = []
    for name in diff:
        match = re.match(r"^teams/(team\d\d)/", name)
        if match and match.group(1) in owned:
            touched.add(match.group(1))
        else:
            outside.append(name)
    for name in outside:
        bad(f"{name}: outside teams/{'|'.join(sorted(owned))}/")
        annotate(name, "This pull request may only change your own team folder.")
    if len(touched) > 1:
        bad("one pull request should change one team folder")
        return 1
    if outside:
        say("\nA pull request may only change files in your own team folder. If the city itself needs to\n"
            "change (a bug, a missing feature), open an Issue instead.")
        return 1
    ok(f"every change is inside teams/{next(iter(touched)) if touched else next(iter(owned))}/")
    return 0


# ---------------------------------------------------------------------------
# new / pack / plots / doctor
# ---------------------------------------------------------------------------

def cmd_new(args):
    folder = team_folder(args.team)
    if folder.exists():
        raise Failure(f"teams/{args.team} already exists")
    plot = team_plot(args.team) or "?"
    # The sign shows the team's topic unless the team names its place itself.
    topic = team_topic(args.team) or {}
    args.title = args.title or topic.get("title", "")
    args.title_en = args.title_en or topic.get("title_en", "")
    values = {"__TEAM__": args.team, "__TITLE__": args.title or args.team, "__TITLE_EN__": args.title_en or args.team,
              "__PLOT__": plot}

    def fill(text):
        for key, value in values.items():
            text = text.replace(key, value)
        return text

    source = TEMPLATES / "cpp"
    for path in sorted(source.rglob("*")):
        if path.is_file():
            target = folder / fill(path.relative_to(source).as_posix())
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(fill(path.read_text(encoding="utf-8")), encoding="utf-8", newline="\n")
    (folder / "assets").mkdir(parents=True, exist_ok=True)
    (folder / "assets" / "README.md").write_text(fill((TEMPLATES / "common" / "assets.md").read_text(encoding="utf-8")),
                                                 encoding="utf-8", newline="\n")
    (folder / "README.md").write_text(fill((TEMPLATES / "common" / "README.md").read_text(encoding="utf-8")),
                                      encoding="utf-8", newline="\n")
    info = {"id": args.team, "title": args.title or "", "title_en": args.title_en or "",
            "members": [{"name": "", "student_id": "", "github": ""}, {"name": "", "student_id": "", "github": ""}]}
    (folder / "team.json").write_text(json.dumps(info, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    say(paint(f"created teams/{args.team}", "32"))
    say(f"  next: fill in teams/{args.team}/team.json, then  {ME} run {args.team}")
    say("  (the next build finds the new folder by itself, in Visual Studio too)")


def cmd_pack(args):
    folder = team_folder(args.team)
    if not folder.is_dir():
        raise Failure(f"teams/{args.team} does not exist")
    if not args.no_check:
        if cmd_check(argparse.Namespace(team=args.team, no_build=False)) != 0:
            warn("the check did not pass; the zip is made anyway, with the report inside")
    out = ROOT / "dist"
    out.mkdir(exist_ok=True)
    target = out / f"CG_PROJ_P{args.phase}_{args.team}.zip"
    commit = commit_of(folder)
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr(f"{args.team}/COMMIT.txt", commit + "\n")
        for path in sorted(folder.rglob("*")):
            if path.is_file() and not any(p in IGNORED_DIRS for p in path.parts) and path.name not in IGNORED_NAMES:
                z.write(path, path.relative_to(TEAMS).as_posix())
        report = BUILD / "check" / f"{args.team}.json"
        if report.exists():
            z.write(report, f"{args.team}/check/{report.name}")
        shots = BUILD / "check" / args.team
        if shots.is_dir():
            for shot in sorted(shots.glob("*.png")):
                z.write(shot, f"{args.team}/check/{shot.name}")
    say(paint(f"wrote {target.relative_to(ROOT)}  ({target.stat().st_size / 1048576:.1f} MB)", "32"))
    say(f"  commit: {commit}")
    if "uncommitted" in commit:
        warn("your team folder has uncommitted changes; commit and push them so GitHub and the zip agree")
    say("  upload it to Courses, and open (or update) your pull request on GitHub")


def commit_of(folder):
    """The commit the zip is made from, as graders will look it up on GitHub."""
    try:
        result = subprocess.run(["git", "rev-parse", "--verify", "-q", "HEAD"], cwd=str(ROOT), stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL, universal_newlines=True)
        head = result.stdout.strip() if result.returncode == 0 else ""
        dirty = subprocess.run(["git", "status", "--porcelain", "--", str(folder)], cwd=str(ROOT),
                               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True).stdout.strip()
    except OSError:
        return "unknown (git was not found)"
    if not head:
        return "unknown (no commit yet)"
    return head + (" + uncommitted changes" if dirty else "")


def team_plot(team_id):
    """The plot of a registered team: the plot of its topic, or the one the registry names."""
    layout = load_json(ROOT / "city" / "layout.json")
    registry = load_json(ROOT / "city" / "registry.json")
    for t in registry.get("teams", []):
        if t["id"] != team_id:
            continue
        if t.get("plot"):
            return t["plot"]
        return next((p["id"] for p in layout["plots"] if p.get("place") and p["place"] == t.get("place")), None)
    return None


def team_topic(team_id):
    """The catalogue entry of a registered team's topic, or None."""
    registry = load_json(ROOT / "city" / "registry.json")
    place = next((t.get("place") for t in registry.get("teams", []) if t["id"] == team_id), None)
    if not place:
        return None
    return next((p for p in load_json(ROOT / "city" / "places.json").get("places", []) if p["id"] == place), None)


def cmd_plots(args):
    layout = load_json(ROOT / "city" / "layout.json")
    registry = load_json(ROOT / "city" / "registry.json")
    places = {p["id"]: p for p in load_json(ROOT / "city" / "places.json").get("places", [])}
    districts = {d["id"]: d.get("title_en", d["id"]) for d in layout["districts"]}
    by_plot = {}
    for t in registry.get("teams", []):
        plot = team_plot(t["id"])
        if plot:
            by_plot[plot] = t
    say(f"{'plot':7} {'size':8} {'district':20} {'topic':26} team")
    for plot in layout["plots"]:
        team = by_plot.get(plot["id"])
        topic = places.get(plot.get("place", ""), {}).get("title_en", "") or "free plot"
        size = f"{plot['width']:g}x{plot['depth']:g}"
        say(f"{plot['id']:7} {size:8} {districts.get(plot['district'], plot['district']):20} {topic:26} "
            f"{team['id'] if team else '-'}")
    taken = sum(1 for p in layout["plots"] if p["id"] in by_plot)
    say(f"\n{len(layout['plots'])} plots, {taken} with a team. Topics: city/places.json")


def version_of(command, pattern=r"(\d+)\.(\d+)"):
    try:
        out = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             universal_newlines=True, errors="replace", timeout=20).stdout
    except (OSError, subprocess.TimeoutExpired):
        return None, ""
    lines = [l.strip() for l in out.splitlines() if l.strip()]
    for line in lines:
        match = re.search(pattern, line)
        if match:
            return tuple(int(g) for g in match.groups() if g is not None), line
    return None, lines[0] if lines else ""


def vswhere(prop):
    """A property of the newest Visual Studio that has the C++ tools, as vswhere reports it, or None."""
    tool = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / \
        "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not tool.exists():
        return None
    _, line = version_of([str(tool), "-latest", "-products", "*", "-requires",
                          "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", prop])
    return line or None


def visual_studio():
    """The name of the newest Visual Studio with the C++ tools, or None."""
    return vswhere("displayName")


def visual_studio_folder():
    """The folder the newest Visual Studio with the C++ tools is installed in, or None."""
    return vswhere("installationPath")


def check_compiler(system):
    """Says which C++ compiler CMake will find; returns 1 if none will do."""
    # g++ 9 is the first with C++17's <filesystem> without extra libraries.
    gxx, gxx_line = version_of(["g++", "-dumpfullversion", "-dumpversion"], r"(\d+)") if shutil.which("g++") else (None, "")
    if system == "Windows":
        studio = visual_studio()
        if studio:
            ok(f"C++ compiler: {studio}")
            if gxx and gxx[0] < 9:
                warn(f"an old g++ {gxx_line} is on PATH too; CMake uses Visual Studio, so this is fine")
            return 0
        if gxx and gxx[0] >= 9 and shutil.which("mingw32-make"):
            ok(f"C++ compiler: g++ {gxx_line} ({shutil.which('g++')}), built with MinGW Makefiles")
            return 0
        bad("no usable C++ compiler: install Visual Studio (2022 or newer) with the"
            " Desktop development with C++ workload, or MinGW-w64 g++ 9+ (see README.md)")
        if gxx:
            say(f"        the g++ on PATH is {gxx_line}, too old for C++17 (9 or newer is needed)")
        return 1
    if gxx and shutil.which("g++") and system == "Linux":
        if gxx[0] >= 9:
            ok(f"C++ compiler: g++ {gxx_line}")
            return 0
        bad(f"g++ {gxx_line} is too old for C++17; 9 or newer is needed")
        say("        sudo apt install g++   (Ubuntu 20.04 and newer have g++ 9+)")
        return 1
    found = next((c for c in ("clang++", "g++", "c++") if shutil.which(c)), None)
    if found:
        _, line = version_of([found, "--version"])
        ok(f"C++ compiler: {line or shutil.which(found)}")
        return 0
    bad("no C++ compiler found")
    say("        " + ("xcode-select --install" if system == "Darwin" else "sudo apt install build-essential"))
    return 1


def cmd_doctor(args):
    system = platform.system()
    problems = 0
    say(f"CityLab doctor  ({system} {platform.release()}, {platform.machine()})\n")

    if sys.version_info >= (3, 8):
        ok(f"Python {platform.python_version()}")
    else:
        bad(f"Python {platform.python_version()}: CityLab needs 3.8 or newer")
        problems += 1

    command = cmake_command()
    cmake, line = version_of([command, "--version"]) if command else (None, "")
    if cmake and cmake >= (3, 20):
        ok(line + ("  (the copy in Visual Studio)" if command and "Microsoft Visual Studio" in command else ""))
    else:
        problems += 1
        bad("CMake 3.20 or newer is needed" + (f" (found {line})" if line else " (not found)"))
        say({"Windows": "        Visual Studio Installer > Modify > tick C++ CMake tools for Windows; or MinGW-w64 (README.md)",
             "Darwin": "        brew install cmake",
             }.get(system, "        sudo apt install cmake   (Ubuntu/Debian)"))

    problems += check_compiler(system)

    if system == "Linux":
        headers = [Path("/usr/include/GL/gl.h"), Path("/usr/include/X11/Xlib.h")]
        missing = [str(h) for h in headers if not h.exists()]
        if missing:
            problems += 1
            bad("OpenGL/X11 headers are missing: " + ", ".join(missing))
            say("        sudo apt install libgl1-mesa-dev libx11-dev")
        else:
            ok("OpenGL and X11 headers")

    exe = executable()
    if exe:
        ok(f"built: {exe.relative_to(ROOT)}")
    else:
        info(f"not built yet; the first  {ME} run  builds it (a few minutes)")

    say()
    if problems:
        say(paint(f"{problems} problem(s) to fix first; see the Setup section of README.md", "31"))
        return 1
    say(paint(f"ready. Next: {ME} run", "32"))
    return 0


# ---------------------------------------------------------------------------

# Options of the program that take a value (build/citylab --help), so that
# "run --team team08" passes team08 to the program instead of reading it as
# the team to run.
PROGRAM_VALUE_OPTIONS = {"--team", "--check", "--time", "--time-scale", "--fixed-dt", "--screenshot", "--frames",
                         "--camera", "--size", "--msaa", "--bench", "--root"}


def split_run(tokens):
    """The arguments of  run: its own (a team, --no-build, --debug) and the program's options,
    which may follow directly ("run team07 --time 20:30") or after "--"."""
    args = argparse.Namespace(command="run", func=cmd_run, team=None, no_build=False, debug=False, rest=[])
    for i, token in enumerate(tokens):
        if token == "--":
            args.rest += tokens[i + 1:]
            break
        if token == "--no-build":
            args.no_build = True
        elif token == "--debug":
            args.debug = True
        elif args.team is None and TEAM_ID.match(token) and not (args.rest and args.rest[-1] in PROGRAM_VALUE_OPTIONS):
            args.team = token
        else:
            args.rest.append(token)
    return args


def main(argv=None):
    parser = argparse.ArgumentParser(prog="citylab.py", description=__doc__.split("\n\n")[0],
                                     formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    sub = parser.add_subparsers(dest="command", metavar="command")
    sub.required = True

    p = sub.add_parser("doctor", help="check that this computer can build and run CityLab")
    p.set_defaults(func=cmd_doctor)

    p = sub.add_parser("new", help="create your team folder from a template")
    p.add_argument("team", help="your team id, e.g. team07")
    p.add_argument("--title", help="the Persian name on the sign (default: your topic's)")
    p.add_argument("--title-en", help="the English name on the sign (default: your topic's)")
    p.set_defaults(func=cmd_new)

    p = sub.add_parser("build", help="build the city (all zones, or only yours with --team)")
    p.add_argument("--team", help="build only this zone (faster)")
    p.add_argument("--debug", action="store_true", help="debug build (for a debugger)")
    p.set_defaults(func=cmd_build)

    p = sub.add_parser("run", help="build, then start the city (at your plot if you name your team)")
    p.add_argument("team", nargs="?", help="start at this team's plot and run only its zone")
    p.add_argument("--no-build", action="store_true", help="do not build first")
    p.add_argument("--debug", action="store_true", help="debug build")
    p.epilog = ("Any other option goes to the program, e.g.  run team07 --time 20:30  (the program's options: "
                "build/citylab --help).")
    p.set_defaults(func=cmd_run, rest=[])

    p = sub.add_parser("check", help="the graders' checks: report and screenshots in build/check/")
    p.add_argument("team")
    p.add_argument("--no-build", action="store_true")
    p.set_defaults(func=cmd_check)

    p = sub.add_parser("lint", help="check the project rules without running anything")
    p.add_argument("team", nargs="?", help="one team (default: every team)")
    p.set_defaults(func=cmd_lint)

    p = sub.add_parser("boundary", help="(CI) a pull request changes only its author's team folder")
    p.add_argument("--base", required=True, help="the branch the pull request goes into, e.g. origin/main")
    p.add_argument("--author", help="GitHub login of the pull request's author")
    p.set_defaults(func=cmd_boundary)

    p = sub.add_parser("pack", help="make the zip to upload to Courses")
    p.add_argument("team")
    p.add_argument("--phase", type=int, choices=[1, 2, 3], required=True)
    p.add_argument("--no-check", action="store_true", help="do not run the check first")
    p.set_defaults(func=cmd_pack)

    p = sub.add_parser("plots", help="list the plots and who builds on them")
    p.set_defaults(func=cmd_plots)

    argv = sys.argv[1:] if argv is None else list(argv)
    own = argv[1:argv.index("--")] if "--" in argv else argv[1:]
    if argv and argv[0] == "run" and not {"-h", "--help"} & set(own):
        args = split_run(argv[1:])
    else:
        args = parser.parse_args(argv)
    try:
        return args.func(args) or 0
    except Failure as e:
        say(paint(f"\ncitylab.py: {e}", "31"))
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
