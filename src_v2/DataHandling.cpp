// src/DataHandling.cpp

#include "DataHandling.h"
#include "Constants.h"
#include "Initialization.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <cstdlib>
#include <string>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <random>
#include <cstring>
#include <set>

// =========================================================
// IMPLEMENTACIÓN DE FUNCIÓN AUXILIAR (Callback para raycast)
// =========================================================

float RaycastCallback(b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context) {
    b2BodyId bodyId = b2Shape_GetBody(shapeId);

    if (b2Body_GetType(bodyId) == b2_dynamicBody) {
        RaycastUserData* data = static_cast<RaycastUserData*>(context);
        data->hitBodies.insert(bodyId);

        if (!data->raySegments.empty()) {

        }
    }
    return fraction;
}

// =========================================================
// IMPLEMENTACIÓN DE FUNCIONES DE INICIALIZACIÓN/FINALIZACIÓN DE DATOS
// =========================================================

void initializeDataFiles() {

    // Crear directorio de resultados
    std::ostringstream dirNameStream;
    dirNameStream << "sim_" << CURRENT_SIMULATION
                  << "part_" << TOTAL_PARTICLES
                  << "_chi" << std::fixed << std::setprecision(2) << CHI
                  << "_ratio" << std::setprecision(2) << SIZE_RATIO
                  << "_br" << std::setprecision(3) << BASE_RADIUS
                  << "_lg" << NUM_LARGE_CIRCLES
                  << "_sm" << NUM_SMALL_CIRCLES
                  << "_poly" << NUM_POLYGON_PARTICLES
                  << "_sides" << NUM_SIDES
                  << "_outlet" << std::setprecision(2) << OUTLET_WIDTH
                  << "_maxAva" << MAX_AVALANCHES;

    std::string outputDir = "./simulations/" + dirNameStream.str() + "/";
    std::filesystem::create_directories(outputDir);

    // Abrir archivos de salida
    if (SAVE_SIMULATION_DATA) {
        simulationDataFile.open(outputDir + "simulation_data.csv");
        simulationDataFile << "Time";
        for (int i = 0; i < TOTAL_PARTICLES; ++i) {
            simulationDataFile << ",p" << i << "_x,p" << i << "_y,p" << i << "_type,p" << i << "_size,p" << i << "_sides,p" << i << "_angle";
        }
        simulationDataFile << ",rays_begin";
        for (int i = 0; i < 120; ++i) {
            simulationDataFile << ",ray" << i << "_x1,ray" << i << "_y1,ray" << i << "_x2,ray" << i << "_y2";
        }
        simulationDataFile << ",rays_end\n";
    }
    avalancheDataFile.open(outputDir + "avalanche_data.csv");
    flowDataFile.open(outputDir + "flow_data.csv");

    // Encabezado para flow_data.csv
    flowDataFile << "Time,MassTotal,MassFlowRate,NoPTotal,NoPFlowRate,MassOriginalTotal,MassOriginalFlowRate,NoPOriginalTotal,NoPOriginalFlowRate\n";
}

void finalizeDataFiles(bool simulationInterrupted) {

    if (inAvalanche && !simulationInterrupted) {
        finalizeAvalanche();
    }
    if (inBlockage && !simulationInterrupted) {
        totalBlockageTime += (simulationTime - blockageStartTime);
    }

    float totalSimulationTime = simulationTime;

    if (accumulatedMass > 0) {
        recordFlowData(simulationTime, 0, 0, 0, 0);
    }

    avalancheDataFile << "\n===== RESUMEN FINAL =====\n";
    avalancheDataFile << "# Tiempo total de simulación: " << totalSimulationTime << " s\n";
    avalancheDataFile << "# Tiempo total en avalanchas: " << totalFlowingTime << " s\n";
    avalancheDataFile << "# Tiempo total en atascos: " << totalBlockageTime << " s\n";
    avalancheDataFile << "# Reintentos de bloqueo realizados: " << blockageRetryCount << "\n";
    avalancheDataFile << "# Simulación interrumpida: " << (simulationInterrupted ? "Sí" : "No") << "\n";
    avalancheDataFile << "# Máximo de avalanchas alcanzado: " << (avalancheCount >= MAX_AVALANCHES ? "Sí" : "No") << "\n";


    if (SAVE_SIMULATION_DATA) simulationDataFile.close();
    avalancheDataFile.close();
    flowDataFile.close();

    std::cout << "\n===== SIMULACIÓN COMPLETADA =====\n";
    std::cout << "Avalanchas registradas: " << avalancheCount << "/" << MAX_AVALANCHES << "\n";
    std::cout << "Tiempo total: " << totalSimulationTime << "s | Flujo: " << totalFlowingTime << "s | Atasco: " << totalBlockageTime << "s\n";
    std::cout << "Partículas salientes: " << totalExitedParticles << "\n";
}

// =========================================================
// IMPLEMENTACIÓN DE FUNCIONES DE FÍSICA Y FLUJO
// =========================================================

void applyRandomImpulses() {
    if (simulationTime - lastShockTime >= SHOCK_INTERVAL) {
        for (const auto& particle : particles) {
            float magnitude = impulseMagnitudeDistribution(randomEngine) * 0.5f;
            float angle = angleDistribution(randomEngine);
            b2Vec2 impulse = { magnitude * cos(angle), magnitude * sin(angle) };
            b2Body_ApplyLinearImpulseToCenter(particle.bodyId, impulse, true);
        }
        lastShockTime = simulationTime;
    }
}

void manageParticles(b2WorldId worldId, float currentTime, float siloHeight,
                     int& exitedTotalCount, float& exitedTotalMass,
                     int& exitedOriginalCount, float& exitedOriginalMass)
{
    const float EXIT_BELOW_Y = -1.5f;
    const float OUTLET_LEFT_X  = -OUTLET_X_HALF_WIDTH;
    const float OUTLET_RIGHT_X =  OUTLET_X_HALF_WIDTH;

    const float REINJECT_HALF_WIDTH = SILO_WIDTH * REINJECT_WIDTH_RATIO * 0.5f;
    const float REINJECT_MIN_X = -REINJECT_HALF_WIDTH;
    const float REINJECT_MAX_X =  REINJECT_HALF_WIDTH;
    const float REINJECT_MIN_Y = siloHeight * REINJECT_HEIGHT_RATIO;
    const float REINJECT_MAX_Y = siloHeight * (REINJECT_HEIGHT_RATIO + REINJECT_HEIGHT_VARIATION);

    exitedTotalCount = 0;
    exitedTotalMass = 0.0f;
    exitedOriginalCount = 0;
    exitedOriginalMass = 0.0f;

    for (size_t i = 0; i < particleBodyIds.size(); ++i) {
        b2BodyId particleId = particleBodyIds[i];
        b2Vec2 pos = b2Body_GetPosition(particleId);

        if (pos.y < EXIT_BELOW_Y && pos.x >= OUTLET_LEFT_X && pos.x <= OUTLET_RIGHT_X) {
            exitedTotalCount++;
            exitedTotalMass += particles[i].mass;
            lastParticleExitTime = currentTime;

            if (particles[i].isOriginal) {
                exitedOriginalCount++;
                exitedOriginalMass += particles[i].mass;
            }

            float randomX = REINJECT_MIN_X + (REINJECT_MAX_X - REINJECT_MIN_X) * static_cast<float>(rand()) / RAND_MAX;
            float randomY = REINJECT_MIN_Y + (REINJECT_MAX_Y - REINJECT_MIN_Y) * static_cast<float>(rand()) / RAND_MAX;

            b2Body_SetTransform(particleId, (b2Vec2){randomX, randomY}, (b2Rot){0.0f, 1.0f});
            b2Body_SetLinearVelocity(particleId, (b2Vec2){0.0f, 0.0f});
            b2Body_SetAngularVelocity(particleId, 0.0f);
            b2Body_SetAwake(particleId, true);
        }

        else if (pos.y < EXIT_BELOW_Y || pos.x < -SILO_WIDTH || pos.x > SILO_WIDTH) {
            float randomX = REINJECT_MIN_X + (REINJECT_MAX_X - REINJECT_MIN_X) * static_cast<float>(rand()) / RAND_MAX;
            float randomY = REINJECT_MIN_Y + (REINJECT_MAX_Y - REINJECT_MIN_Y) * static_cast<float>(rand()) / RAND_MAX;

            b2Body_SetTransform(particleId, (b2Vec2){randomX, randomY}, (b2Rot){0.0f, 1.0f});
            b2Body_SetLinearVelocity(particleId, (b2Vec2){0.0f, 0.0f});
            b2Body_SetAngularVelocity(particleId, 0.0f);
            b2Body_SetAwake(particleId, true);
        }
    }
}

void recordFlowData(float currentTime, int exitedTotalCount, float exitedTotalMass,
                    int exitedOriginalCount, float exitedOriginalMass) {

    accumulatedMass += exitedTotalMass;
    accumulatedParticles += exitedTotalCount;
    accumulatedOriginalMass += exitedOriginalMass;
    accumulatedOriginalParticles += exitedOriginalCount;

    if (currentTime - lastRecordedTime >= RECORD_INTERVAL) {
        float timeSinceLast = currentTime - lastRecordedTime;

        float massFlowRate = (timeSinceLast > 0) ? accumulatedMass / timeSinceLast : 0.0f;
        float particleFlowRate = (timeSinceLast > 0) ? accumulatedParticles / timeSinceLast : 0.0f;
        float originalMassFlowRate = (timeSinceLast > 0) ? accumulatedOriginalMass / timeSinceLast : 0.0f;
        float originalParticleFlowRate = (timeSinceLast > 0) ? accumulatedOriginalParticles / timeSinceLast : 0.0f;

        totalExitedMass += accumulatedMass;
        totalExitedParticles += accumulatedParticles;
        totalExitedOriginalMass += accumulatedOriginalMass;
        totalExitedOriginalParticles += accumulatedOriginalParticles;

        flowDataFile << std::fixed << std::setprecision(5) << currentTime << ","
                     << totalExitedMass << "," << massFlowRate << ","
                     << totalExitedParticles << "," << particleFlowRate << ","
                     << totalExitedOriginalMass << "," << originalMassFlowRate << ","
                     << totalExitedOriginalParticles << "," << originalParticleFlowRate << "\n";

        accumulatedMass = 0.0f;
        accumulatedParticles = 0;
        accumulatedOriginalMass = 0.0f;
        accumulatedOriginalParticles = 0;
        lastRecordedTime = currentTime;
    }
}

void detectAndReinjectArchViaRaycast(b2WorldId worldId, float siloHeight) {

    const float REINJECT_HEIGHT = siloHeight * REINJECT_HEIGHT_RATIO;
    float baseRange = OUTLET_WIDTH * 2.0f;
    float progressiveMultiplier = 1.0f + (blockageRetryCount * 0.5f);
    float maxRange = std::min(siloHeight * 0.05f, siloHeight * 0.5f);

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
        std::cout << "detectAndReinjectArchViaRaycast: no se detectaron partículas tras reintentos internos.\n";
        return;
    }

    const int maxReinjectPerStep = 10;
    int reinjected = 0;

    for (b2BodyId body : raycastData.hitBodies) {
        if (reinjected >= maxReinjectPerStep) break;

        b2Vec2 pos = b2Body_GetPosition(body);
        float jitter = ((float)rand() / RAND_MAX - 0.5f) * 0.05f;
        b2Vec2 newPos = { pos.x + jitter, REINJECT_HEIGHT + ((float)rand() / RAND_MAX - 0.5f) * REINJECT_HEIGHT_VARIATION };

        b2Body_SetTransform(body, newPos, b2Body_GetRotation(body));
        b2Body_SetLinearVelocity(body, {0.0f, 0.0f});
        b2Body_SetAngularVelocity(body, 0.0f);
        b2Body_SetAwake(body, true);

        ++reinjected;
    }

    std::cout << "Reinyectadas " << reinjected << " partículas del arco "
              << "(Intento global #" << blockageRetryCount << ", Rango usado: "
              << std::fixed << std::setprecision(2)
              << std::min(baseRange * progressiveMultiplier * localMultiplier, maxRange) << " m)\n";
}

void finalizeAvalanche() {

    float currentAvalancheDuration = simulationTime - avalancheStartTime;

    if (currentAvalancheDuration >= MIN_AVALANCHE_DURATION) {
        totalFlowingTime += currentAvalancheDuration;
        int particlesInThisAvalanche = totalExitedParticles - avalancheStartParticleCount;

        avalancheDataFile << "Avalancha " << (avalancheCount + 1) << ","
                        << avalancheStartTime << ","
                        << simulationTime << ","
                        << currentAvalancheDuration << ","
                        << particlesInThisAvalanche << "\n";

        avalancheCount++;
        std::cout << "Avalancha " << avalancheCount << " registrada: "
                  << currentAvalancheDuration << "s, "
                  << particlesInThisAvalanche << " partículas\n";
    }

    particlesExitedInCurrentAvalanche.clear();
    inAvalanche = false;
}

void startAvalanche() {
    inAvalanche = true;
    avalancheStartTime = simulationTime;
    avalancheStartParticleCount = totalExitedParticles;
    particlesExitedInCurrentAvalanche.clear();
    std::cout << "Inicio de avalancha " << (avalancheCount + 1) << " a t=" << simulationTime << "s\n";
}

void startBlockage() {
    finalizeAvalanche();
    inBlockage = true;
    blockageStartTime = simulationTime;
    blockageRetryCount = 0;
    std::cout << "Atasco detectado a t=" << simulationTime << "s\n";
}

void checkFlowStatus(b2WorldId worldId, float timeSinceLastExit) {

    if (!inAvalanche && !inBlockage) {
        // Estado inicial: esperando primer flujo
        if (totalExitedParticles > lastTotalExitedCount) {
            startAvalanche();
        }
    }
    else if (inAvalanche) {
        // Durante avalancha: verificar si se atasca
        if (timeSinceLastExit > BLOCKAGE_THRESHOLD) {
            startBlockage();
        }
    }
    else if (inBlockage) {
        // Durante atasco: verificar si fluye o si necesita raycast
        if (totalExitedParticles > lastTotalExitedCount) {
            // Flujo reanudado naturalmente
            float blockageDuration = simulationTime - blockageStartTime;
            totalBlockageTime += blockageDuration;
            inBlockage = false;
            startAvalanche();
            std::cout << "Flujo reanudado después de atasco de " << blockageDuration << "s\n";
        }
        else if (simulationTime - blockageStartTime > 2.0f) {
            if (simulationTime - lastRaycastTime >= RAYCAST_COOLDOWN) {
                detectAndReinjectArchViaRaycast(worldId, silo_height);
                lastRaycastTime = simulationTime;
                blockageRetryCount++;
            }
        }
    }

    lastTotalExitedCount = totalExitedParticles;
}