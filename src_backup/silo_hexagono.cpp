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
float BASE_RADIUS = 0.05f;
float SIZE_RATIO = 0.4f;
float CHI = 0.2f;
int TOTAL_PARTICLES = 250;
float OUTLET_WIDTH = 0.3056f;  // Ancho total del orificio
float SILO_WIDTH = 2.6f;       // Ancho total del silo
float silo_height = 11.70f;    // Altura del silo

// Constantes de simulación
const float TIME_STEP = 0.001f;
const int SUB_STEP_COUNT = 4;
const float BLOCKAGE_THRESHOLD = 5.0f;
const float SHOCK_INTERVAL = 0.1f;
const float RANDOM_FORCE_MAGNITUDE = 5.0e-5f;
const float RECORD_INTERVAL = 0.01f;
const float MIN_AVALANCHE_DURATION = 1.0f;  // Tiempo mínimo de flujo para considerar bloqueo roto
const float RAYCAST_COOLDOWN = 2.0f;        // Tiempo mínimo entre raycasts

// Parámetros de reinyección configurables
float REINJECT_HEIGHT_RATIO = 1.0f;        // Fracción de altura para reinyección (0.0-1.0)
float REINJECT_HEIGHT_VARIATION = 0.043f;   // Variación de altura como fracción del silo (0.0-1.0)
float REINJECT_WIDTH_RATIO = 0.31f;         // Fracción del ancho del silo para zona de reinyección (0.0-1.0)

// Variables derivadas
float OUTLET_X_HALF_WIDTH;  // Se calculará como OUTLET_WIDTH / 2.0f

// Parámetros de partículas
int NUM_LARGE_CIRCLES = 0;
int NUM_SMALL_CIRCLES = 0;
int NUM_POLYGON_PARTICLES = 0;
int NUM_SIDES = 5;
float POLYGON_PERIMETER = 0.0f;

// Variables de estado
float simulationTime = 0.0f;
float lastPrintTime = 0.0f;
float lastRaycastTime = -RAYCAST_COOLDOWN;  // Tiempo del último raycast
int frameCounter = 0;

// Variables para guardado de datos
std::ofstream simulationDataFile;
std::ofstream avalancheDataFile;
std::ofstream flowDataFile;
bool SAVE_SIMULATION_DATA = false;
int CURRENT_SIMULATION = 1;
int TOTAL_SIMULATIONS = 1;

// Variables para avalanchas/bloqueos
int avalancheCount = 0;
float totalFlowingTime = 0.0f;
float totalBlockageTime = 0.0f;
bool inAvalanche = false;
bool inBlockage = false;
float blockageStartTime = 0.0f;
float avalancheStartTime = 0.0f;
int particlesInCurrentAvalanche = 0;
int avalancheStartParticleCount = 0;  // Nueva variable para rastrear partículas al inicio de avalancha
float lastExitDuringAvalanche = 0.0f;
float lastParticleExitTime = 0.0f;
float previousBlockageDuration = 0.0f;
int blockageRetryCount = 0;
const int MAX_BLOCKAGE_RETRIES = 15;

// Variables para seguimiento de progreso real del flujo
int lastTotalExitedCount = 0;
float lastProgressTime = 0.0f;
bool waitingForFlowConfirmation = false;  // Estado de espera después de raycast

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
float lastShockTime = 0.0f;
std::mt19937 randomEngine(time(NULL));
std::uniform_real_distribution<> angleDistribution(0.0f, 2.0f * M_PI);
std::uniform_real_distribution<> impulseMagnitudeDistribution(0.0f, RANDOM_FORCE_MAGNITUDE);

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

// Comparador para b2BodyId
struct BodyIdComparator {
    bool operator()(const b2BodyId& a, const b2BodyId& b) const {
        return a.index1 < b.index1;
    }
};

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
            float magnitude = impulseMagnitudeDistribution(randomEngine);
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
    const float EXIT_LEFT_X = -5.0f;
    const float EXIT_RIGHT_X = 5.0f;
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

        if (pos.y < EXIT_BELOW_Y || pos.x < EXIT_LEFT_X || pos.x > EXIT_RIGHT_X) {
            float randomX = REINJECT_MIN_X + (REINJECT_MAX_X - REINJECT_MIN_X) * static_cast<float>(rand()) / RAND_MAX;
            float randomY = REINJECT_MIN_Y + (REINJECT_MAX_Y - REINJECT_MIN_Y) * static_cast<float>(rand()) / RAND_MAX;

            b2Body_SetTransform(particleId, (b2Vec2){randomX, randomY}, (b2Rot){0.0f, 1.0f});
            b2Body_SetLinearVelocity(particleId, (b2Vec2){0.0f, 0.0f});
            b2Body_SetAngularVelocity(particleId, 0.0f);
            b2Body_SetAwake(particleId, true);

            exitedTotalCount++;
            exitedTotalMass += particles[i].mass;
            lastExitTimeRef = currentTime;

            if (particles[i].isOriginal) {
                exitedOriginalCount++;
                exitedOriginalMass += particles[i].mass;
            }
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

// Función para detectar y reinyectar partículas en arcos
void detectAndReinjectArchViaRaycast(b2WorldId worldId, float siloHeight) {
    const float REINJECT_HEIGHT = siloHeight * REINJECT_HEIGHT_RATIO;
    
    // Rango de detección progresivo basado en número de intentos
    // Empezar conservador y aumentar gradualmente si el bloqueo persiste
    float baseRange = OUTLET_WIDTH * 2.0f;  // Empezar con 2x el ancho del outlet
    float progressiveMultiplier = 1.0f + (blockageRetryCount * 0.5f);  // Aumentar 50% por intento
    float maxRange = std::min(siloHeight * 0.4f, 4.0f);  // Máximo: 40% altura o 4m
    const float DETECTION_RANGE = std::min(baseRange * progressiveMultiplier, maxRange);

    RaycastUserData raycastData;
    const int numRays = 120;
    const float maxAngle = M_PI/2;
    const b2Vec2 origin = {0.0f, -0.1f};

    for (int i = 0; i < numRays; ++i) {
        float angle = -maxAngle + (2*maxAngle*i)/(numRays-1);
        b2Vec2 end = {
            origin.x + DETECTION_RANGE * sin(angle),
            origin.y + DETECTION_RANGE * cos(angle)
        };
        raycastData.raySegments.push_back({origin, end});
        b2World_CastRay(worldId, origin, end, b2DefaultQueryFilter(), RaycastCallback, &raycastData);
    }

    if (!raycastData.hitBodies.empty()) {
        int reinjected = 0;
        // Número progresivo de partículas a reinyectar: más agresivo con más intentos
        const int maxReinjectPerStep = std::min(20 + (blockageRetryCount * 5), 50);

        for (b2BodyId body : raycastData.hitBodies) {
            if (reinjected++ >= maxReinjectPerStep) break;

            // Usar la misma lógica de reinyección que manageParticles para consistencia
            const float REINJECT_HALF_WIDTH = SILO_WIDTH * REINJECT_WIDTH_RATIO * 0.5f;
            const float REINJECT_MIN_X = -REINJECT_HALF_WIDTH;
            const float REINJECT_MAX_X = REINJECT_HALF_WIDTH;
            
            b2Vec2 newPos = {
                REINJECT_MIN_X + (REINJECT_MAX_X - REINJECT_MIN_X) * (rand()/(float)RAND_MAX),
                REINJECT_HEIGHT + (rand()/(float)RAND_MAX) * (siloHeight * REINJECT_HEIGHT_VARIATION)
            };

            b2Body_SetTransform(body, newPos, b2Body_GetRotation(body));
            b2Body_SetLinearVelocity(body, {0,0});
            b2Body_SetAngularVelocity(body, 0);
            b2Body_SetAwake(body, true);  // ¡CRÍTICO! Despertar la partícula para evitar congelamiento
        }

        std::cout << "Reinyectadas " << reinjected << " partículas del arco "
                  << "(Intento #" << blockageRetryCount << ", Rango: " << std::fixed << std::setprecision(2) 
                  << DETECTION_RANGE << "m)\n";
    }

    if (SAVE_SIMULATION_DATA) {
        simulationDataFile << ",rays_begin";
        for (const auto& ray : raycastData.raySegments) {
            simulationDataFile << "," << ray.first.x << "," << ray.first.y
                             << "," << ray.second.x << "," << ray.second.y;
        }
        simulationDataFile << ",rays_end";
    }
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
    }

    // Calcular variables derivadas
    OUTLET_X_HALF_WIDTH = OUTLET_WIDTH / 2.0f;

    // Validación de parámetros
    if (REINJECT_HEIGHT_RATIO < 0.1f || REINJECT_HEIGHT_RATIO > 1.2f) {
        std::cerr << "Advertencia: REINJECT_HEIGHT_RATIO (" << REINJECT_HEIGHT_RATIO
                  << ") fuera del rango recomendado [0.1, 0.9]. Ajustando a 0.51.\n";
        REINJECT_HEIGHT_RATIO = 0.51f;
    }

    if (REINJECT_HEIGHT_VARIATION < 0.0f || REINJECT_HEIGHT_VARIATION > 0.2f) {
        std::cerr << "Advertencia: REINJECT_HEIGHT_VARIATION (" << REINJECT_HEIGHT_VARIATION
                  << ") fuera del rango recomendado [0.0, 0.2]. Ajustando a 0.043.\n";
        REINJECT_HEIGHT_VARIATION = 0.043f;
    }

    if (REINJECT_WIDTH_RATIO < 0.1f || REINJECT_WIDTH_RATIO > 0.8f) {
        std::cerr << "Advertencia: REINJECT_WIDTH_RATIO (" << REINJECT_WIDTH_RATIO
                  << ") fuera del rango recomendado [0.1, 0.8]. Ajustando a 0.31.\n";
        REINJECT_WIDTH_RATIO = 0.31f;
    }

    if (silo_height <= 0 || SILO_WIDTH <= 0 || OUTLET_WIDTH <= 0) {
        std::cerr << "Error: Dimensiones del silo deben ser positivas.\n";
        std::cerr << "  silo_height = " << silo_height << "\n";
        std::cerr << "  SILO_WIDTH = " << SILO_WIDTH << "\n";
        std::cerr << "  OUTLET_WIDTH = " << OUTLET_WIDTH << "\n";
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
                  << std::fixed << std::setprecision(4) << POLYGON_PERIMETER << " m (basado en BASE_RADIUS).\n";
    }

    if (CURRENT_SIMULATION > 5) {
        SAVE_SIMULATION_DATA = false;
    }

    srand(time(NULL));

    // Calcular radios de partículas
    const float largeCircleRadius = BASE_RADIUS;
    const float smallCircleRadius = BASE_RADIUS * SIZE_RATIO;

    // Crear directorio de resultados con nombre descriptivo
    std::ostringstream dirNameStream;
    dirNameStream << "sim_" << TOTAL_PARTICLES
                  << "_chi" << std::fixed << std::setprecision(2) << CHI
                  << "_ratio" << std::setprecision(2) << SIZE_RATIO
                  << "_br" << std::setprecision(3) << BASE_RADIUS
                  << "_lg" << NUM_LARGE_CIRCLES
                  << "_sm" << NUM_SMALL_CIRCLES
                  << "_poly" << NUM_POLYGON_PARTICLES
                  << "_sides" << NUM_SIDES
                  << "_outlet" << std::setprecision(2) << OUTLET_WIDTH;

    std::string outputDir;
    if (SAVE_SIMULATION_DATA) {
        outputDir = "./simulations/" + dirNameStream.str() + "/";
    } else {
        outputDir = "./simulations/" + dirNameStream.str() + "/";
    }
    std::filesystem::create_directories(outputDir);

    // Abrir archivos de salida
    if (SAVE_SIMULATION_DATA) {
        simulationDataFile.open(outputDir + "simulation_data.csv");
        simulationDataFile << "Time";
        for (int i = 0; i < TOTAL_PARTICLES; ++i) {
            simulationDataFile << ",p" << i << "_x,p" << i << "_y,p" << i << "_type,p" << i << "_size,p" << i << "_sides";
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
    std::cout << "Zona de reinyección: Ancho=" << (SILO_WIDTH * REINJECT_WIDTH_RATIO) << "m, "
              << "Altura=" << (silo_height * REINJECT_HEIGHT_RATIO) << "-"
              << (silo_height * (REINJECT_HEIGHT_RATIO + REINJECT_HEIGHT_VARIATION)) << "m\n";
    std::cout << "Duración de simulación: 150 segundos\n";
    std::cout << "Magnitud máxima de impulso aleatorio: " << RANDOM_FORCE_MAGNITUDE << " Ns\n";
    std::cout << "Simulación Actual: " << CURRENT_SIMULATION << " / " << TOTAL_SIMULATIONS << "\n";
    std::cout << "Guardar Datos Detallados: " << (SAVE_SIMULATION_DATA ? "Sí" : "No") << "\n";
    std::cout << "Máximo de reintentos para bloqueos: " << MAX_BLOCKAGE_RETRIES << "\n";

    // Encabezado para flow_data.csv
    flowDataFile << "Time,MassTotal,MassFlowRate,NoPTotal,NoPFlowRate,MassOriginalTotal,MassOriginalFlowRate,NoPOriginalTotal,NoPOriginalFlowRate\n";

    // Configurar mundo Box2D
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = (b2Vec2){0.0f, -10.0f};
    b2WorldId worldId = b2CreateWorld(&worldDef);

    // Definición de la forma de las paredes
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.material.friction = 0.5f;
    shapeDef.material.restitution = 0.05f;

    // --- Crear estructuras del silo ---
    const float wall_thickness = 0.1f;
    const float ground_level_y = 0.0f;

    // 1. Pared izquierda
    b2BodyDef leftWallDef = b2DefaultBodyDef();
    leftWallDef.position = (b2Vec2){-(SILO_WIDTH / 2.0f) - (wall_thickness / 2.0f), ground_level_y + (silo_height / 2.0f)};
    b2BodyId leftWallId = b2CreateBody(worldId, &leftWallDef);
    b2Body_SetType(leftWallId, b2_staticBody);
    b2Polygon leftWallShape = b2MakeBox(wall_thickness / 2.0f, silo_height / 2.0f);
    b2CreatePolygonShape(leftWallId, &shapeDef, &leftWallShape);

    // 2. Pared derecha
    b2BodyDef rightWallDef = b2DefaultBodyDef();
    rightWallDef.position = (b2Vec2){(SILO_WIDTH / 2.0f) + (wall_thickness / 2.0f), ground_level_y + (silo_height / 2.0f)};
    b2BodyId rightWallId = b2CreateBody(worldId, &rightWallDef);
    b2Body_SetType(rightWallId, b2_staticBody);
    b2Polygon rightWallShape = b2MakeBox(wall_thickness / 2.0f, silo_height / 2.0f);
    b2CreatePolygonShape(rightWallId, &shapeDef, &rightWallShape);

    // 3. Fondo izquierdo
    b2BodyDef groundLeftDef = b2DefaultBodyDef();
    groundLeftDef.position = (b2Vec2){(-SILO_WIDTH / 2.0f + -OUTLET_X_HALF_WIDTH) / 2.0f, ground_level_y - (wall_thickness / 2.0f)};
    b2BodyId groundLeftId = b2CreateBody(worldId, &groundLeftDef);
    b2Body_SetType(groundLeftId, b2_staticBody);
    b2Polygon groundLeftShape = b2MakeBox((SILO_WIDTH / 2.0f - OUTLET_X_HALF_WIDTH) / 2.0f, wall_thickness / 2.0f);
    b2CreatePolygonShape(groundLeftId, &shapeDef, &groundLeftShape);

    // 4. Fondo derecho
    b2BodyDef groundRightDef = b2DefaultBodyDef();
    groundRightDef.position = (b2Vec2){(OUTLET_X_HALF_WIDTH + SILO_WIDTH / 2.0f) / 2.0f, ground_level_y - (wall_thickness / 2.0f)};
    b2BodyId groundRightId = b2CreateBody(worldId, &groundRightDef);
    b2Body_SetType(groundRightId, b2_staticBody);
    b2Polygon groundRightShape = b2MakeBox((SILO_WIDTH / 2.0f - OUTLET_X_HALF_WIDTH) / 2.0f, wall_thickness / 2.0f);
    b2CreatePolygonShape(groundRightId, &shapeDef, &groundRightShape);

    // Rango de generación de partículas
    const float minX_gen = -SILO_WIDTH / 2.0f + BASE_RADIUS;
    const float maxX_gen = SILO_WIDTH / 2.0f - BASE_RADIUS;
    const float minY_gen = ground_level_y + BASE_RADIUS * 2.0f;
    const float maxY_gen = silo_height - BASE_RADIUS * 2.0f;

    // Definir tipos de partículas a crear
    std::vector<ParticleShapeType> particleTypesToCreate;
    particleTypesToCreate.reserve(TOTAL_PARTICLES);

    for (int i = 0; i < NUM_LARGE_CIRCLES; ++i) {
        particleTypesToCreate.push_back(CIRCLE);
    }
    for (int i = 0; i < NUM_SMALL_CIRCLES; ++i) {
        particleTypesToCreate.push_back(CIRCLE);
    }
    for (int i = 0; i < NUM_POLYGON_PARTICLES; ++i) {
        particleTypesToCreate.push_back(POLYGON);
    }

    // Mezclar los tipos de partículas
    std::shuffle(particleTypesToCreate.begin(), particleTypesToCreate.end(), randomEngine);

    const int BOX2D_MAX_POLYGON_VERTICES = 8;
    const float POLYGON_SKIN_RADIUS = 0.005f;

    // Crear partículas
    for (int i = 0; i < TOTAL_PARTICLES; ++i) {
        float randomX = minX_gen + (maxX_gen - minX_gen) * static_cast<float>(rand()) / RAND_MAX;
        float randomY = minY_gen + (maxY_gen - minY_gen) * static_cast<float>(rand()) / RAND_MAX;

        b2BodyDef particleDef = b2DefaultBodyDef();
        particleDef.type = b2_dynamicBody;
        particleDef.position = (b2Vec2){randomX, randomY};
        b2BodyId particleId = b2CreateBody(worldId, &particleDef);

        b2ShapeDef particleShapeDef = b2DefaultShapeDef();
        particleShapeDef.density = 1.0f;
        particleShapeDef.material.friction = 0.5f;
        particleShapeDef.material.restitution = 0.1f;

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
            if (currentNumSides < 3) {
                std::cerr << "Error: Número de lados para polígono debe ser al menos 3. Ajustando a 3.\n";
                currentNumSides = 3;
            }
            float polyCircumRadius = POLYGON_PERIMETER / (2.0f * currentNumSides * sin(M_PI / currentNumSides));
            currentParticleSize = polyCircumRadius;

            b2Vec2 vertices[BOX2D_MAX_POLYGON_VERTICES];
            int actualNumSides = currentNumSides;
            if (currentNumSides > BOX2D_MAX_POLYGON_VERTICES) {
                std::cerr << "Advertencia: Lados polígono (" << currentNumSides << ") excede máximo Box2D ("
                          << BOX2D_MAX_POLYGON_VERTICES << "). Usando " << BOX2D_MAX_POLYGON_VERTICES << " lados.\n";
                actualNumSides = BOX2D_MAX_POLYGON_VERTICES;
            }

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

    // Bucle principal de simulación
    bool simulationInterrupted = false;
    int exitedTotalCount = 0;
    float exitedTotalMass = 0.0f;
    int exitedOriginalCount = 0;
    float exitedOriginalMass = 0.0f;
    float timeSinceLastExit = 0.0f;

    while (simulationTime < 150.0f && !simulationInterrupted) {
        // Paso de simulación Box2D
        b2World_Step(worldId, TIME_STEP, SUB_STEP_COUNT);
        simulationTime += TIME_STEP;
        frameCounter++;

        // Manejo de partículas y registro de flujo
        manageParticles(worldId, particleBodyIds, simulationTime, lastParticleExitTime, particles,
                       exitedTotalCount, exitedTotalMass, exitedOriginalCount, exitedOriginalMass, silo_height);
        recordFlowData(simulationTime, exitedTotalCount, exitedTotalMass, exitedOriginalCount, exitedOriginalMass);

        // Cálculo del tiempo desde la última salida y progreso real
        timeSinceLastExit = simulationTime - lastParticleExitTime;

        // Detectar progreso real del flujo (incremento en partículas totales salientes)
        bool hasFlowProgress = (totalExitedParticles > lastTotalExitedCount);
        float timeSinceLastProgress = simulationTime - lastProgressTime;

        if (hasFlowProgress) {
            lastTotalExitedCount = totalExitedParticles;
            lastProgressTime = simulationTime;
            timeSinceLastProgress = 0.0f;
        }

        // Lógica de estados mejorada basada en progreso real
        if (inAvalanche) {
            if (timeSinceLastProgress > BLOCKAGE_THRESHOLD) {
                // Terminar avalancha cuando se detecta bloqueo
                float currentAvalancheDuration = simulationTime - avalancheStartTime;
                if (currentAvalancheDuration >= MIN_AVALANCHE_DURATION) {
                    totalFlowingTime += currentAvalancheDuration;
                    // Calcular correctamente las partículas en la avalancha
                    int particlesInThisAvalanche = totalExitedParticles - avalancheStartParticleCount;
                    avalancheDataFile << "Avalancha " << (avalancheCount + 1) << ","
                                    << avalancheStartTime << ","
                                    << simulationTime << ","
                                    << currentAvalancheDuration << ","
                                    << particlesInThisAvalanche << "\n";
                    avalancheCount++;
                }
                inAvalanche = false;
                inBlockage = true;
                blockageStartTime = simulationTime;
                waitingForFlowConfirmation = false;  // Reset estado de espera
            } else if (hasFlowProgress) {
                lastExitDuringAvalanche = simulationTime;
            }
        }
        else if (inBlockage) {
            // Solo intentar romper el bloqueo si ha pasado el cooldown y sigue bloqueado
            if ((simulationTime - lastRaycastTime >= RAYCAST_COOLDOWN) &&
                (timeSinceLastProgress > BLOCKAGE_THRESHOLD)) {

                detectAndReinjectArchViaRaycast(worldId, silo_height);
                blockageRetryCount++;
                lastRaycastTime = simulationTime;
                waitingForFlowConfirmation = true;  // Activar estado de espera

                if (blockageRetryCount > MAX_BLOCKAGE_RETRIES) {
                    std::cout << "Bloqueo persistente después de " << MAX_BLOCKAGE_RETRIES
                              << " intentos. Interrumpiendo simulación.\n";
                    simulationInterrupted = true;
                    break;
                }
            }

            // Verificar si el bloqueo persiste después de dar tiempo al flujo
            if (timeSinceLastProgress > BLOCKAGE_THRESHOLD + MIN_AVALANCHE_DURATION) {
                totalBlockageTime += TIME_STEP;
            }

            // Si hay progreso real después del raycast, salir del bloqueo
            if (hasFlowProgress && waitingForFlowConfirmation) {
                totalBlockageTime += (simulationTime - blockageStartTime);
                inBlockage = false;
                waitingForFlowConfirmation = false;
                // No iniciar avalancha inmediatamente, esperar confirmación sostenida
            } else if (hasFlowProgress && !waitingForFlowConfirmation) {
                // Progreso sin raycast reciente - transición normal
                totalBlockageTime += (simulationTime - blockageStartTime);
                inBlockage = false;
                inAvalanche = true;
                avalancheStartTime = simulationTime;
                avalancheStartParticleCount = totalExitedParticles;  // Guardar punto de inicio
                lastExitDuringAvalanche = simulationTime;
                blockageRetryCount = 0;  // Reset contador cuando se inicia avalancha normal
            }
        }
        else {
            // Estado inicial o después de raycast esperando confirmación
            if (hasFlowProgress && !waitingForFlowConfirmation) {
                inAvalanche = true;
                avalancheStartTime = simulationTime;
                avalancheStartParticleCount = totalExitedParticles;  // Guardar punto de inicio
                lastExitDuringAvalanche = simulationTime;
                blockageRetryCount = 0;  // Reset contador cuando se inicia avalancha desde estado inicial
            } else if (hasFlowProgress && waitingForFlowConfirmation) {
                // Confirmar que el flujo es sostenido después del raycast
                if (timeSinceLastProgress <= 1.0f) {  // Flujo sostenido por 1 segundo
                    waitingForFlowConfirmation = false;
                    inAvalanche = true;
                    avalancheStartTime = simulationTime;
                    avalancheStartParticleCount = totalExitedParticles;  // Guardar punto de inicio
                    lastExitDuringAvalanche = simulationTime;
                    blockageRetryCount = 0;  // Reset contador solo cuando hay flujo confirmado
                }
            } else if (timeSinceLastProgress > BLOCKAGE_THRESHOLD && simulationTime > BLOCKAGE_THRESHOLD) {
                inBlockage = true;
                blockageStartTime = simulationTime - BLOCKAGE_THRESHOLD;
                waitingForFlowConfirmation = false;
            }
        }        // Información periódica
        if (simulationTime - lastPrintTime >= 5.0f || simulationTime >= 150.0f - TIME_STEP) {
            std::string currentState = inAvalanche ? "AVALANCHA" :
                                     (inBlockage ? "BLOQUEO" :
                                     (waitingForFlowConfirmation ? "ESPERANDO" : "INICIAL"));
            std::cout << "Tiempo: " << std::fixed << std::setprecision(2) << simulationTime
                      << "s, Partículas Salientes: " << totalExitedParticles
                      << ", Masa Saliente: " << totalExitedMass
                      << ", Originales Salientes: " << totalExitedOriginalParticles
                      << ", Reintentos de bloqueo: " << blockageRetryCount
                      << ", Progreso: " << std::setprecision(1) << timeSinceLastProgress << "s"
                      << ", Estado: " << currentState
                      << "\n";
            lastPrintTime = simulationTime;
        }

        // Guardado de datos detallados
        if (SAVE_SIMULATION_DATA) {
            simulationDataFile << std::fixed << std::setprecision(5) << simulationTime;
            for (const auto& particle : particles) {
                b2Vec2 pos = b2Body_GetPosition(particle.bodyId);
                simulationDataFile << "," << pos.x << "," << pos.y << ","
                                   << particle.shapeType << "," << particle.size << "," << particle.numSides;
            }
            simulationDataFile << "\n";
        }
    }

    // Resumen final
    if (inAvalanche && !simulationInterrupted) {
        float currentAvalancheDuration = simulationTime - avalancheStartTime;
        totalFlowingTime += currentAvalancheDuration;
        // Calcular correctamente las partículas en la avalancha final
        int particlesInThisAvalanche = totalExitedParticles - avalancheStartParticleCount;
        avalancheDataFile << "Avalancha " << (avalancheCount + 1) << ","
                          << avalancheStartTime << ","
                          << simulationTime << ","
                          << currentAvalancheDuration << ","
                          << particlesInThisAvalanche << "\n";
        avalancheCount++;
    }

    if (inBlockage && !simulationInterrupted) {
        totalBlockageTime += (simulationTime - blockageStartTime);
    }

    float totalSimulationTime = simulationTime;
    float timeDiscrepancy = totalSimulationTime - (totalFlowingTime + totalBlockageTime);

    avalancheDataFile << "\n===== RESUMEN FINAL =====\n";
    avalancheDataFile << "# Tiempo total de simulación: " << totalSimulationTime << " s\n";
    avalancheDataFile << "# Tiempo total en avalanchas: " << totalFlowingTime << " s\n";
    avalancheDataFile << "# Tiempo total en atascos: " << totalBlockageTime << " s\n";
    avalancheDataFile << "# Diferencia de tiempo: " << timeDiscrepancy << " s\n";
    avalancheDataFile << "# Suma de estados: " << (totalFlowingTime + totalBlockageTime) << " s\n";
    avalancheDataFile << "# Reintentos de bloqueo realizados: " << blockageRetryCount << "\n";
    avalancheDataFile << "# Simulación interrumpida: " << (simulationInterrupted ? "Sí" : "No") << "\n";

    if (accumulatedMass > 0 || accumulatedParticles > 0 || accumulatedOriginalMass > 0 || accumulatedOriginalParticles > 0) {
        recordFlowData(simulationTime, 0, 0, 0, 0);
    }

    if (SAVE_SIMULATION_DATA) {
        simulationDataFile.close();
    }
    avalancheDataFile.close();
    flowDataFile.close();

    b2DestroyWorld(worldId);

    std::cout << "\n===== SIMULACIÓN COMPLETADA =====\n";
    std::cout << "Frames simulados: " << frameCounter << "\n";
    std::cout << "Avalanchas registradas: " << avalancheCount << "\n";
    std::cout << "Tiempo total de simulación: " << totalSimulationTime << " s\n";
    std::cout << "Tiempo en avalanchas: " << totalFlowingTime << " s\n";
    std::cout << "Tiempo en atascos: " << totalBlockageTime << " s\n";
    std::cout << "Reintentos de bloqueo realizados: " << blockageRetryCount << "\n";
    std::cout << "Simulación interrumpida: " << (simulationInterrupted ? "Sí" : "No") << "\n";
    std::cout << "Partículas totales salientes: " << totalExitedParticles << "\n";
    std::cout << "Masa total de partículas salientes: " << totalExitedMass << "\n";
    std::cout << "Partículas originales totales salientes: " << totalExitedOriginalParticles << "\n";
    std::cout << "Masa total de partículas originales salientes: " << totalExitedOriginalMass << "\n";
    std::cout << "Datos de flujo (incluyendo originales) en: " << outputDir << "flow_data.csv\n";

    return 0;
}