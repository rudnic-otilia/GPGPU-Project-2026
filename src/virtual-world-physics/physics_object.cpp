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
    boundingVolume.sizes = (scale + 0.2f) * 0.5f;
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
  // TODO
    if (isStatic) return;

    // 1. Actualizează viteza pe baza accelerației acumulate
    velocity += acceleration * deltaTime;

    // 2. Actualizează poziția pe baza noii viteze
    position += velocity * deltaTime;

    // 3. Sincronizează bounding volume-ul cu noua poziție
    UpdateBoundingVolume();

    // 4. Resetează accelerația pentru cadrul următor
    acceleration = glm::vec3(0.0f);
}
} // namespace physics
