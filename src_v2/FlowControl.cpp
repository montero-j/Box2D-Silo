#include "FlowControl.h"
#include <algorithm>
#include <iomanip>

// =================================================================================================
// 2. RAYCAST Y REINYECCIÓN DE ARCO
// =================================================================================================



void detectAndReinjectArchViaRaycast(b2WorldId worldId) {
    const float REINJECT_HEIGHT = silo_height * REINJECT_HEIGHT_RATIO;
    float baseRange = OUTLET_WIDTH * 2.0f;
    float progressiveMultiplier = 1.0f + (blockageRetryCount * 0.5f);
    float maxRange = std::min(silo_height * 0.05f, silo_height * 0.5f);

    const int numRays = 120;
    const float maxAngle = M_PI / 2.0f;
    const b2Vec2 origin = {0.0f, -0.1f};

    const int maxInternalRetries = 3;
    const float internalGrowth = 1.5f;

    RaycastUserData raycastData;
    bool anyHit = false;
    float localMultiplier = 1.0f;

    for (int internalAttempt = 0; internalAttempt <= maxInternalRetries && !anyHit; ++internalAttempt) {
        float DETECTION_RANGE = std::min(baseRange * progressiveMultiplier * localMultiplier, maxRange);

        raycastData.raySegments.clear();
        raycastData.hitBodies.clear();

        for (int i = 0; i < numRays; ++i) {
            float angle = -maxAngle + (2.0f * maxAngle * i) / (numRays - 1);
            b2Vec2 dir = { std::cos(angle), std::sin(angle) };
            b2Vec2 end = { origin.x + dir.x * DETECTION_RANGE, origin.y + dir.y * DETECTION_RANGE };

            raycastData.raySegments.push_back({ origin, end });
            b2World_CastRay(worldId, origin, end, b2DefaultQueryFilter(), RaycastCallback, &raycastData);
        }

        if (!raycastData.hitBodies.empty()) {
            anyHit = true;
            break;
        } else {
            localMultiplier *= internalGrowth;
        }
    }

    if (!anyHit) {
        return;
    }

    const int maxReinjectPerStep = 10;
    int reinjected = 0;

    std::uniform_real_distribution<> jitterDistribution(-0.05f, 0.05f);

    for (b2BodyId body : raycastData.hitBodies) {
        if (reinjected >= maxReinjectPerStep) break;

        b2Vec2 pos = b2Body_GetPosition(body);
        float jitter = jitterDistribution(randomEngine);
        float randomY = REINJECT_HEIGHT + ((float)rand() / RAND_MAX - 0.5f) * REINJECT_HEIGHT_VARIATION;
        b2Vec2 newPos = { pos.x + jitter, randomY };

        b2Body_SetTransform(body, newPos, b2Body_GetRotation(body));
        b2Body_SetLinearVelocity(body, {0.0f, 0.0f});
        b2Body_SetAngularVelocity(body, 0.0f);
        b2Body_SetAwake(body, true);

        ++reinjected;
    }

    std::cout << "Reinyectadas " << reinjected << " partículas del arco "
              << "(Intento global #" << blockageRetryCount << ", Rango: "
              << std::fixed << std::setprecision(2)
              << std::min(baseRange * progressiveMultiplier * localMultiplier, maxRange) << " m)\n";
}
