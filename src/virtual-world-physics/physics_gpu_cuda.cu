#ifndef __CUDACC_VER__
#define __CUDACC_VER__ 120000
#endif

#define CCCL_IGNORE_MSVC_TRADITIONAL_PREPROCESSOR_WARNING 1

#include <glm/glm.hpp>

#define GLM_FORCE_CUDA
#define GLM_ENABLE_EXPERIMENTAL

#include "physics_gpu.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include <thrust/sort.h>
#include <thrust/execution_policy.h>

#include <iostream>
#include <algorithm>
#include <memory>

namespace physics {

    __global__ void ComputeBoundsKernel(int numObjects, const glm::vec3* centers, const glm::vec3* sizes, float* outMinX, int* outIndices) {
        int i = blockIdx.x * blockDim.x + threadIdx.x;
        if (i < numObjects) {
            outMinX[i] = centers[i].x - sizes[i].x;
            outIndices[i] = i;
        }
    }

    __global__ void PhysicsCollisionKernel(
        int numObjects,
        const glm::vec3* centers,
        const glm::vec3* sizes,
        const bool* isStatic,
        const int* sortedIndices,
        CollisionInfo* outCollisions,
        int* outCount,
        int maxCollisions)
    {
        int threadId = blockIdx.x * blockDim.x + threadIdx.x;
        if (threadId >= numObjects) return;

        int i = sortedIndices[threadId];

        if (isStatic[i]) return;

        for (int s = 0; s < numObjects; ++s) {
            if (!isStatic[s]) continue; 

            glm::vec3 delta = centers[s] - centers[i];

            float overlapX = sizes[i].x + sizes[s].x - fabsf(delta.x);
            if (overlapX <= 0.0f) continue;

            float overlapY = sizes[i].y + sizes[s].y - fabsf(delta.y);
            if (overlapY <= 0.0f) continue;

            float overlapZ = sizes[i].z + sizes[s].z - fabsf(delta.z);
            if (overlapZ <= 0.0f) continue;

            int idx = atomicAdd(outCount, 1);
            if (idx < maxCollisions) {
                CollisionInfo info;
                info.isValid = true;
                info.indexA = i;
                info.indexB = s;

                if (overlapX < overlapY && overlapX < overlapZ) {
                    info.penetration = overlapX;
                    info.normal = glm::vec3((delta.x >= 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);
                }
                else if (overlapY < overlapZ) {
                    info.penetration = overlapY;
                    info.normal = glm::vec3(0.0f, (delta.y >= 0.0f) ? 1.0f : -1.0f, 0.0f);
                }
                else {
                    info.penetration = overlapZ;
                    info.normal = glm::vec3(0.0f, 0.0f, (delta.z >= 0.0f) ? 1.0f : -1.0f);
                }
                outCollisions[idx] = info;
            }
        }

        float myMaxX = centers[i].x + sizes[i].x;

        for (int nextIdx = threadId + 1; nextIdx < numObjects; ++nextIdx) {
            int j = sortedIndices[nextIdx];
            if (isStatic[j]) continue;

            float otherMinX = centers[j].x - sizes[j].x;

            if (otherMinX > myMaxX) {
                break;
            }

            glm::vec3 delta = centers[j] - centers[i];

            float overlapX = sizes[i].x + sizes[j].x - fabsf(delta.x);
            if (overlapX <= 0.0f) continue;

            float overlapY = sizes[i].y + sizes[j].y - fabsf(delta.y);
            if (overlapY <= 0.0f) continue;

            float overlapZ = sizes[i].z + sizes[j].z - fabsf(delta.z);
            if (overlapZ <= 0.0f) continue;

            int idx = atomicAdd(outCount, 1);
            if (idx < maxCollisions) {
                CollisionInfo info;
                info.isValid = true;
                info.indexA = i;
                info.indexB = j;

                if (overlapX < overlapY && overlapX < overlapZ) {
                    info.penetration = overlapX;
                    info.normal = glm::vec3((delta.x >= 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);
                }
                else if (overlapY < overlapZ) {
                    info.penetration = overlapY;
                    info.normal = glm::vec3(0.0f, (delta.y >= 0.0f) ? 1.0f : -1.0f, 0.0f);
                }
                else {
                    info.penetration = overlapZ;
                    info.normal = glm::vec3(0.0f, 0.0f, (delta.z >= 0.0f) ? 1.0f : -1.0f);
                }
                outCollisions[idx] = info;
            }
        }
    }

    bool GPUCollisionDetector::Initialize() {
        cudaError_t err = cudaMalloc(&d_collisionCount, sizeof(int));
        if (err != cudaSuccess) return false;

        err = cudaMalloc(&d_collisions, m_maxCollisions * sizeof(CollisionInfo));
        if (err != cudaSuccess) return false;

        return true;
    }

    std::vector<CollisionInfo> GPUCollisionDetector::DetectCollisions(const std::vector<PhysicsObject>& objects) {
        int numObjects = static_cast<int>(objects.size());
        if (numObjects == 0) return {};

        if (numObjects > m_maxObjects) {
            cudaFree(d_centers);
            cudaFree(d_sizes);
            cudaFree(d_isStatic);
            cudaFree(d_minX);
            cudaFree(d_objectIndices);

            cudaMalloc(&d_centers, numObjects * sizeof(glm::vec3));
            cudaMalloc(&d_sizes, numObjects * sizeof(glm::vec3));
            cudaMalloc(&d_isStatic, numObjects * sizeof(bool));
            cudaMalloc(&d_minX, numObjects * sizeof(float));
            cudaMalloc(&d_objectIndices, numObjects * sizeof(int));

            m_maxObjects = numObjects;
        }

        std::vector<glm::vec3> host_centers(numObjects);
        std::vector<glm::vec3> host_sizes(numObjects);
        std::unique_ptr<bool[]> host_isStatic(new bool[numObjects]);

        for (int i = 0; i < numObjects; ++i) {
            host_centers[i] = objects[i].boundingVolume.center;
            host_sizes[i] = objects[i].boundingVolume.sizes;
            host_isStatic[i] = objects[i].isStatic;
        }

        cudaMemcpy(d_centers, host_centers.data(), numObjects * sizeof(glm::vec3), cudaMemcpyHostToDevice);
        cudaMemcpy(d_sizes, host_sizes.data(), numObjects * sizeof(glm::vec3), cudaMemcpyHostToDevice);
        cudaMemcpy(d_isStatic, host_isStatic.get(), numObjects * sizeof(bool), cudaMemcpyHostToDevice);

        int zero = 0;
        cudaMemcpy(d_collisionCount, &zero, sizeof(int), cudaMemcpyHostToDevice);

        int threadsPerBlock = 256;
        int blocks = (numObjects + threadsPerBlock - 1) / threadsPerBlock;

        ComputeBoundsKernel << <blocks, threadsPerBlock >> > (numObjects, d_centers, d_sizes, d_minX, d_objectIndices);
        cudaDeviceSynchronize();

        thrust::sort_by_key(thrust::device, d_minX, d_minX + numObjects, d_objectIndices);

        PhysicsCollisionKernel << <blocks, threadsPerBlock >> > (
            numObjects, d_centers, d_sizes, d_isStatic, d_objectIndices, d_collisions, d_collisionCount, m_maxCollisions
            );
        cudaDeviceSynchronize();

        int host_collisionCount = 0;
        cudaMemcpy(&host_collisionCount, d_collisionCount, sizeof(int), cudaMemcpyDeviceToHost);

        int validCollisions = std::min(host_collisionCount, m_maxCollisions);
        std::vector<CollisionInfo> result(validCollisions);

        if (validCollisions > 0) {
            cudaMemcpy(result.data(), d_collisions, validCollisions * sizeof(CollisionInfo), cudaMemcpyDeviceToHost);
        }

        return result;
    }

    GPUCollisionDetector::~GPUCollisionDetector() {
        cudaFree(d_centers);
        cudaFree(d_sizes);
        cudaFree(d_isStatic);
        cudaFree(d_minX);
        cudaFree(d_objectIndices);
        cudaFree(d_collisions);
        cudaFree(d_collisionCount);
    }

} // namespace physics