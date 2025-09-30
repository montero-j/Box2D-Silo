#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <string>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <random>
#include <cstring>
#include <set>
#include <cstdint>
#include <tuple>

#include "box2d/box2d.h"

// Parámetros ajustables (configurables por línea de comandos)
float BASE_RADIUS = 0.5f;
float SIZE_RATIO = 0.0f;
float CHI = 0.0f;
int TOTAL_PARTICLES = 2000;
float OUTLET_WIDTH = 3.9f*2*BASE_RADIUS;
float SILO_WIDTH = 20.2f*2*BASE_RADIUS;
float silo_height = 120*2*BASE_RADIUS;

// Constantes de simulación
const float TIME_STEP = 0.005f;
const int SUB_STEP_COUNT = 20;
const float BLOCKAGE_THRESHOLD = 5.0f;
const float RECORD_INTERVAL = 0.01f;
const float MIN_AVALANCHE_DURATION = 0.5f;
const float RAYCAST_COOLDOWN = 0.5f;
const float SHOCK_INTERVAL = 0.1f;

// Parámetros de reinyección configurables
float REINJECT_HEIGHT_RATIO = 1.0f;
float REINJECT_HEIGHT_VARIATION = 0.043f;
float REINJECT_WIDTH_RATIO = 0.31f;

// Variables derivadas
float OUTLET_X_HALF_WIDTH;

// Parámetros de partículas
int NUM_LARGE_CIRCLES = 0;
int NUM_SMALL_CIRCLES = 0;
int NUM_POLYGON_PARTICLES = 0;
int NUM_SIDES = 5;
float POLYGON_PERIMETER = 0.0f;

// Variables de estado
float simulationTime = 0.0f;
float lastPrintTime = 0.0f;
float lastRaycastTime = -RAYCAST_COOLDOWN;
float lastShockTime = 0.0f;
int frameCounter = 0;

// Variables para guardado de datos
std::ofstream simulationDataFile;
std::ofstream avalancheDataFile;
std::ofstream flowDataFile;
bool SAVE_SIMULATION_DATA = false;
int CURRENT_SIMULATION = 1;
int TOTAL_SIMULATIONS = 1;

// Variables para avalanchas/atascos
int avalancheCount = 0;
float totalFlowingTime = 0.0f;
float totalBlockageTime = 0.0f;
bool inAvalanche = false;
bool inBlockage = false;
float blockageStartTime = 0.0f;
float avalancheStartTime = 0.0f;
int particlesInCurrentAvalanche = 0;
int avalancheStartParticleCount = 0;
float lastExitDuringAvalanche = 0.0f;
float lastParticleExitTime = 0.0f;
float previousBlockageDuration = 0.0f;
int blockageRetryCount = 0;
const int MAX_BLOCKAGE_RETRIES = 100;
int MAX_AVALANCHES = 50;  // Nuevo: máximo de avalanchas antes de interrumpir

// Comparador para b2BodyId en std::set
struct BodyIdComparator {
    bool operator()(const b2BodyId& a, const b2BodyId& b) const {
        return a.index1 < b.index1;
    }
};

// Set para rastrear partículas que ya salieron en la avalancha actual
std::set<b2BodyId, BodyIdComparator> particlesExitedInCurrentAvalanche;

// Variables para seguimiento de progreso real del flujo
int lastTotalExitedCount = 0;
float lastProgressTime = 0.0f;
bool waitingForFlowConfirmation = false;

// Variables para registro de flujo
float totalExitedMass = 0.0f;
int totalExitedParticles = 0;
float totalExitedOriginalMass = 0.0f;
int totalExitedOriginalParticles = 0;
float lastRecordedTime = -0.01f;
float accumulatedMass = 0.0f;
int accumulatedParticles = 0;
float accumulatedOriginalMass = 0.0f;
int accumulatedOriginalParticles = 0;

// Variables para impulsos
std::mt19937 randomEngine(time(NULL));
std::uniform_real_distribution<> angleDistribution(0.0f, 2.0f * M_PI);
std::uniform_real_distribution<> impulseMagnitudeDistribution(0.0f, 1.0f);

// Estructuras de datos
enum ParticleShapeType { CIRCLE, POLYGON };

struct ParticleInfo {
    b2BodyId bodyId;
    ParticleShapeType shapeType;
    float size;
    float mass;
    bool isOriginal;
    int numSides;
};

std::vector<ParticleInfo> particles;
std::vector<b2BodyId> particleBodyIds;

// Datos para raycast
struct RaycastUserData {
    std::set<b2BodyId, BodyIdComparator> hitBodies;
    std::vector<std::pair<b2Vec2, b2Vec2>> raySegments;
};

// Callback para raycast
float RaycastCallback(b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context) {
    b2BodyId bodyId = b2Shape_GetBody(shapeId);

    if (b2Body_GetType(bodyId) == b2_dynamicBody) {
        RaycastUserData* data = static_cast<RaycastUserData*>(context);
        data->hitBodies.insert(bodyId);

        if (!data->raySegments.empty()) {
            data->raySegments.back().second = point;
        }
    }
    return fraction;
}

// Función para aplicar impulsos aleatorios
void applyRandomImpulses(std::vector<ParticleInfo>& particles) {
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

// Función para manejar partículas que salen del silo
void manageParticles(b2WorldId worldId, std::vector<b2BodyId>& particleIds,
                     float currentTime, float& lastExitTimeRef,
                     const std::vector<ParticleInfo>& particles,
                     int& exitedTotalCount, float& exitedTotalMass,
                     int& exitedOriginalCount, float& exitedOriginalMass,
                     float siloHeight) {
    const float EXIT_BELOW_Y = -1.5f;
    const float OUTLET_LEFT_X = -OUTLET_X_HALF_WIDTH;
    const float OUTLET_RIGHT_X = OUTLET_X_HALF_WIDTH;
    const float REINJECT_HALF_WIDTH = SILO_WIDTH * REINJECT_WIDTH_RATIO * 0.5f;
    const float REINJECT_MIN_X = -REINJECT_HALF_WIDTH;
    const float REINJECT_MAX_X = REINJECT_HALF_WIDTH;
    const float REINJECT_MIN_Y = siloHeight * REINJECT_HEIGHT_RATIO;
    const float REINJECT_MAX_Y = siloHeight * (REINJECT_HEIGHT_RATIO + REINJECT_HEIGHT_VARIATION);

    exitedTotalCount = 0;
    exitedTotalMass = 0.0f;
    exitedOriginalCount = 0;
    exitedOriginalMass = 0.0f;

    for (size_t i = 0; i < particleIds.size(); ++i) {
        b2BodyId particleId = particleIds[i];
        b2Vec2 pos = b2Body_GetPosition(particleId);

        if (pos.y < EXIT_BELOW_Y && pos.x >= OUTLET_LEFT_X && pos.x <= OUTLET_RIGHT_X) {
            bool alreadyCountedInAvalanche = (particlesExitedInCurrentAvalanche.find(particleId) != particlesExitedInCurrentAvalanche.end());

            if (!alreadyCountedInAvalanche) {
                particlesExitedInCurrentAvalanche.insert(particleId);
                exitedTotalCount++;
                exitedTotalMass += particles[i].mass;
                lastExitTimeRef = currentTime;

                if (particles[i].isOriginal) {
                    exitedOriginalCount++;
                    exitedOriginalMass += particles[i].mass;
                }
            }

            // Reinyección
            float randomX = REINJECT_MIN_X + (REINJECT_MAX_X - REINJECT_MIN_X) * static_cast<float>(rand()) / RAND_MAX;
            float randomY = REINJECT_MIN_Y + (REINJECT_MAX_Y - REINJECT_MIN_Y) * static_cast<float>(rand()) / RAND_MAX;

            b2Body_SetTransform(particleId, (b2Vec2){randomX, randomY}, (b2Rot){0.0f, 1.0f});
            b2Body_SetLinearVelocity(particleId, (b2Vec2){0.0f, 0.0f});
            b2Body_SetAngularVelocity(particleId, 0.0f);
            b2Body_SetAwake(particleId, true);
        }
        // Reinyectar partículas que se escapan por los lados
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

// Función para registrar datos de flujo
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
    float maxRange = std::min(siloHeight * 0.05f, siloHeight * 0.5f);//4.0f);

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

// Función para finalizar una avalancha
void finalizeAvalanche(float currentTime) {
    float currentAvalancheDuration = currentTime - avalancheStartTime;
    
    if (currentAvalancheDuration >= MIN_AVALANCHE_DURATION) {
        totalFlowingTime += currentAvalancheDuration;
        int particlesInThisAvalanche = totalExitedParticles - avalancheStartParticleCount;
        
        avalancheDataFile << "Avalancha " << (avalancheCount + 1) << ","
                        << avalancheStartTime << ","
                        << currentTime << ","
                        << currentAvalancheDuration << ","
                        << particlesInThisAvalanche << "\n";
        
        avalancheCount++;
        std::cout << "Avalancha " << avalancheCount << " registrada: "
                  << currentAvalancheDuration << "s, " 
                  << particlesInThisAvalanche << " partículas\n";
    }
    
    particlesExitedInCurrentAvalanche.clear();
}

int main(int argc, char* argv[]) {
    // Parsear argumentos de línea de comandos
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

    // Calcular variables derivadas
    OUTLET_X_HALF_WIDTH = OUTLET_WIDTH / 2.0f;

    // Validación de parámetros
    if (REINJECT_HEIGHT_RATIO < 0.1f || REINJECT_HEIGHT_RATIO > 1.2f) {
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
        return 1;
    }

    // Lógica para determinar el número de cada tipo de partícula
    bool explicitParticleCounts = (NUM_LARGE_CIRCLES > 0 || NUM_SMALL_CIRCLES > 0 || NUM_POLYGON_PARTICLES > 0);

    if (!explicitParticleCounts) {
        NUM_LARGE_CIRCLES = static_cast<int>((1.0f - CHI) * TOTAL_PARTICLES);
        NUM_SMALL_CIRCLES = static_cast<int>(CHI * TOTAL_PARTICLES);
        NUM_POLYGON_PARTICLES = 0;
    } else {
        TOTAL_PARTICLES = NUM_LARGE_CIRCLES + NUM_SMALL_CIRCLES + NUM_POLYGON_PARTICLES;
    }

    if (NUM_POLYGON_PARTICLES > 0 && POLYGON_PERIMETER == 0.0f) {
        POLYGON_PERIMETER = 2.0f * M_PI * BASE_RADIUS;
        std::cout << "Advertencia: Perímetro de polígono no especificado. Usando valor por defecto: "
                  << std::fixed << std::setprecision(4) << POLYGON_PERIMETER << " m\n";
    }

    if (CURRENT_SIMULATION > 5) {
        SAVE_SIMULATION_DATA = false;
    }

    srand(time(NULL));

    // Calcular radios de partículas
    const float largeCircleRadius = BASE_RADIUS;
    const float smallCircleRadius = BASE_RADIUS * SIZE_RATIO;

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

    // Imprimir parámetros al inicio
    std::cout << "Inicio de simulación granular\n";
    std::cout << "Radio base (r₀): " << BASE_RADIUS << " m\n";
    std::cout << "Razón de tamaño (r): " << SIZE_RATIO << "\n";
    std::cout << "Fracción de partículas pequeñas (χ): " << CHI << "\n";
    std::cout << "Partículas circulares grandes: " << NUM_LARGE_CIRCLES << " (Radio: " << largeCircleRadius << ")\n";
    std::cout << "Partículas circulares pequeñas: " << NUM_SMALL_CIRCLES << " (Radio: " << smallCircleRadius << ")\n";
    std::cout << "Partículas poligonales: " << NUM_POLYGON_PARTICLES << " (Lados: " << NUM_SIDES << ", Perímetro: " << POLYGON_PERIMETER << ")\n";
    std::cout << "Total de partículas: " << TOTAL_PARTICLES << "\n";
    std::cout << "Ancho del silo: " << SILO_WIDTH << " m\n";
    std::cout << "Altura del silo: " << silo_height << " m\n";
    std::cout << "Abertura del silo: " << OUTLET_WIDTH << " m (" << (OUTLET_WIDTH / (2 * BASE_RADIUS)) << " diámetros base)\n";
    std::cout << "Máximo de avalanchas: " << MAX_AVALANCHES << "\n";
    std::cout << "Simulación Actual: " << CURRENT_SIMULATION << " / " << TOTAL_SIMULATIONS << "\n";

    // Encabezado para flow_data.csv
    flowDataFile << "Time,MassTotal,MassFlowRate,NoPTotal,NoPFlowRate,MassOriginalTotal,MassOriginalFlowRate,NoPOriginalTotal,NoPOriginalFlowRate\n";

    // Configurar mundo Box2D
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = (b2Vec2){0.0f, -9.81f};
    b2WorldId worldId = b2CreateWorld(&worldDef);

    // Definición de la forma de las paredes
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.filter.categoryBits = 0x0001;
    shapeDef.filter.maskBits = 0xFFFF;
    shapeDef.material.friction = 0.5f;
    shapeDef.material.restitution = 0.9f;

    // Crear estructuras del silo
    const float wall_thickness = 0.1f;
    const float ground_level_y = 0.0f;

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
    b2BodyId outletBlockId = b2CreateBody(worldId, &outletBlockDef);
    b2Body_SetType(outletBlockId, b2_staticBody);
    b2Polygon outletBlockShape = b2MakeBox(OUTLET_X_HALF_WIDTH, wall_thickness / 2.0f);
    b2CreatePolygonShape(outletBlockId, &shapeDef, &outletBlockShape);

    // DISTRIBUCIÓN ALEATORIA DE PARTÍCULAS
    const float minX_gen = -SILO_WIDTH / 2.0f + BASE_RADIUS + 0.01f;
    const float maxX_gen = SILO_WIDTH / 2.0f - BASE_RADIUS - 0.01f;
    const float minY_gen = BASE_RADIUS + 0.01f;
    const float maxY_gen = silo_height - BASE_RADIUS - 0.01f;

    // Definir tipos de partículas a crear
    std::vector<ParticleShapeType> particleTypesToCreate;
    for (int i = 0; i < NUM_LARGE_CIRCLES; ++i) particleTypesToCreate.push_back(CIRCLE);
    for (int i = 0; i < NUM_SMALL_CIRCLES; ++i) particleTypesToCreate.push_back(CIRCLE);
    for (int i = 0; i < NUM_POLYGON_PARTICLES; ++i) particleTypesToCreate.push_back(POLYGON);
    std::shuffle(particleTypesToCreate.begin(), particleTypesToCreate.end(), randomEngine);

    const int BOX2D_MAX_POLYGON_VERTICES = 8;
    float Density = 1.0f;

    std::cout << "Generando " << TOTAL_PARTICLES << " partículas con distribución aleatoria...\n";

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
            // Usar posición aleatoria aunque se superponga
            float x = minX_gen + (maxX_gen - minX_gen) * static_cast<float>(rand()) / RAND_MAX;
            float y = minY_gen + (maxY_gen - minY_gen) * static_cast<float>(rand()) / RAND_MAX;
            particlePositions.push_back({x, y});
        }
    }

    // Crear partículas con orientaciones aleatorias
    for (int i = 0; i < TOTAL_PARTICLES; ++i) {
        float particleX = particlePositions[i].first;
        float particleY = particlePositions[i].second;
        
        // Generar ángulo aleatorio para la orientación
        float randomAngle = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
        b2Rot randomRotation = {cosf(randomAngle / 2.0f), sinf(randomAngle / 2.0f)};

        b2BodyDef particleDef = b2DefaultBodyDef();
        particleDef.type = b2_dynamicBody;
        particleDef.position = (b2Vec2){particleX, particleY};
        particleDef.rotation = randomRotation;  // Establecer rotación aleatoria
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

    // SEDIMENTACIÓN
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

        if (SAVE_SIMULATION_DATA && ((int)(sedimentationTime * 100)) % 5 == 0) {
            float negativeTime = -(MAX_SEDIMENTATION_TIME - sedimentationTime);
            simulationDataFile << std::fixed << std::setprecision(5) << negativeTime;
            for (const auto& particle : particles) {
                b2Vec2 pos = b2Body_GetPosition(particle.bodyId);
                float angle = b2Rot_GetAngle(b2Body_GetRotation(particle.bodyId));
                simulationDataFile << "," << pos.x << "," << pos.y << ","
                                << particle.shapeType << "," << particle.size << ","
                                << particle.numSides << "," << angle;
            }
            simulationDataFile << "\n";
        }

        if (sedimentationTime - lastStabilityCheck >= STABILITY_CHECK_INTERVAL) {
            totalKineticEnergy = 0.0f;
            for (const auto& particle : particles) {
                b2Vec2 velocity = b2Body_GetLinearVelocity(particle.bodyId);
                float speed = sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
                totalKineticEnergy += 0.5f * particle.mass * speed * speed;
            }

            float energyChange = abs(totalKineticEnergy - previousKineticEnergy);
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

    // ABRIR EL SILO
    std::cout << "\nABRIENDO SILO - Eliminando bloqueo temporal\n";
    b2DestroyBody(outletBlockId);
    std::cout << "SILO ABIERTO - Iniciando simulación de flujo granular\n\n";

    simulationTime = 0.0f;

    // BUCLE PRINCIPAL DE SIMULACIÓN - LÓGICA SIMPLIFICADA
    bool simulationInterrupted = false;
    int exitedTotalCount = 0;
    float exitedTotalMass = 0.0f;
    int exitedOriginalCount = 0;
    float exitedOriginalMass = 0.0f;
    float timeSinceLastExit = 0.0f;

    while (avalancheCount < MAX_AVALANCHES && !simulationInterrupted) {
        b2World_Step(worldId, TIME_STEP, SUB_STEP_COUNT);
        simulationTime += TIME_STEP;
        frameCounter++;

        applyRandomImpulses(particles);

        manageParticles(worldId, particleBodyIds, simulationTime, lastParticleExitTime, particles,
                       exitedTotalCount, exitedTotalMass, exitedOriginalCount, exitedOriginalMass, silo_height);
        recordFlowData(simulationTime, exitedTotalCount, exitedTotalMass, exitedOriginalCount, exitedOriginalMass);

        timeSinceLastExit = simulationTime - lastParticleExitTime;

        // LÓGICA SIMPLIFICADA: FLUJO -> ATASCO -> RAYCAST -> REANUDACIÓN
        if (!inAvalanche && !inBlockage) {
            // Estado inicial: esperando primer flujo
            if (exitedTotalCount > 0) {
                inAvalanche = true;
                avalancheStartTime = simulationTime;
                avalancheStartParticleCount = totalExitedParticles;
                particlesExitedInCurrentAvalanche.clear();
                std::cout << "Inicio de avalancha " << (avalancheCount + 1) << " a t=" << simulationTime << "s\n";
            }
        }
        else if (inAvalanche) {
            // Durante avalancha: verificar si se atasca
            if (timeSinceLastExit > BLOCKAGE_THRESHOLD) {
                finalizeAvalanche(simulationTime);
                inAvalanche = false;
                inBlockage = true;
                blockageStartTime = simulationTime;
                blockageRetryCount = 0;
                std::cout << "Atasco detectado a t=" << simulationTime << "s\n";
            }
        }
        else if (inBlockage) {
            // Durante atasco: aplicar raycast después de 2 segundos
            if (exitedTotalCount > 0) {
                // Flujo reanudado naturalmente
                float blockageDuration = simulationTime - blockageStartTime;
                totalBlockageTime += blockageDuration;
                inBlockage = false;
                std::cout << "Flujo reanudado después de atasco de " << blockageDuration << "s\n";
            }
            else if (simulationTime - blockageStartTime > 2.0f) {
                if (simulationTime - lastRaycastTime >= RAYCAST_COOLDOWN) {
                    std::cout << "Aplicando raycast para romper atasco a t=" << simulationTime << "s\n";
                    detectAndReinjectArchViaRaycast(worldId, silo_height);
                    lastRaycastTime = simulationTime;
                    blockageRetryCount++;
                    
                    if (blockageRetryCount > MAX_BLOCKAGE_RETRIES) {
                        std::cout << "Bloqueo persistente después de " << MAX_BLOCKAGE_RETRIES << " intentos.\n";
                        simulationInterrupted = true;
                    }
                }
            }
        }

        // Información periódica
        if (simulationTime - lastPrintTime >= 5.0f) {
            std::string currentState = inAvalanche ? "AVALANCHA" : (inBlockage ? "BLOQUEO" : "INICIAL");
            std::cout << "Tiempo: " << std::fixed << std::setprecision(2) << simulationTime
                      << "s, Partículas Salientes: " << totalExitedParticles
                      << ", Avalanchas: " << avalancheCount << "/" << MAX_AVALANCHES
                      << ", Estado: " << currentState << "\n";
            lastPrintTime = simulationTime;
        }

        // Guardado de datos detallados
        if (SAVE_SIMULATION_DATA) {
            simulationDataFile << std::fixed << std::setprecision(5) << simulationTime;
            for (const auto& particle : particles) {
                b2Vec2 pos = b2Body_GetPosition(particle.bodyId);
                float angle = b2Rot_GetAngle(b2Body_GetRotation(particle.bodyId));
                simulationDataFile << "," << pos.x << "," << pos.y << ","
                                << particle.shapeType << "," << particle.size << ","
                                << particle.numSides << "," << angle;
            }
            simulationDataFile << "\n";
        }
    }

    // FINALIZACIÓN
    if (inAvalanche && !simulationInterrupted) {
        finalizeAvalanche(simulationTime);
    }
    if (inBlockage && !simulationInterrupted) {
        totalBlockageTime += (simulationTime - blockageStartTime);
    }

    float totalSimulationTime = simulationTime;
    avalancheDataFile << "\n===== RESUMEN FINAL =====\n";
    avalancheDataFile << "# Tiempo total de simulación: " << totalSimulationTime << " s\n";
    avalancheDataFile << "# Tiempo total en avalanchas: " << totalFlowingTime << " s\n";
    avalancheDataFile << "# Tiempo total en atascos: " << totalBlockageTime << " s\n";
    avalancheDataFile << "# Reintentos de bloqueo realizados: " << blockageRetryCount << "\n";
    avalancheDataFile << "# Simulación interrumpida: " << (simulationInterrupted ? "Sí" : "No") << "\n";
    avalancheDataFile << "# Máximo de avalanchas alcanzado: " << (avalancheCount >= MAX_AVALANCHES ? "Sí" : "No") << "\n";

    if (accumulatedMass > 0) {
        recordFlowData(simulationTime, 0, 0, 0, 0);
    }

    if (SAVE_SIMULATION_DATA) simulationDataFile.close();
    avalancheDataFile.close();
    flowDataFile.close();
    b2DestroyWorld(worldId);

    std::cout << "\n===== SIMULACIÓN COMPLETADA =====\n";
    std::cout << "Avalanchas registradas: " << avalancheCount << "/" << MAX_AVALANCHES << "\n";
    std::cout << "Tiempo total: " << totalSimulationTime << "s | Flujo: " << totalFlowingTime << "s | Atasco: " << totalBlockageTime << "s\n";
    std::cout << "Partículas salientes: " << totalExitedParticles << "\n";

    return 0;
}
