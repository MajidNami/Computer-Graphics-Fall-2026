# team12 · Football Stadium

Amirabbas and Mahan are building an Allianz Arena-inspired stadium on CityLab's double plot SP-1 + SP-2 (160 × 100 m).

## Setup

Install Visual Studio Community with Desktop development with C++, the Windows SDK and CMake tools, and Python 3.8 or newer. From the repository root, run:

```powershell
python tools/citylab.py doctor
python tools/citylab.py run team12
python tools/citylab.py lint team12
python tools/citylab.py check team12
```

If PowerShell cannot resolve `python`, use `py`, `python3`, or the full path to your installed Python. With uv, use `uv python list --only-installed` to locate the executable. The [root setup guide](../../README.md#setup-method-1-visual-studio-windows-recommended) covers fresh installations.

The checker writes a JSON report and four screenshots under `build/check/`. Fill in student IDs and GitHub handles in `team.json` when available.

## Phase 1

The stadium shell, roof, seating and grounds are implemented. The final design still needs the player facilities and dressing rooms, ticket office and fan shop. Each of the four main structures requires at least eight separately drawn components. The [architecture note](ARCHITECTURE.md) records the shared dimensions, ownership and measured checkpoints.

## Sources and licences

The project uses CityLab's generated template and SDK. Allianz Arena, Munich is the architectural reference. No external art assets are used. Codex assisted with the bootstrap, Amirabbas-stage geometry, and validation.
