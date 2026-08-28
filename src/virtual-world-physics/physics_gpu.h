#pragma once

#include <cstddef>
#include <vector>
#include <glm/glm.hpp>

#include "physics_object.h"

namespace physics {

	typedef struct CollisionInfo_ {
		bool isValid{ false };
		size_t indexA{ 0 };
		size_t indexB{ 0 };
		glm::vec3 normal{ 0.0f };
		float penetration{ 0.0f };
	} CollisionInfo;

	class GPUCollisionDetector {
	public:
		GPUCollisionDetector() = default;
		~GPUCollisionDetector();

		bool Initialize();

		std::vector<CollisionInfo> DetectCollisions(const std::vector<PhysicsObject>& objects); // TODO: implement in physics_gpu_cuda.cu

	private:
		bool m_initialized{ false };
		glm::vec3* d_centers = nullptr;
		glm::vec3* d_sizes = nullptr;
		bool* d_isStatic = nullptr;

		CollisionInfo* d_collisions = nullptr;
		int* d_collisionCount = nullptr;

		int m_maxObjects = 0;
		const int m_maxCollisions = 100000;

		// Sweep and Prune
		float* d_minX = nullptr;
		int* d_objectIndices = nullptr;
	};

} // namespace physics