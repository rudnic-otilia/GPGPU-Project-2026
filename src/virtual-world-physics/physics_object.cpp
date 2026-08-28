#include "physics_object.h"
#include <glm/gtc/matrix_transform.hpp>

namespace physics {
glm::mat4 PhysicsObject::GetModelMatrix() const {
  glm::mat4 modelMatrix = glm::mat4(1.0f);
  modelMatrix = glm::translate(modelMatrix, position);
  modelMatrix = glm::scale(modelMatrix, scale);
  return modelMatrix;
}

void PhysicsObject::UpdateBoundingVolume() {
  // TODO
	boundingVolume.center = position;
    boundingVolume.sizes = scale * 0.5f;
}

void PhysicsObject::ApplyForce(const glm::vec3 &force) {
  // TODO
	if (isStatic || mass <= 0.0f) return;
	acceleration += force / mass;
}

void PhysicsObject::ApplyImpulse(const glm::vec3 &impulse) {
  // TODO
	if (isStatic || mass <= 0.0f) return;
	velocity += impulse / mass;
}

void PhysicsObject::Integrate(float deltaTime) {
    if (isStatic) return;

    velocity += acceleration * deltaTime;

    velocity *= 0.999f;

    if (glm::length(velocity) < 0.01f) {
        velocity = glm::vec3(0.0f);
    }
    position += velocity * deltaTime;

    UpdateBoundingVolume();

    acceleration = glm::vec3(0.0f);
}
} // namespace physics
