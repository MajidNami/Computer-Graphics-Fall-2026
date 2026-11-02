#include "city/Props.h"

#include "citylab/Props.h"

namespace citylab {
namespace {

template <typename Draw>
GLuint compile(Draw&& draw) {
    const GLuint list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    draw();
    glEndList();
    return list;
}

}  // namespace

void Props::build(const Materials& m) {
    // The city's own textures on the SDK objects teams can use too.
    props::Textures t;
    t.leaves = m.foliage;
    t.wood = m.wood;
    t.concrete = m.concrete;
    t.metal = m.plain;
    t.glass = m.glass;
    flowerBed = compile([&] { props::flowerBed(t); });
    streetLamp = compile([&] { props::streetLampPost(t); });
    lampHead = compile([] { props::streetLampHead(); });
    parkLamp = compile([&] { props::parkLampPost(t); });
    parkGlobe = compile([] { props::parkLampGlobe(); });
    bench = compile([&] { props::bench(t); });
    busShelter = compile([&] { props::busShelter(t); });
    craneMast = compile([&] { props::craneMast(t); });
    craneJib = compile([&] { props::craneJib(t); });
}

}  // namespace citylab
