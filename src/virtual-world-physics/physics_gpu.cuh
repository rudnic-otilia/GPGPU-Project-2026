#pragma once

// Define here functions and auxiliaries for GPU physics computations.
#define GLM_FORCE_CUDA
#define GLM_ENABLE_EXPERIMENTAL

#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "physics_object.h"
#include <vector>

namespace physics {


    class GPUCollisionDetector {
    public:
        GPUCollisionDetector() = default;
        ~GPUCollisionDetector();

        bool Initialize();

    private:
        glm::vec3* d_centers = nullptr;
        glm::vec3* d_sizes = nullptr;
        bool* d_isStatic = nullptr;

        CollisionInfo* d_collisions = nullptr;
        int* d_collisionCount = nullptr;

        int m_maxObjects = 0;
        const int m_maxCollisions = 100000;
    };

} // namespace physics