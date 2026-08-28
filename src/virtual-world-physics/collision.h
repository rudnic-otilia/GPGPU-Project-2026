#pragma once

#include "bounding_volume.h"
#include <glm/glm.hpp>

namespace physics {

inline bool Intersects(const bounding_volume_t &boxA,
                       const bounding_volume_t &boxB) {
    // Write a simple AABB/OOBB intersection test   
    glm::vec3 delta = glm::abs(boxB.center - boxA.center);
    glm::vec3 totalHalfSizes = boxA.sizes + boxB.sizes;

    return (delta.x <= totalHalfSizes.x) &&
        (delta.y <= totalHalfSizes.y) &&
        (delta.z <= totalHalfSizes.z);
}

} // namespace physics
