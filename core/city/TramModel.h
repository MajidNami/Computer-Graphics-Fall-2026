#pragma once

namespace citylab::tram {

// Geometry in tram-local metres: nose at z = 0, tail at z = -27.
// Called inside display lists; material and texture state is restored by the caller.
void body();
void lights(float cabinGlow);
// One wheel is cached once; the instances rotate with distance travelled.
void wheel();
void wheels(float distance, unsigned int wheelList);
// Alpha-cut portrait in the active cab, under the wiper and above the console.
void driver(unsigned int texture, bool night);

} // namespace citylab::tram
