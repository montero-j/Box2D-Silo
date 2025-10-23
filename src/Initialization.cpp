// src/Initialization.cpp
#include "Initialization.h"
#include "Constants.h"
#include <iostream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <cstring>
#include <random>
#include <set>
#include <sstream>
#include <limits>
#include <iomanip>

// =========================================================
// DEFINICIÓN E INICIALIZACIÓN DE VARIABLES GLOBALES DE ESTE MÓDULO
// =========================================================
b2WorldId worldId = b2_nullWorldId;
std::vector<ParticleInfo> particles;
std::vector<b2BodyId> particleBodyIds;

// =========================================================
// IMPLEMENTACIÓN DE LAS FUNCIONES DEL MÓDULO
// =========================================================

bool parseAndValidateArgs(int argc, char** argv) {
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--size-ratio") == 0 && i + 1 < argc) {
            SIZE_RATIO = std::stof(argv[++i]);
        }
        else if (strcmp(argv[i], "--chi") == 0 && i + 1 < argc) {
            CHI = std::stof(argv[++i]);
        }
        else if (strcmp(argv[i], "--base-radius") == 0 && i + 1 < argc) {
            BASE_RADIUS = std::stof(argv[++i]);
        }
        else if (strcmp(argv[i], "--outlet-width") == 0 && i + 1 < argc) {
            OUTLET_WIDTH = std::stof(argv[++i]);
        }
        else if (strcmp(argv[i], "--silo-width") == 0 && i + 1 < argc) {
            SILO_WIDTH = std::stof(argv[++i]);
        }
        else if (strcmp(argv[i], "--silo-height") == 0 && i + 1 < argc) {
            silo_height = std::stof(argv[++i]);
        }
        else if (strcmp(argv[i], "--total-particles") == 0 && i + 1 < argc) {
            TOTAL_PARTICLES = std::stoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--num-large-circles") == 0 && i + 1 < argc) {
            NUM_LARGE_CIRCLES = std::stoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--num-small-circles") == 0 && i + 1 < argc) {
            NUM_SMALL_CIRCLES = std::stoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--num-polygon-particles") == 0 && i + 1 < argc) {
            NUM_POLYGON_PARTICLES = std::stoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--num-sides") == 0 && i + 1 < argc) {
            NUM_SIDES = std::stoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--polygon-perimeter") == 0 && i + 1 < argc) {
            POLYGON_PERIMETER = std::stof(argv[++i]);
        }
        else if (strcmp(argv[i], "--current-sim") == 0 && i + 1 < argc) {
            CURRENT_SIMULATION = std::stoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--total-sims") == 0 && i + 1 < argc) {
            TOTAL_SIMULATIONS = std::stoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--save-sim-data") == 0 && i + 1 < argc) {
            SAVE_SIMULATION_DATA = (std::stoi(argv[++i]) == 1);
        }
        else if (strcmp(argv[i], "--reinject-height-ratio") == 0 && i + 1 < argc) {
            REINJECT_HEIGHT_RATIO = std::stof(argv[++i]);
        }
        else if (strcmp(argv[i], "--reinject-height-variation") == 0 && i + 1 < argc) {
            REINJECT_HEIGHT_VARIATION = std::stof(argv[++i]);
        }
        else if (strcmp(argv[i], "--reinject-width-ratio") == 0 && i + 1 < argc) {
            REINJECT_WIDTH_RATIO = std::stof(argv[++i]);
        }
        else if (strcmp(argv[i], "--max-avalanches") == 0 && i + 1 < argc) {
            MAX_AVALANCHES = std::stoi(argv[++i]);
        }
    }

    if (REINJECT_HEIGHT_RATIO < 0.1f || REINJECT_HEIGHT_RATIO > 12.0f) {
        std::cerr << "Advertencia: REINJECT_HEIGHT_RATIO fuera del rango recomendado. Ajustando a 0.51.\n";
        REINJECT_HEIGHT_RATIO = 0.51f;
    }

    if (REINJECT_HEIGHT_VARIATION < 0.0f || REINJECT_HEIGHT_VARIATION > 0.2f) {
        std::cerr << "Advertencia: REINJECT_HEIGHT_VARIATION fuera del rango recomendado. Ajustando a 0.043.\n";
        REINJECT_HEIGHT_VARIATION = 0.043f;
    }

    if (REINJECT_WIDTH_RATIO < 0.1f || REINJECT_WIDTH_RATIO > 0.8f) {
        std::cerr << "Advertencia: REINJECT_WIDTH_RATIO fuera del rango recomendado. Ajustando a 0.31.\n";
        REINJECT_WIDTH_RATIO = 0.31f;
    }

    if (silo_height <= 0 || SILO_WIDTH <= 0 || OUTLET_WIDTH <= 0) {
        std::cerr << "Error: Dimensiones del silo deben ser positivas.\n";
        return false;
    }

    return true;
}

bool calculateDerivedParameters() {

    OUTLET_X_HALF_WIDTH = OUTLET_WIDTH / 2.0f;

    if (TOTAL_PARTICLES > 0) {
        
        bool usePolygonsAsLargeComponent = (NUM_SIDES > 0 || NUM_POLYGON_PARTICLES > 0);
        int N_ref = TOTAL_PARTICLES;

        NUM_LARGE_CIRCLES = 0;
        NUM_SMALL_CIRCLES = 0;
        NUM_POLYGON_PARTICLES = 0;

        if (SIZE_RATIO < 1e-3) {
            
            if (usePolygonsAsLargeComponent) {
                NUM_POLYGON_PARTICLES = N_ref;
            } else {
                NUM_LARGE_CIRCLES = N_ref;
            }
            NUM_SMALL_CIRCLES = 0;
        }
        else {
            // K = Razón de Masa = (M_large / M_small) = 1 / (SIZE_RATIO)^2
            float K_mass_ratio = 1.0f / (SIZE_RATIO * SIZE_RATIO);

            // 1. Calcular N_L (Número de partículas grandes)
            float N_L_float = CHI * (float)N_ref;
            // Redondear al entero más cercano
            int N_L = static_cast<int>(std::round(N_L_float));
            // Asegurar que N_L está dentro de los límites [0, N_ref]
            N_L = std::max(0, std::min(N_ref, N_L));

            // 2. Calcular N_S (Número de partículas pequeñas)
            // N_S = K * (N_ref - N_L)
            int N_L_final = N_L; 
            float N_S_float = K_mass_ratio * (float)(N_ref - N_L_final);
            int N_S = static_cast<int>(std::round(N_S_float));

            // 3. Asignar y actualizar el número total de partículas
            TOTAL_PARTICLES = N_L + N_S;

            if (usePolygonsAsLargeComponent) {
                NUM_POLYGON_PARTICLES = N_L;
                NUM_LARGE_CIRCLES = 0;
                NUM_SMALL_CIRCLES = N_S;
            } else {
                NUM_LARGE_CIRCLES = N_L;
                NUM_POLYGON_PARTICLES = 0;
                NUM_SMALL_CIRCLES = N_S;
            }
        }
    } else {
        
        TOTAL_PARTICLES = NUM_LARGE_CIRCLES + NUM_SMALL_CIRCLES + NUM_POLYGON_PARTICLES;
    }

    
    if (NUM_POLYGON_PARTICLES > 0 && POLYGON_PERIMETER == 0.0f) {
        if (NUM_SIDES < 3) {
            std::cerr << "Error: Un polígono debe tener al menos 3 lados (NUM_SIDES).\n";
            return false;
        }

        // Área deseada = Área del Círculo Grande (Radio = BASE_RADIUS)
        const float desired_area = M_PI * BASE_RADIUS * BASE_RADIUS;



        float tan_pi_over_N = tanf(M_PI / (float)NUM_SIDES);

        // Se calcula el cuadrado del lado (side length squared)
        float s_squared = (4.0f * desired_area * tan_pi_over_N) / (float)NUM_SIDES;

        // s = sqrt(s_squared)
        float side_length = std::sqrt(s_squared);

        // Perímetro = N * s
        POLYGON_PERIMETER = (float)NUM_SIDES * side_length;

        std::cout << "Calculando Perímetro del Polígono (" << NUM_SIDES << " lados) para igualar el Área del Círculo (R=" << BASE_RADIUS << "): " << POLYGON_PERIMETER << "\n";
    }
    // =========================================================================


    if (CURRENT_SIMULATION > 10) {
        SAVE_SIMULATION_DATA = false;
    }

    return true;
}

b2WorldId createWorldAndWalls(b2BodyId& outletBlockIdRef) {

    // Configurar mundo Box2D
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = (b2Vec2){0.0f, -9.81f};
    b2WorldId newWorldId = b2CreateWorld(&worldDef);

    worldId = newWorldId;

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.filter.categoryBits = 0x0001;
    shapeDef.filter.maskBits = 0xFFFF;
    shapeDef.material.friction = 0.5f;
    shapeDef.material.restitution = 0.9f;

    // Crear estructuras del silo
    const float wall_thickness = WALL_THICKNESS;
    const float ground_level_y = GROUND_LEVEL_Y;

    // Pared izquierda
    b2BodyDef leftWallDef = b2DefaultBodyDef();
    leftWallDef.position = (b2Vec2){-(SILO_WIDTH / 2.0f) - (wall_thickness / 2.0f), ground_level_y + (silo_height / 2.0f)};
    b2BodyId leftWallId = b2CreateBody(worldId, &leftWallDef);
    b2Body_SetType(leftWallId, b2_staticBody);
    b2Polygon leftWallShape = b2MakeBox(wall_thickness / 2.0f, silo_height / 2.0f);
    b2CreatePolygonShape(leftWallId, &shapeDef, &leftWallShape);

    // Pared derecha
    b2BodyDef rightWallDef = b2DefaultBodyDef();
    rightWallDef.position = (b2Vec2){(SILO_WIDTH / 2.0f) + (wall_thickness / 2.0f), ground_level_y + (silo_height / 2.0f)};
    b2BodyId rightWallId = b2CreateBody(worldId, &rightWallDef);
    b2Body_SetType(rightWallId, b2_staticBody);
    b2Polygon rightWallShape = b2MakeBox(wall_thickness / 2.0f, silo_height / 2.0f);
    b2CreatePolygonShape(rightWallId, &shapeDef, &rightWallShape);

    // Fondo izquierdo
    b2BodyDef groundLeftDef = b2DefaultBodyDef();
    groundLeftDef.position = (b2Vec2){(-SILO_WIDTH / 2.0f + -OUTLET_X_HALF_WIDTH) / 2.0f, ground_level_y - (wall_thickness / 2.0f)};
    b2BodyId groundLeftId = b2CreateBody(worldId, &groundLeftDef);
    b2Body_SetType(groundLeftId, b2_staticBody);
    b2Polygon groundLeftShape = b2MakeBox((SILO_WIDTH / 2.0f - OUTLET_X_HALF_WIDTH) / 2.0f, wall_thickness / 2.0f);
    b2CreatePolygonShape(groundLeftId, &shapeDef, &groundLeftShape);

    // Fondo derecho
    b2BodyDef groundRightDef = b2DefaultBodyDef();
    groundRightDef.position = (b2Vec2){(OUTLET_X_HALF_WIDTH + SILO_WIDTH / 2.0f) / 2.0f, ground_level_y - (wall_thickness / 2.0f)};
    b2BodyId groundRightId = b2CreateBody(worldId, &groundRightDef);
    b2Body_SetType(groundRightId, b2_staticBody);
    b2Polygon groundRightShape = b2MakeBox((SILO_WIDTH / 2.0f - OUTLET_X_HALF_WIDTH) / 2.0f, wall_thickness / 2.0f);
    b2CreatePolygonShape(groundRightId, &shapeDef, &groundRightShape);

    // OUTLET TEMPORAL BLOQUEADO
    b2BodyDef outletBlockDef = b2DefaultBodyDef();
    outletBlockDef.position = (b2Vec2){0.0f, ground_level_y - (wall_thickness / 2.0f)};
    b2BodyId tempOutletBlockId = b2CreateBody(worldId, &outletBlockDef);
    b2Body_SetType(tempOutletBlockId, b2_staticBody);
    b2Polygon outletBlockShape = b2MakeBox(OUTLET_X_HALF_WIDTH, wall_thickness / 2.0f);
    b2CreatePolygonShape(tempOutletBlockId, &shapeDef, &outletBlockShape);

    outletBlockIdRef = tempOutletBlockId;

    return newWorldId;
}

void createParticles(b2WorldId worldId) {

    particles.clear();
    particleBodyIds.clear();

    const float largeCircleRadius = BASE_RADIUS;
    const float smallCircleRadius = BASE_RADIUS * SIZE_RATIO;

    // Definir tipos de partículas a crear
    std::vector<ParticleShapeType> particleTypesToCreate;
    for (int i = 0; i < NUM_LARGE_CIRCLES; ++i) particleTypesToCreate.push_back(CIRCLE);
    for (int i = 0; i < NUM_SMALL_CIRCLES; ++i) particleTypesToCreate.push_back(CIRCLE);
    for (int i = 0; i < NUM_POLYGON_PARTICLES; ++i) particleTypesToCreate.push_back(POLYGON);
    std::shuffle(particleTypesToCreate.begin(), particleTypesToCreate.end(), randomEngine);

    const float minX_gen = -SILO_WIDTH / 2.0f + BASE_RADIUS + 0.01f;
    const float maxX_gen = SILO_WIDTH / 2.0f - BASE_RADIUS - 0.01f;
    const float minY_gen = BASE_RADIUS + 0.01f;
    const float maxY_gen = silo_height - BASE_RADIUS - 0.01f;

    // Generar posiciones aleatorias
    std::vector<std::pair<float, float>> particlePositions;
    particlePositions.reserve(TOTAL_PARTICLES);

    // Calcular radio máximo para evitar superposiciones
    float maxParticleRadius = largeCircleRadius;
    if (NUM_POLYGON_PARTICLES > 0) {
        float polyCircumRadius = POLYGON_PERIMETER / (2.0f * NUM_SIDES * sin(M_PI / NUM_SIDES));
        maxParticleRadius = std::max(maxParticleRadius, polyCircumRadius);
    }

    for (int i = 0; i < TOTAL_PARTICLES; ++i) {
        bool positionValid = false;
        int attempts = 0;
        const int MAX_ATTEMPTS = 1000;

        while (!positionValid && attempts < MAX_ATTEMPTS) {
            float x = minX_gen + (maxX_gen - minX_gen) * static_cast<float>(rand()) / RAND_MAX;
            float y = minY_gen + (maxY_gen - minY_gen) * static_cast<float>(rand()) / RAND_MAX;

            // Verificar superposición
            bool overlaps = false;
            for (const auto& existingPos : particlePositions) {
                float dx = x - existingPos.first;
                float dy = y - existingPos.second;
                float minDistance = 2.0f * maxParticleRadius + 0.01f;
                if (dx * dx + dy * dy < minDistance * minDistance) {
                    overlaps = true;
                    break;
                }
            }

            if (!overlaps) {
                particlePositions.push_back({x, y});
                positionValid = true;
            }
            attempts++;
        }

        if (!positionValid) {
            float x = minX_gen + (maxX_gen - minX_gen) * static_cast<float>(rand()) / RAND_MAX;
            float y = minY_gen + (maxY_gen - minY_gen) * static_cast<float>(rand()) / RAND_MAX;
            particlePositions.push_back({x, y});
        }
    }

    // Crear partículas con orientaciones aleatorias
    for (int i = 0; i < TOTAL_PARTICLES; ++i) {
        float particleX = particlePositions[i].first;
        float particleY = particlePositions[i].second;

        float randomAngle = angleDistribution(randomEngine);
        b2Rot randomRotation = {cosf(randomAngle / 2.0f), sinf(randomAngle / 2.0f)};

        b2BodyDef particleDef = b2DefaultBodyDef();
        particleDef.type = b2_dynamicBody;
        particleDef.position = (b2Vec2){particleX, particleY};
        particleDef.rotation = randomRotation;
        particleDef.isBullet = false;
        b2BodyId particleId = b2CreateBody(worldId, &particleDef);

        b2ShapeDef particleShapeDef = b2DefaultShapeDef();
        particleShapeDef.density = Density;
        particleShapeDef.material.friction = 0.5f;
        particleShapeDef.material.restitution = 0.9f;

        ParticleShapeType currentParticleType = particleTypesToCreate[i];
        float currentParticleSize = 0.0f;
        int currentNumSides = 0;

        if (currentParticleType == CIRCLE) {
            bool isLargeCircle = (i < NUM_LARGE_CIRCLES);
            currentParticleSize = isLargeCircle ? largeCircleRadius : smallCircleRadius;

            b2Circle circle = {};
            circle.radius = currentParticleSize;
            b2CreateCircleShape(particleId, &particleShapeDef, &circle);

            b2MassData massData = b2Body_GetMassData(particleId);
            particles.push_back({particleId, CIRCLE, currentParticleSize, massData.mass, isLargeCircle, 0});
        } else {
            currentNumSides = NUM_SIDES;
            if (currentNumSides < 3) currentNumSides = 3;

            float polyCircumRadius = POLYGON_PERIMETER / (2.0f * currentNumSides * sin(M_PI / currentNumSides));
            currentParticleSize = polyCircumRadius;
            const float POLYGON_SKIN_RADIUS = 0.0f;

            b2Vec2 vertices[BOX2D_MAX_POLYGON_VERTICES];
            int actualNumSides = std::min(currentNumSides, BOX2D_MAX_POLYGON_VERTICES);

            for (int j = 0; j < actualNumSides; ++j) {
                float angle = 2.0f * M_PI * j / actualNumSides;
                vertices[j] = (b2Vec2){polyCircumRadius * cos(angle), polyCircumRadius * sin(angle)};
            }

            b2Hull hull = b2ComputeHull(vertices, actualNumSides);
            b2Polygon polygonShape = b2MakePolygon(&hull, POLYGON_SKIN_RADIUS);
            b2CreatePolygonShape(particleId, &particleShapeDef, &polygonShape);

            b2MassData massData = b2Body_GetMassData(particleId);
            particles.push_back({particleId, POLYGON, currentParticleSize, massData.mass, true, actualNumSides});
        }
        particleBodyIds.push_back(particleId);
    }
    std::cout << "Generación completada: " << TOTAL_PARTICLES << " partículas con distribución y orientación aleatorias\n\n";
}

void runSedimentation(b2WorldId worldId) {

    std::cout << "Dejando que " << TOTAL_PARTICLES << " partículas se sedimenten por gravedad\n";

    float sedimentationTime = 0.0f;
    const float MAX_SEDIMENTATION_TIME = 5.0f;
    const float STABILITY_CHECK_INTERVAL = 1.0f;
    float lastStabilityCheck = 0.0f;

    float totalKineticEnergy = 0.0f;
    float previousKineticEnergy = 1000.0f;
    int stabilityCounter = 0;
    const int REQUIRED_STABILITY_CHECKS = 3;
    bool sedimentationComplete = false;


    while (sedimentationTime < MAX_SEDIMENTATION_TIME && !sedimentationComplete) {
        b2World_Step(worldId, TIME_STEP, SUB_STEP_COUNT);
        sedimentationTime += TIME_STEP;


        if (sedimentationTime - lastStabilityCheck >= STABILITY_CHECK_INTERVAL) {
            totalKineticEnergy = 0.0f;
            for (const auto& particle : particles) {
                b2Vec2 velocity = b2Body_GetLinearVelocity(particle.bodyId);
                float speed = sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
                totalKineticEnergy += 0.5f * particle.mass * speed * speed;
            }

            float energyChange = std::abs(totalKineticEnergy - previousKineticEnergy);
            const float STABILITY_THRESHOLD = 0.1f;

            if (energyChange < STABILITY_THRESHOLD) {
                stabilityCounter++;
            } else {
                stabilityCounter = 0;
            }

            if (stabilityCounter >= REQUIRED_STABILITY_CHECKS) {
                sedimentationComplete = true;
                std::cout << "Estabilización completa en " << sedimentationTime << " segundos\n";
            }

            previousKineticEnergy = totalKineticEnergy;
            lastStabilityCheck = sedimentationTime;
        }
    }

    if (!sedimentationComplete) {
        std::cout << "Estabilización finalizada por timeout después de " << MAX_SEDIMENTATION_TIME << " segundos\n";
    }
}
