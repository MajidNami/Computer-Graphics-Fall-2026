# The project

Over the term your team builds one topic from the catalogue on the plot
that topic owns in the city: 80 × 100 metres for most topics, 160 × 100 for
the big ones. Each phase adds to the same plot, and each phase practises
one part of the course: modelling, then animation, then light and texture.
Artistic skill is not what is graded. What counts is that the graphics
concepts are there, work correctly and that you can explain them.

> The dates follow the course calendar and are announced in class. Each
> phase asks only for what the course has taught before its deadline: phase 1
> is due around session 14, phase 2 around session 20 and phase 3 two weeks
> after session 25. The weights below are the current plan and may still
> change; the version announced with each phase is the one that counts.

## Teams and topics

- Teams have two members. Both members must understand and be able to
  explain every part of the code.
- The staff publish the catalogue of topics ([docs/city.md](city.md),
  `city/places.json`). Every topic is big enough for a whole project and has
  a fixed plot in a district that suits it: the airport at the edge of the
  city, the railway station by the tram line, the amusement park by the
  central park. The airport, the railway station, the amusement park and the
  stadium have double plots (160 × 100 m).
- Each team sends three preferences; each topic goes to one team, by lot if
  two teams want the same topic. A new idea can be proposed with an Issue.
- The catalogue suggests what each phase could add to each topic. These are
  starting points, not requirements.
- A team that needs more room can be given the free plot next to it. Only
  the registry changes: your code reads the plot's size from `plot()`, and
  the borders grow by themselves.
- Zones are written in C++ (C++17) with classic OpenGL, using the CityLab
  SDK in `sdk/cpp/citylab/`.

## Phase 1: Build the plot

**Topics:** transforms, the matrix stack, hierarchical modelling, display
lists (sessions 6–11).
**Outcome:** a coherent static plot on your theme that loads in the city
without warnings.

Requirements:

1. Ground work for the whole plot (paths, paving, some green) and a main
   entrance at the gate (z = +50).
2. At least **3 main structures and 1 distinctive object**, all inside the
   plot box (80 wide, 40 high, 100 deep, and at most 5 below ground), with
   the main façade towards the street
   ([the plot contract](rules.md#the-plot-contract)).
3. Meaningful use of `glTranslatef`, `glRotatef` and **non-uniform**
   `glScalef`, with balanced `glPushMatrix`/`glPopMatrix`.
4. One geometry reused **at least 5 times** with different transforms,
   through a function or a display list. Vertices are not copied.
5. At least one **custom mesh** that you modelled vertex by vertex (a sloped
   roof, an arch, a prism). Normals are not required yet; phase 3 adds
   them.
6. A **static hierarchy** with at least two levels of local frames (building
   → floor → window).
7. The state of each object (its transform) is kept apart from its raw
   geometry.
8. A `README.md` in your team folder in the *Math in Code* form: one
   representative chain of transforms written as a matrix product (for
   example M = T · R<sub>y</sub> · S), where it is in the code, and two
   screenshots taken with **F12**.

## Phase 2: Bring it to life

**Topics:** time-based animation, pivots, hierarchical motion, curves,
interpolation (sessions 14–16).
**Outcome:** a plot that lives with the shared city clock.

1. All motion is driven by `dt` or the city clock; frame counters are not
   allowed. The result must look the same with `--fixed-dt 0.1` and
   `--fixed-dt 0.008`.
2. At least **3 moving elements, each showing a different idea**:
   - a rotation about an **off-centre pivot**;
   - motion along a **cubic Bézier path** that you implement yourselves (de
     Casteljau or Bernstein), with the object oriented along the tangent;
   - a **moving hierarchy** with at least two moving levels and correct
     pivots (T · R · T<sup>−1</sup>).

   Three elements of the same kind (three fans) count as one. At least one
   must be visible from the street.
3. **Interpolation:** `lerp` and an easing function for at least one motion.
4. At least one behaviour driven by the **time of day** (`timeOfDay`): a shop
   that opens in the morning, a bell at noon.
5. `reset()` restarts every animation when **F8** is pressed. Moving objects
   stay inside the plot.
6. Report: for each moving object its transform tree, pivot and the role of
   time; the Bézier formula and where it is in the code; a short GIF or
   video.

## Phase 3: Light and finish

**Topics:** normals, the lighting model, materials, texture mapping and
filtering (sessions 19–22).
**Outcome:** a plot that looks right under the shared sun and at night, and
fits with the rest of the city.

1. Correct normals on every surface, including your custom meshes from
   phase 1 and objects with non-uniform scale. Explain why `GL_NORMALIZE` is
   needed.
2. The plot responds to the sun (from the city) and has **at least one local
   point or spot light** on `GL_LIGHT1`…`GL_LIGHT4` that switches on at night
   using `daylight`.
3. **At least 2 different materials** with `glMaterialfv`, including specular
   colour and shininess.
4. **At least 2 textured surfaces** with meaningful UVs, one of them with
   `GL_REPEAT`. Explain the filtering you chose (nearest, linear, mipmaps).
5. Integration: phases 1 and 2 still work, `check` passes without errors,
   the plot fits the city's fog and light, and the main structures have
   colliders (`addCollider`) so visitors cannot walk through walls.
6. Final report, a **30–60 second video** (day and night) and a **10-minute
   defence** in person, in which both members can explain any part of the
   code.

## Grading

| Phase | Concepts implemented | Integration and code structure | Explanation and report | Bonus (cap) |
|---|---|---|---|---|
| 1 | 55% | 30% | 15% | +10% |
| 2 | 55% | 30% | 15% | +15% |
| 3 | 60% | 25% | 15% | +20% |

The *integration* column includes the city contract: each error reported by
`check` (leaving the plot, budgets, OpenGL errors, unbalanced push and pop)
costs points in this column, and a submission that does not build earns
nothing in it until it is fixed.

Each submission is graded in three steps: the automatic `check` report, the
phase checklist that a TA fills in while running your zone, and one question
about a transform, an animation or a light of the TA's choosing. Both
members get the same grade, unless the commit history and the defence show
that one of them contributed very little.

## Bonus ideas

Bonus work never replaces a requirement, and counts only if it stays inside
the city contract (borders, budgets, classic OpenGL). Each idea is tied to a
part of the course. Points are a percentage of the phase, up to the cap in
the table above.

| Idea | Phase | Course topic | Difficulty | Points |
|---|---|---|---|---|
| Procedural trees or plants with a recursive L-system | 1 | hierarchical transforms | medium | 5 |
| A minimap of the plot drawn with your HW02 software rasterizer (Bresenham, scan-line) as a texture on a sign | 1 or 3 | rasterization | medium | 8 |
| A Bézier surface for a roof or canopy (`glMap2f`/`glEvalMesh2` or your own) | 2 or 3 | curves and surfaces | medium | 8 |
| A closed Catmull-Rom or B-spline path with C1 continuity and constant speed (arc-length parameterisation) | 2 | curves | medium | 6 |
| A particle system (smoke, steam, water) with billboards and correct blending (depth writes off, sorting) | 2 or 3 | alpha blending | medium | 8 |
| A planar shadow from the shared sun with a projection matrix, following the city clock | 3 | transforms and light | medium | 8 |
| A reflection on water or a shiny floor with the stencil buffer and a reflection matrix | 3 | reflection transforms, depth | hard | 10 |
| An animated texture (cinema screen, LED board) with the `GL_TEXTURE` matrix | 3 | texture mapping | easy | 4 |
| Hand-made mipmaps (your own box filter) and a visual comparison of filters | 3 | texture filtering | medium | 6 |
| Picking objects with a ray from the camera, with your own ray–box or ray–sphere test | 3 | ray casting | medium | 8 |
| A small CPU ray tracer that renders your plot's scene with shadows and reflections | 3 | ray tracing | hard | 12 |
| An octree or grid for culling many objects (500 amphitheatre seats), measured before and after | 3 | spatial structures | hard | 10 |

A new idea can be proposed with an Issue and must be approved before you
start. Buffers, vertex arrays and shaders are not part of the course
project and earn no bonus.

## Submitting a phase

A phase is submitted in two places:

1. **GitHub:** a pull request from your fork with the last commit before the
   deadline. The staff merge green pull requests so the whole city stays
   visible.
2. **Courses:** the zip made by

   ```sh
   python3 tools/citylab.py pack team07 --phase 1    # Windows: py tools\citylab.py pack team07 --phase 1
   ```

   It is written to `dist/CG_PROJ_P1_team07.zip` and holds your team folder,
   the check report, the screenshots and the commit it was made from.

Late days come out of the course's fixed 72-hour allowance.
[docs/workflow.md](workflow.md) walks through the steps.
