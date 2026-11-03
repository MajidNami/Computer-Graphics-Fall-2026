# The project

Over the term your team builds one topic from the catalogue on the plot
that topic owns in the city: 80 × 100 metres for most topics, 160 × 100 for
the big ones. The project is built in three phases on the same plot; each
phase has its own document in [docs/phases](phases).

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
- A team that needs more room can be given the free plot next to it. Only
  the registry changes: your code reads the plot's size from `plot()`, and
  the borders grow by themselves.
- Zones are written in C++ (C++17) with classic OpenGL, using the CityLab
  SDK in `sdk/cpp/citylab/`.
