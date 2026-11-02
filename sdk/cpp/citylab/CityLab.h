#pragma once

// The only header a C++ team needs:
//
//     #include <citylab/CityLab.h>
//
// It brings in classic OpenGL, GLM, the Zone and Context interfaces, the
// shape helpers and the city's props (lamps, benches, flower beds). Each header
// explains its part of the API; README.md ("Writing your zone") has an overview.
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "citylab/Context.h"
#include "citylab/OpenGL.h"
#include "citylab/Props.h"
#include "citylab/Shapes.h"
#include "citylab/Zone.h"
