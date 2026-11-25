# team12 stadium architecture and checklist

The updated Allianz Arena plan governs team12. The official Phase 1 PDF and original CityLab SDK take precedence. Only the team directory is editable. CMake discovers sources under `src/`. `MyZone.cpp` registers one zone. Its `init(Context&)` reads plot dimensions, builds the shared layout and precomputes meshes and placement data. The const draw callback renders grounds and auxiliary buildings in plot coordinates, then applies the stadium transform once for the bowl, roof, shell, and pitch.

## Shared geometry contract

Plot origin is at ground center. +Y is up, +Z faces the street. The SDK reports 160 × 100 m for SP-1 + SP-2, with a 10 m gate. The first 6 m inside the gate remain clear. Dimensions below describe the confirmed plot in metres.

| Element | Center or reference | Dimensions |
| --- | --- | --- |
| Stadium | Plot (-18, -5), yaw 0° | 112 × 76 outer ellipse, 96 × 60 inner ellipse. Axes along X and Z |
| Shell and roof | Stadium origin | Plinth band to y = 4.4. Shell spring y = 20. Roof ridge y = 24, with an 82 × 51 rounded-rectangle opening |
| Pitch | Stadium (0, 0), yaw 0° | 70 × 36. Goal lines at x = ±35 and sidelines at z = ±18 |
| Goal and sideline reservations | Stadium-local | 3 m behind each goal line and 10 m wide around z = 0. 2 m runoff beside both sidelines within the ends of the pitch |
| Lower tier | First row radii (48.65, 30.65) | Three rows at 0.95 radial spacing. Seat surface y = 1.00, 1.45, 1.90. Treads y = 0.54, 0.99, 1.44 |
| Upper tier | First row radii (52.25, 34.25) | Three rows at 0.95 radial spacing. Seat surface y = 4.00, 4.48, 4.96. Treads y = 3.54, 4.02, 4.50 |
| Spectator portals | Four cardinal points of the outer ellipse | 4.8 m nominal opening width. Main +Z portal 4 m high, others 3.5 m. Interior aisles 4 m wide. Stair runs occupy one side |
| Player tunnel | Stadium z = -18, plot z = -23 | 3 m wide, 3.2 m high shell opening. Reserved stadium x = 36 to about 49.32. Exterior route continues to plot x = 47 |
| Player facilities | Plot (59, -23) | 24 × 18. Public +Z approach and west-side player approach |
| Ticket office | Plot (59, 3) | 22 × 12. Public +Z approach |
| Fan shop | Plot (59, 25) | 24 × 16. Public +Z approach |
| Football monument | Plot (15, 40) | 5 × 5 reservation beside the gate approach |

The 76 × 44 m pitch proposed in the plan crosses the chosen inner ellipse at its corners. The 70 × 36 m pitch fits the goal and sideline reservations above. Actual goals, nets, technical areas, tunnel finishing, and door colliders must be checked when Mahan integrates his geometry. The eastern 4 m spine joins the entrance plaza and all public approaches. The shell portal and clear bowl opening are available for the player route.

## Seat placement and ownership

`Seating.cpp` produces **520** shared placements for eight sections and 24 rows. The nominal slot ID is assigned before filtering four aisles and the tunnel. Each record carries stadium-local seat-surface center, yaw in degrees around +Y, zero-based tier/row/sector, and eligibility. The model's +Z faces the pitch. In `SeatingBowl.cpp`, the seat transform is **Stadium → Seating Section → Row → Seat**. Row-local seat translations reconstruct the exact original placement. The molded seat uses six triangles per instance. Take spectators from the shared vector in init and keep spectator rendering under the same stadium root.

| Owner | Module | Working cap | Stage vertices |
| --- | --- | ---: | ---: |
| Amirabbas | Shell and façade | 14,000 | 13,752 |
| Amirabbas | Roof, supports, interior banners and their lights | 7,000 | 6,690 |
| Amirabbas | Seating, stairs and railings | 20,000 | 18,900 |
| Amirabbas | Grounds and monument | 8,000 | 7,836 |
| Mahan | Pitch, goals and equipment, including scoreboard | 6,000 | 3,796 |
| Mahan | Spectators and flags | 10,000 | Pending |
| Mahan | Player facilities and tunnel | 8,000 | Placeholder |
| Mahan | Ticket office and fan shop | 7,000 | Placeholder |

Both members share layout, helpers, the entry point, integration, testing, and documentation. The scene totals **51,100** checker-counted vertices, including 108 for the floodlight fixture and 18 from three auxiliary building pads. Filling Mahan's allocations projects **78,286** vertices if these placeholders are replaced. The city limit is 100,000, with an 80,000 working target. There are 314 registered colliders of the SDK's 400-box team allowance. Reserve the remaining 86 for later integration.

## Implementation checklist and measured checkpoints

- [x] Bootstrap. Official template 180 vertices, revised preview 468. SDK lifecycle, double plot, and baseline setup verified.
- [x] A1. Closed oval shell segments, 192 staggered cushions, structural portal jambs, and five ground-level openings. Shell checkpoint 13,284 vertices in the scene. Street view inspected. Final cushions use 12 noncoplanar polygon faces, represented by 16 triangles per cushion. CPU edge-pair checks passed for the cushions.
- [x] A2. Closed annular roof, columns, inclined nonuniformly scaled rafters, open pitch view, and entrance canopy hinge. Roof checkpoint 17,424 vertices. Aerial and interior views inspected. The canopy uses explicit T(p) Rₓ(12°) T(-p), with p = (0, 4.8, 37.6) in stadium coordinates.
- [x] A3. Two distinct tiers using closed handmade tread wedges, 520 composite seats, aisles with stairs and handrails, and section/row/seat transforms. Seating checkpoint 36,516 vertices. Interior capture inspected. CPU checks verified matched directed edges on the tread wedges and reconstructed all seat positions.
- [x] A4 stage. Full plot ground, plaza and path reservations, planting beds, trees, lamps, benches, bollards, and the street-visible football monument. Landscaping checkpoint 44,460 vertices. Aerial, street and monument views inspected. The monument has five separately drawn pedestal/trim pieces plus a black-pentagon and white-hexagon ball, and exceeds 4 m. Lamps, benches, trees, and bollards each have at least five placed instances.
- [x] Integrated Amirabbas stage. CityLab build, lint, and check passed in the isolated build directory. Final check 44,652 scene vertices, zero checker failures or warnings. The CPU walk used CityLab collision against the current zone and reached the stadium interior, player tunnel, and all three auxiliary sites. It also traversed both reserved sideline and goal boundaries and reached the upper stair landing at y = 4.50 m. The facilities are currently pads.
- [x] Interior revision. Red molded seats with grouped white accents, increased roof coverage, and curved FC BAYERN and MIA SAN MIA wall banners. The 64-segment opening leaves about 2.31 m at the pitch corner and 0.77 m at the sideline runoff corner in plan view. CityLab check and lint passed at 46,734 scene vertices. Visual reference is the [official Allianz Arena interior gallery](https://allianz-arena.com/de/galleries/2018/bauarbeiten-sommer-2018_3).
- [x] M1 pitch. Striped grass, touchlines and goal lines, center line/circle/spot, penalty and goal areas, penalty spots and arcs, corner arcs, and two mirrored goals with fixed grid nets. The goal post center spacing is 5.8 m and rear depth is 2.4 m. Check and lint passed at 48,850 scene vertices. Inspected the close-up goal and overview.
- [x] Night lighting. Red and white facade cushions gain emission as CityLab daylight fades. One roof-mounted spotlight (GL_LIGHT1) illuminates the pitch and goals only. Day and night captures inspected. Official check and lint passed at 48,958 vertices, with zero extra lights by day and one at night.
- [x] Scoreboard and banner lighting. The static display reads BAYERN 8-2 BARCELONA with a full-time match clock at 90:00. Each banner has three wall-mounted fixtures. GL_LIGHT2 and GL_LIGHT3 are scoped to the banner draw, and GL_LIGHT1 still illuminates the pitch. Night captures inspected. Check and lint passed at 51,100 vertices.
- [ ] M1 equipment. Dugouts and flags.
- [ ] M2. Spectator selection, seated models and static banners.
- [ ] M3. Player facilities, dressing rooms, finished tunnel and locker-door pivot.
- [ ] M4. Ticket office and fan shop with accessible frontages and eight or more components each.
- [ ] Verify complete plot coverage, all four structures, goal and tunnel geometry, final access and collider count, root-transform demonstration, and final visual/rubric evidence after integration.
- [ ] Complete the README report and images, including student IDs, matrix and vertex experiments, contributions and AI use. Pack and submit after final verification.
