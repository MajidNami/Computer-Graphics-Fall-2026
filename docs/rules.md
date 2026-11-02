# Rules of the city

Thirty teams draw into one city at the same time. These rules keep every
plot inside its borders, keep the city fast, and make sure the graphics in
each plot are your own work in classic OpenGL. Most of them are checked by
the tools, so you find out on your own computer long before a TA does:

```sh
python3 tools/citylab.py lint team07     # the rules you can see in the code
python3 tools/citylab.py check team07    # the rules you can only see by running it
```

(On Windows write `py tools\citylab.py` instead of `python3 tools/citylab.py`.)

## The plot contract

Your zone draws in **plot-local coordinates**, in metres. When `draw()` runs,
the modelview matrix already places your plot in the city, whichever way it
faces, so the same code works on every plot.

```
                 back of the plot (z = -50)
            +-------------------------------+
            |                               |
            |                               |
    x = -40 |            origin             | x = +40
            |               +               |
            |                               |
            |                               |
            +-----------|  gate  |----------+
                 street side (z = +50)
```

| | Single plot | Double plot |
|---|---|---|
| x (across the frontage) | −40 … +40 | −80 … +80 |
| z (back → street) | −50 … +50 | −50 … +50 |
| y (up) | 0 … 40, down to −5 below ground | the same |
| gate | 10 m wide, centred on x = 0 at z = +50 | the same |

- **+z points at the street.** Your main façade and your entrance face +z.
- **Read the size from `plot()`.** Never write 40 or 80 for the half-width:
  use `plot.width / 2` (and `plot.depth / 2` for the depth). A team that is given the free plot next to it then
  needs no change in its code.
- **Below ground.** The city lays no ground under your plot, so you may dig
  down to y = −5 for a basement, a pool or a dock. Draw your own floor down
  there, or visitors see the sky through the hole.
- **The gate stays open.** A visitor must be able to walk in through the
  gate. The check walks in from 4 m outside to 6 m inside the plot at
  x = −2.5, 0 and +2.5, so keep that strip free of colliders.

### What happens outside the borders

Five clip planes (`GL_CLIP_PLANE0` to `GL_CLIP_PLANE4`) cut away everything
outside x, z and above y = 40, so nothing you draw can paint over the street
or a neighbour. The check still reports every vertex outside the box as a
warning, because a building that is cut in half looks broken. Animations
count too: a crane that swings its jib over the street is outside the plot.

## What draw() starts from

Before every `draw()` the city saves the whole OpenGL state
(`glPushAttrib(GL_ALL_ATTRIB_BITS)` and the matrices) and sets the same
starting state. After `draw()` it puts everything back. So you can change
any state inside your zone without breaking the city, and you never need to
undo it yourself.

| State | At the start of draw() |
|---|---|
| Lighting | on, with the sun (`GL_LIGHT0`) and the sky light (`GL_LIGHT7`); `GL_LIGHT1` to `GL_LIGHT6` off |
| Colour material | on: `glColor` sets the ambient and diffuse colour of the front and back faces |
| Specular, emission, shininess | black, black, 0 |
| `GL_NORMALIZE` | on |
| Depth test | on, `GL_LEQUAL`, depth writes on |
| Face culling | off, counter-clockwise front faces |
| Blending, alpha test, textures | off; texture 0 bound, `GL_MODULATE` |
| Polygon mode, shading | filled, smooth |
| Colour | white |
| Line width, point size | 1, 1 |
| Clip planes | 0 to 4 keep you in the plot; plane 5 is free for you |

## What belongs to the city

| You may not | Because | Instead |
|---|---|---|
| Change the projection, the viewport or the camera (`glFrustum`, `glOrtho`, `gluLookAt`, `GL_PROJECTION`) | the city draws everyone with one camera | move your objects |
| Call `glLoadIdentity()` on the modelview | it throws away the camera and your plot's place | `glPushMatrix()` / `glPopMatrix()` |
| Call `glClear()` | it wipes the whole city off the screen | nothing; the city clears once per frame |
| Change the fog | fog is shared | nothing; your plot gets the city's fog |
| Touch `GL_LIGHT0` (the sun), `GL_LIGHT5` to `GL_LIGHT7` | they light the whole city | `GL_LIGHT1` to `GL_LIGHT4` |
| Use `GL_CLIP_PLANE0` to `GL_CLIP_PLANE4` | they keep you in your plot | `GL_CLIP_PLANE5` |
| Open a window, a GL context or another GL library (GLFW, GLUT, GLEW, SDL...) | the city owns the window | the CityLab SDK (`#include <citylab/CityLab.h>`) |
| Start threads or programs, open network connections, exit the process | zones are guests in the city's process | nothing |

## Classic OpenGL only

The course teaches the fixed-function pipeline, so the project uses it:
`glBegin`/`glEnd`, the matrix stack, `glLight`, `glMaterial`, textures and
display lists. Buffer objects, vertex-array objects, framebuffers and shaders
are rejected by `lint`. Vertex arrays (`glVertexPointer`, `glDrawArrays`)
are allowed but give a warning, because the course does not use them.

## Time

- Drive **every** motion by `frame.dt` (seconds since the last frame) or by
  the city clock. A frame counter makes a fast computer move faster. Your
  zone must look the same with `--fixed-dt 0.1` and `--fixed-dt 0.008`.
- `frame.time` counts seconds since the zone started or was reset with F8.
- `frame.timeOfDay` is the shared clock in hours (0 to 24) and
  `frame.daylight` goes from 1 at noon to 0 at night. Switch your lights on
  with `daylight`, not with a fixed hour, so they agree with the sky.
- `reset()` puts every animation back to its start (F8).

## Budgets

Every plot gets the same share of the computer. The check measures them.

| What | Limit | Checked by |
|---|---|---|
| Vertices per frame, after display lists are expanded, counted as triangle corners (a quad is 6) | 100 000 | `check` (fail) |
| Average time of `draw()` | 6 ms | `check` (warning) |
| Own lights | `GL_LIGHT1` to `GL_LIGHT4` | `lint`, `check` |
| Textures | 16, each at most 1024 × 1024 (bigger images are scaled down) | the city (warning) |
| Colliders | 400 boxes | the city (warning) |
| Team folder | 15 MB | `lint`, `check` (fail) |

A display list does not make geometry free: what it draws still counts
towards the vertex budget. It makes drawing faster, which counts towards the
time budget.

## Colliders

From phase 3 the walls of your main structures are solid. Add boxes with
`addCollider(min, max)` in `init()`, in plot-local coordinates. A visitor
cannot walk through them; a box lower than 0.4 m can be stepped onto, which
is how stairs work. A box that lies completely outside the plot is ignored
with a warning. Keep the gate clear (see above).

## Your folder

- Change only `teams/teamNN/`. A pull request that changes any other file is
  rejected by CI (`boundary`). If the city needs a change, open an Issue.
- Allowed files: sources (`.cpp`, `.h`, `.hpp`), `.json`,
  `.md`, `.txt` and images (`.png`, `.jpg`, `.jpeg`, `.bmp`, `.tga`). No
  build products, no archives, no symbolic links.
- Put your classes in `namespace teamNN` (the name of your folder).
- `extras/` is outside the city build: the rules above do not apply there,
  and nothing in it is graded.
- `README.md` lists every image or file you did not make yourselves, with its
  source and licence.

## Mistakes the city forgives

A zone can crash or misbehave without taking the city down:

- An unbalanced `glBegin`/`glEnd`, `glPushMatrix`/`glPopMatrix`,
  `glPushAttrib`/`glPopAttrib` or `glNewList`/`glEndList` is repaired after
  your `draw()`, and reported once, by name, in the terminal and by `check`.
  OpenGL errors are reported the same way.
- An exception stops only your zone. Your plot shows the building site and
  the panel at the top left says why. Fix the code, build and start the city
  again; **F8** at your plot restarts a stopped zone without a new build.

Each repaired mistake is still a warning in the check report, and warnings
cost points in the *integration* column of [the grading](project.md#grading).

## What check reports

`python3 tools/citylab.py check team07` runs your zone alone, writes
`build/check/team07.json` and four screenshots (front, aerial, street and
night) in `build/check/team07/`, and prints one line per check:

| Check | Fails or warns when |
|---|---|
| team folder, folder size | `team.json` is missing or invalid; the folder is over 15 MB |
| registered, names on the sign | your team is not in `city/registry.json`; `title` or `title_en` is empty |
| starts and runs | `init()`, `update()` or `draw()` throws, or the zone stops |
| fast enough | `draw()` takes more than 6 ms on average |
| vertex budget, draws something | more than 100 000 vertices; no geometry at all |
| stays inside the plot | any vertex outside the plot volume |
| moving geometry | (information) whether anything moved in one second |
| lights | `GL_LIGHT0` was changed, or a light above `GL_LIGHT4` was used |
| textures, colliders | (information) how many you use |
| the gate is open | a visitor walking in from the street is stopped |
| OpenGL usage | anything the city had to repair, and OpenGL errors |

A red line (fail) must be fixed before you submit. A yellow line (warning)
costs points but does not stop the grading.
