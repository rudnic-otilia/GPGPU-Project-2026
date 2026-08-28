#include "physics_engine.h"
#include "virtual-world-physics/bounding_volume.h"
#include "virtual-world-physics/collision.h"
#include "virtual-world-physics/physics_gpu.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace physics {
void PhysicsEngine::ClearObjects() { m_objects.clear(); }

void PhysicsEngine::Update(float deltaTime) {
  // Reset statistics
  m_stats.detectedCollisions = 0;
  m_stats.objectCount = static_cast<int>(m_objects.size());

  auto startTime = std::chrono::high_resolution_clock::now();

  // Fixed timestep with accumulator
  m_accumulator += deltaTime;
  int subSteps = 0;
  while (m_accumulator >= m_fixedDeltaTime && subSteps < m_maxSubSteps) {
    ApplyGravity(m_fixedDeltaTime);
    for (auto &object : m_objects) {
      object.Integrate(m_fixedDeltaTime);
    }
    if (m_useGPU && m_gpuDetector) {
      try {
        auto collisions = m_gpuDetector->DetectCollisions(m_objects);

        m_stats.detectedCollisions = collisions.size();

        for (const auto &collision : collisions) {
          ResolveCollision(collision.indexA, collision.indexB, collision);
        }
      } catch (const std::exception &ex) {
        std::cerr << "CUDA collision backend failed, falling back to CPU: "
                  << ex.what() << std::endl;
        m_useGPU = false;
        BroadPhase();
        NarrowPhase();
      }
    } else {
      BroadPhase();
      NarrowPhase();
    }

    m_accumulator -= m_fixedDeltaTime;
    subSteps++;
  }
  if (m_accumulator > m_fixedDeltaTime) {
    m_accumulator = 0.0f;
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  m_stats.collisionDetectionTime =
      std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void PhysicsEngine::ApplyGravity(float deltaTime) {
    // TODO
    for (auto& object : m_objects) {
        if (!object.isStatic) {
            object.velocity += m_gravity * deltaTime;
        }
        else {
            continue;
        }
    }
}

void PhysicsEngine::BroadPhase() {  
  // TODO
    std::vector<std::pair<size_t, size_t>> potentialPairs = GetPotentialCollisionPairs();
}


std::vector<std::pair<size_t, size_t>> PhysicsEngine::GetPotentialCollisionPairs() {
  // TODO
    std::vector<std::pair<size_t, size_t>> potentialPairs;
    for (int i = 0; i < m_objects.size(); i++) {
        for (int j = i + 1; j < m_objects.size(); j++) {
            if (m_objects[i].isStatic && m_objects[j].isStatic) {
                continue;
            }
            if (Intersects(m_objects[i].boundingVolume, m_objects[j].boundingVolume)) {
                potentialPairs.emplace_back(i, j);
            }
        }
    }
  return potentialPairs;
}

void PhysicsEngine::NarrowPhase() {
  // TODO
    std::vector<std::pair<size_t, size_t>> potentialPairs = GetPotentialCollisionPairs();

    for (auto &pair : potentialPairs) {
        CollisionInfo collision = DetectCollision(pair.first, pair.second);
        if (collision.isValid) {
            ResolveCollision(pair.first, pair.second, collision);
            m_stats.detectedCollisions++;
        }
    }
}

CollisionInfo PhysicsEngine::DetectCollision(size_t indexA, size_t indexB) {
  // TODO
  return ComputeBoxBoxCollision(indexA, indexB, m_objects[indexA].boundingVolume, m_objects[indexB].boundingVolume);
}

CollisionInfo PhysicsEngine::ComputeBoxBoxCollision(size_t indexA, size_t indexB,
                                      const bounding_volume_t &boxA,
                                      const bounding_volume_t &boxB) {
  // TODO
    CollisionInfo info{};
    info.indexA = indexA;
    info.indexB = indexB;

    glm::vec3 delta = boxB.center - boxA.center;

    float overlapX = boxA.sizes.x + boxB.sizes.x - std::abs(delta.x);
    float overlapY = boxA.sizes.y + boxB.sizes.y - std::abs(delta.y);
    float overlapZ = boxA.sizes.z + boxB.sizes.z - std::abs(delta.z);

    if (overlapX <= 0.0f || overlapY <= 0.0f || overlapZ <= 0.0f) {
        info.isValid = false;
        return info;
    }

    info.isValid = true;

    if (overlapX < overlapY && overlapX < overlapZ) {
        info.penetration = overlapX;
        info.normal = glm::vec3((delta.x > 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);
    }
    else if (overlapY < overlapZ) {
        info.penetration = overlapY;
        info.normal = glm::vec3(0.0f, (delta.y > 0.0f) ? 1.0f : -1.0f, 0.0f);
    }
    else {
        info.penetration = overlapZ;
        info.normal = glm::vec3(0.0f, 0.0f, (delta.z > 0.0f) ? 1.0f : -1.0f);
    }

    return info;
}

void PhysicsEngine::ResolveCollision(size_t indexA, size_t indexB,
    const CollisionInfo& collision) {
    auto& objA = m_objects[indexA];
    auto& objB = m_objects[indexB];

    float invMassA = (objA.isStatic || objA.mass <= 0.0f) ? 0.0f : (1.0f / objA.mass);
    float invMassB = (objB.isStatic || objB.mass <= 0.0f) ? 0.0f : (1.0f / objB.mass);

    float totalInvMass = invMassA + invMassB;
    if (totalInvMass <= 0.0f) {
        return;
    }

    // 1. Separare pozițională imediată
    glm::vec3 correction = collision.normal * (collision.penetration / totalInvMass);
    objA.position -= correction * invMassA;
    objB.position += correction * invMassB;

    objA.UpdateBoundingVolume();
    objB.UpdateBoundingVolume();

    // 2. Viteza relativă
    glm::vec3 rv = objB.velocity - objA.velocity;
    float velAlongNormal = glm::dot(rv, collision.normal);

    // Nu aplicăm impuls dacă deja se îndepărtează
    if (velAlongNormal > 0.0f) {
        return;
    }

    float e = std::min(objA.restitution, objB.restitution);

    // Dacă viteza este foarte mică (repaus), setăm restituția la 0 ca să nu vibreze/sară la infinit
    if (std::abs(velAlongNormal) < 0.1f) {
        e = 0.0f;
    }

    float j = -(1.0f + e) * velAlongNormal;
    j /= totalInvMass;

    glm::vec3 impulse = collision.normal * j;
    objA.velocity -= impulse * invMassA;
    objB.velocity += impulse * invMassB;
}

void PhysicsEngine::Init(const glm::vec3 &gravity, bool useGpu) {
  m_gravity = gravity;
  m_useGPU = false;

  delete m_gpuDetector;
  m_gpuDetector = nullptr;

  if (useGpu) {
    m_gpuDetector = new GPUCollisionDetector();
    if (m_gpuDetector->Initialize()) {
      m_useGPU = true;
    } else {
      delete m_gpuDetector;
      m_gpuDetector = nullptr;
      std::cerr << "CUDA collision backend unavailable; using CPU backend."
                << std::endl;
    }
  }
}

PhysicsEngine::~PhysicsEngine() { delete m_gpuDetector; }
} // namespace physics
