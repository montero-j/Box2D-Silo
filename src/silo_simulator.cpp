#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <string>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <random>
#include <cstring>
#include <set>
#include <cstdint>

#include "box2d/box2d.h"

// Constantes de simulación
const float TIME_STEP = 1.0f / 60.0f;
const int SUB_STEP_COUNT = 4;
const float BLOCKAGE_THRESHOLD = 5.0f;
const float SHOCK_INTERVAL = 1.0f;
const float BASE_SHOCK_MAGNITUDE = 30.0f;
const int MAX_SHOCK_ATTEMPTS = 5;
const float RANDOM_FORCE_MAX = 5e-1f; // 5x10^-2 N
const float OUTLET_X_HALF_WIDTH = 0.7f; // La mitad del ancho del orificio (1.4m / 2)

// Parámetros ajustables (se establecerán desde main)
float BASE_RADIUS;
float SIZE_RATIO;
float CHI;
int TOTAL_PARTICLES;
int NUM_POLYGON_PARTICLES;
int MAX_AVALANCHES; // Número de avalanchas a registrar

// Variables para guardado de datos
std::ofstream simulationDataFile;
std::ofstream avalancheDataFile;
std::ofstream flowDataFile;
int frameCounter = 0;

// Variables para detección de avalanchas
int avalancheCount = 0;
float lastParticleExitTime = 0.0f;
bool inBlockage = true;
bool inAvalanche = false;
float blockageStartTime = 0.0f;
float avalancheStartTime = 0.0f;
int particlesInCurrentAvalanche = 0;
float previousBlockageDuration = 0.0f;
float lastExitDuringAvalanche = 0.0f;

// Variables para tiempos acumulados
float totalFlowingTime = 0.0f;
float totalBlockageTime = 0.0f;

// Variables para gestión de golpes
float lastShockTime = 0.0f;
int shockAttempts = 0;
bool shockAppliedThisFrame = false;

// Variables para registro de flujo
float totalExitedMass = 0.0f;
int totalExitedParticles = 0;
float lastRecordedTime = -0.01f;
const float RECORD_INTERVAL = 0.01f;
float accumulatedMass = 0.0f;
int accumulatedParticles = 0;

// Motores de números aleatorios
std::mt19937 randomEngine(time(NULL));
std::uniform_real_distribution<> angleDistribution(0.0f, 2.0f * M_PI);
std::uniform_real_distribution<> magnitudeDistribution(0.0f, RANDOM_FORCE_MAX);

// Definición de tipos de partículas
enum ParticleShapeType {
    CIRCLE,
    POLYGON
};

struct ParticleInfo {
    b2BodyId bodyId;
    ParticleShapeType shapeType;
    float size;
    float mass;
};

// Variables para control de múltiples simulaciones
int CURRENT_SIMULATION = 1;
int TOTAL_SIMULATIONS = 50;
bool SAVE_SIMULATION_DATA = true;

// Comparador para b2BodyId corregido para Box2D v3
struct CompareB2BodyId {
    bool operator()(b2BodyId a, b2BodyId b) const {
        if (a.world0 != b.world0) return a.world0 < b.world0;
        return a.index1 < b.index1; // Compare index1 if world0 is the same
    }
};

// Estructura para pasar datos al callback del raycast
struct RaycastUserData {
    std::set<b2BodyId, CompareB2BodyId> hitBodies;
};

// Callback de raycast corregido para Box2D v3
// La firma debe coincidir con float (*)(b2ShapeId, b2Vec2, b2Vec2, float, void*)
float RaycastCallback(b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context) {
    b2BodyId bodyId = b2Shape_GetBody(shapeId);

    if (b2Body_GetType(bodyId) == b2_dynamicBody) {
        RaycastUserData* data = static_cast<RaycastUserData*>(context);
        data->hitBodies.insert(bodyId);
    }

    // Retorna la fracción para que el raycast se detenga en la primera colisión
    // Esto asegura que solo las partículas más cercanas al origen del rayo sean detectadas.
    return fraction; 
}


// Función para guardar el estado de la simulación
void saveSimulationState(b2WorldId worldId, const std::vector<ParticleInfo>& particles,
                         b2BodyId groundIdleft, b2BodyId groundIdright,
                         b2BodyId leftWallId, b2BodyId rightWallId) {
    // Si no se debe guardar, salir
    if (!SAVE_SIMULATION_DATA) {
        return;
    }

    // Guardar información de las paredes
    b2Vec2 groundLeftPos = b2Body_GetPosition(groundIdleft);
    b2Vec2 groundRightPos = b2Body_GetPosition(groundIdright);
    b2Vec2 leftWallPos = b2Body_GetPosition(leftWallId);
    b2Vec2 rightWallPos = b2Body_GetPosition(rightWallId);
    
    simulationDataFile << "Walls "
                       << groundLeftPos.x << " " << groundLeftPos.y << " "
                       << groundRightPos.x << " " << groundRightPos.y << " "
                       << leftWallPos.x << " " << leftWallPos.y << " "
                       << rightWallPos.x << " " << rightWallPos.y << "\n";
    
    // Guardar información de cada partícula
    for (const auto& particle : particles) {
        b2Vec2 pos = b2Body_GetPosition(particle.bodyId);
        b2Rot rotation = b2Body_GetRotation(particle.bodyId);
        float angle = b2Rot_GetAngle(rotation);
        
        simulationDataFile << static_cast<int>(particle.shapeType) << " "
                           << pos.x << " " << pos.y << " "
                           << angle << " " << particle.size << "\n";
    }
    
    simulationDataFile << "EndFrame\n";
}

// Función para aplicar golpe durante atascos
void applyBlockageShock(std::vector<ParticleInfo>& particles, float magnitude) {
    for (const auto& particle : particles) {
        float randomAngle = angleDistribution(randomEngine);
        b2Vec2 forceDirection = {cos(randomAngle), sin(randomAngle)};
        b2Vec2 force = {forceDirection.x * magnitude,
                        forceDirection.y * magnitude};
        b2Vec2 particlePos = b2Body_GetPosition(particle.bodyId);
        b2Body_ApplyForce(particle.bodyId, force, particlePos, true);
    }
}

// Aplicar fuerzas aleatorias constantes a todas las partículas
void applyRandomForces(std::vector<ParticleInfo>& particles) {
    for (const auto& particle : particles) {
        // Generar magnitud y dirección aleatorias
        float magnitude = magnitudeDistribution(randomEngine);
        float angle = angleDistribution(randomEngine);
        b2Vec2 force = {
            magnitude * cos(angle),
            magnitude * sin(angle)
        };
        // Aplicar fuerza al centro de masa
        b2Body_ApplyForceToCenter(particle.bodyId, force, true);
    }
}

// Función para gestionar partículas que salen del silo
std::pair<int, float> manageParticles(b2WorldId worldId, std::vector<b2BodyId>& particleIds,
                                      float currentTime, float& lastExitTimeRef,
                                      const std::vector<ParticleInfo>& particles) {
    
    const float EXIT_BELOW_Y = -1.5f;
    const float EXIT_LEFT_X = -5.5f;
    const float EXIT_RIGHT_X = 5.5f;
    const float REINJECT_MIN_X = -3.5f;
    const float REINJECT_MAX_X = 3.5f;
    const float REINJECT_MIN_Y = 15.0f;
    const float REINJECT_MAX_Y = 18.0f;

    int exitedCount = 0;
    float exitedMass = 0.0f;

    for (size_t i = 0; i < particleIds.size(); ++i) {
        b2BodyId particleId = particleIds[i];
        b2Vec2 pos = b2Body_GetPosition(particleId);

        if (pos.y < EXIT_BELOW_Y || pos.x < EXIT_LEFT_X || pos.x > EXIT_RIGHT_X) {
            float randomX = REINJECT_MIN_X + (REINJECT_MAX_X - REINJECT_MIN_X) * static_cast<float>(rand()) / RAND_MAX;
            float randomY = REINJECT_MIN_Y + (REINJECT_MAX_Y - REINJECT_MIN_Y) * static_cast<float>(rand()) / RAND_MAX;

            b2Body_SetTransform(particleId, (b2Vec2){randomX, randomY}, b2Body_GetRotation(particleId));
            b2Body_SetLinearVelocity(particleId, (b2Vec2){0.0f, 0.0f});
            b2Body_SetAngularVelocity(particleId, 0.0f);
            
            // Solo contar si es una partícula grande (radio igual a BASE_RADIUS)
            // Asumiendo que el índice de `particles` corresponde al de `particleIds`
            // se necesitaría un mapa o una búsqueda.
            // Para este código, se asume sincronización por el for-loop.
            if (particles[i].size == BASE_RADIUS && particles[i].shapeType == CIRCLE) {
                exitedCount++;
                exitedMass += particles[i].mass;
                lastExitTimeRef = currentTime;
            }
        }
    }
    
    return {exitedCount, exitedMass};
}

// Función para detectar y reinyectar arcos vía raycast
void detectAndReinjectArchViaRaycast(b2WorldId worldId) {
    const float REINJECT_MIN_X = -3.5f;
    const float REINJECT_MAX_X = 3.5f;
    const float REINJECT_MIN_Y = 15.0f;
    const float REINJECT_MAX_Y = 18.0f;

    // Preparamos la estructura que recogerá los IDs de las partículas
    RaycastUserData raycastData;

    // Parámetros para el "abanico" de rayos
    const int numRays = 25; // Número de rayos a disparar (más rayos = más precisión)
    const float maxAngle = M_PI / 3.0f; // Abanico de 120 grados (-60 a +60)
    const float rayLength = 5.0f;       // Longitud del rayo, suficiente para cruzar el arco
    const b2Vec2 origin = {0.0f, -0.5f}; // Origen de los rayos, justo en el centro de la abertura

    // Disparamos los rayos en un bucle
    for (int i = 0; i < numRays; ++i) {
        // Calculamos el ángulo para el rayo actual
        float angle = -maxAngle + (2.0f * maxAngle * i) / (numRays - 1);
        
        // Calculamos el vector de traslación (dirección y fin del rayo)
        // Usamos sin(angle) para X y cos(angle) para Y para un abanico vertical
        b2Vec2 translation = {rayLength * sin(angle), rayLength * cos(angle)};

        // Ejecutamos el raycast pasando origen, traslación, el filtro de consulta, el callback y los datos de usuario
        b2World_CastRay(worldId, origin, translation, b2DefaultQueryFilter(), RaycastCallback, &raycastData);
    }

    // Ahora, 'raycastData.hitBodies' contiene los IDs de las partículas que forman el arco.
    // Procedemos a reinyectarlas.
    if (!raycastData.hitBodies.empty()) {
        std::cout << "Raycast detectó " << raycastData.hitBodies.size() << " partículas en el arco. Reinyectando...\n";
        
        for (b2BodyId bodyToReinject : raycastData.hitBodies) {
            float randomX = REINJECT_MIN_X + (REINJECT_MAX_X - REINJECT_MIN_X) * static_cast<float>(rand()) / RAND_MAX;
            float randomY = REINJECT_MIN_Y + (REINJECT_MAX_Y - REINJECT_MIN_Y) * static_cast<float>(rand()) / RAND_MAX;
            
            // Obtenemos la rotación actual para mantenerla si queremos
            b2Rot currentRotation = b2Body_GetRotation(bodyToReinject);

            // Movemos la partícula a la parte superior
            b2Body_SetTransform(bodyToReinject, {randomX, randomY}, currentRotation);
            b2Body_SetLinearVelocity(bodyToReinject, {0.0f, 0.0f});
            b2Body_SetAngularVelocity(bodyToReinject, 0.0f);
        }
    } else {
        std::cout << "Raycast no detectó un arco claro.\n";
    }
}


// Función para registrar datos de flujo
void recordFlowData(float currentTime, int exitedCount, float exitedMass) {
    accumulatedMass += exitedMass;
    accumulatedParticles += exitedCount;
    
    // Solo registrar en intervalos específicos
    if (currentTime - lastRecordedTime >= RECORD_INTERVAL) {
        float timeSinceLast = currentTime - lastRecordedTime;
        
        // Calcular tasas de flujo
        float massFlowRate = (timeSinceLast > 0) ? accumulatedMass / timeSinceLast : 0.0f;
        float particleFlowRate = (timeSinceLast > 0) ? accumulatedParticles / timeSinceLast : 0.0f;
        
        // Actualizar totales
        totalExitedMass += accumulatedMass;
        totalExitedParticles += accumulatedParticles;
        
        // Escribir datos
        flowDataFile << std::fixed << std::setprecision(2) << currentTime << ","
                     << totalExitedMass << ","
                     << massFlowRate << ","
                     << totalExitedParticles << ","
                     << particleFlowRate << "\n";
        
        // Resetear acumuladores
        accumulatedMass = 0.0f;
        accumulatedParticles = 0;
        lastRecordedTime = currentTime;
    }
}

int main(int argc, char* argv[]) {
    // Valores por defecto
    BASE_RADIUS = 0.25f;
    SIZE_RATIO = 0.8f;
    CHI = 0.4286f;
    TOTAL_PARTICLES = 780;
    NUM_POLYGON_PARTICLES = 0;
    MAX_AVALANCHES = 100;  // Valor por defecto

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
        else if (strcmp(argv[i], "--total-particles") == 0 && i + 1 < argc) {
            TOTAL_PARTICLES = std::stoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--polygon-particles") == 0 && i + 1 < argc) {
            NUM_POLYGON_PARTICLES = std::stoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--max-avalanches") == 0 && i + 1 < argc) {
            MAX_AVALANCHES = std::stoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--target-r") == 0 && i + 1 < argc) {
            SIZE_RATIO = std::stof(argv[++i]);
        }
        else if (strcmp(argv[i], "--target-chi") == 0 && i + 1 < argc) {
            CHI = std::stof(argv[++i]);
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
    }

    // Solo guardar datos de simulación en las primeras 5 simulaciones
    if (CURRENT_SIMULATION > 5) {
        SAVE_SIMULATION_DATA = false;
    }

    srand(time(NULL));
    
    // Calcular parámetros de partículas basados en constantes
    const float particleRadius = BASE_RADIUS;
    const float particlesmallRadius = BASE_RADIUS * SIZE_RATIO;
    
    const int numLargeParticles = static_cast<int>(CHI * (TOTAL_PARTICLES - NUM_POLYGON_PARTICLES));
    const int numSmallParticles = (TOTAL_PARTICLES - NUM_POLYGON_PARTICLES) - numLargeParticles;
    
    // Imprimir parámetros para verificación
    std::cout << "===== PARÁMETROS DE SIMULACIÓN =====\n";
    std::cout << "Simulación actual: " << CURRENT_SIMULATION << " de " << TOTAL_SIMULATIONS << "\n";
    std::cout << "Guardar datos de simulación: " << (SAVE_SIMULATION_DATA ? "Sí" : "No") << "\n";
    std::cout << "Radio grande: " << particleRadius << " m\n";
    std::cout << "Radio pequeño: " << particlesmallRadius << " m (Razón: " << SIZE_RATIO << ")\n";
    std::cout << "Fracción partículas grandes (χ): " << CHI << "\n";
    std::cout << "Número partículas grandes: " << numLargeParticles << "\n";
    std::cout << "Número partículas pequeñas: " << numSmallParticles << "\n";
    std::cout << "Número partículas poligonales: " << NUM_POLYGON_PARTICLES << "\n";
    std::cout << "Total partículas: " << (numLargeParticles + numSmallParticles + NUM_POLYGON_PARTICLES) << "\n";
    std::cout << "Abertura del silo: 1.4 m (" << (1.4f / (2 * particleRadius)) << " diámetros grandes)\n";
    std::cout << "Número de avalanchas a registrar: " << MAX_AVALANCHES << "\n";
    std::cout << "Fuerza aleatoria máxima: " << RANDOM_FORCE_MAX << " N\n";
    std::cout << "====================================\n";
    
    // Crear directorio de salida (incluye parámetros y número de simulación)
    std::string outputDir = "simulation_data/r_" + std::to_string(SIZE_RATIO) +
                            "_chi_" + std::to_string(CHI) +
                            "_sim_" + std::to_string(CURRENT_SIMULATION) + "/";
    std::filesystem::create_directories(outputDir);
    
    // Abrir archivos de datos
    simulationDataFile.open(outputDir + "simulation_data.txt");
    avalancheDataFile.open(outputDir + "avalanches.txt");
    flowDataFile.open(outputDir + "flow_data.csv");
    
    if ((SAVE_SIMULATION_DATA && !simulationDataFile.is_open()) ||
        !avalancheDataFile.is_open() ||
        !flowDataFile.is_open()) {
        std::cerr << "Error al abrir archivos de datos!" << std::endl;
        return -1;
    }
    
    // Escribir encabezados para archivos
    avalancheDataFile << "# AvalancheNumber ParticleCount AvalancheDuration(s) BlockageDuration(s)\n";
    avalancheDataFile << "# Parameters: BASE_RADIUS=" << BASE_RADIUS
                      << " SIZE_RATIO=" << SIZE_RATIO
                      << " CHI=" << CHI
                      << " TOTAL_PARTICLES=" << TOTAL_PARTICLES
                      << " NUM_POLYGON_PARTICLES=" << NUM_POLYGON_PARTICLES
                      << " MAX_AVALANCHES=" << MAX_AVALANCHES
                      << " SIMULATION_NUM=" << CURRENT_SIMULATION
                      << " RANDOM_FORCE_MAX=" << RANDOM_FORCE_MAX << "\n";
    
    flowDataFile << "Time,MassTotal,MassFlowRate,NoPTotal,NoPFlowRate\n";
    
    // Configurar mundo Box2D
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = (b2Vec2){0.0f, -10.0f};
    b2WorldId worldId = b2CreateWorld(&worldDef);
    
    // Crear estructuras del silo
    b2BodyDef groundDefleft = b2DefaultBodyDef();
    groundDefleft.position = (b2Vec2){-3.0f, -0.25f};
    b2BodyId groundIdleft = b2CreateBody(worldId, &groundDefleft);
    b2Body_SetType(groundIdleft, b2_staticBody);
    b2Polygon groundShapeleft = b2MakeBox(2.3f, 0.5f);
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    b2CreatePolygonShape(groundIdleft, &shapeDef, &groundShapeleft);

    b2BodyDef groundDefright = b2DefaultBodyDef();
    groundDefright.position = (b2Vec2){3.0f, -0.25f};
    b2BodyId groundIdright = b2CreateBody(worldId, &groundDefright);
    b2Body_SetType(groundIdright, b2_staticBody);
    b2Polygon groundShaperight = b2MakeBox(2.3f, 0.5f);
    b2CreatePolygonShape(groundIdright, &shapeDef, &groundShaperight);

    b2BodyDef leftWallDef = b2DefaultBodyDef();
    leftWallDef.position = (b2Vec2){-5.0f, 7.5f};
    b2BodyId leftWallId = b2CreateBody(worldId, &leftWallDef);
    b2Body_SetType(leftWallId, b2_staticBody);
    b2Polygon leftWallShape = b2MakeBox(0.5f, 15.0f);
    b2CreatePolygonShape(leftWallId, &shapeDef, &leftWallShape);

    b2BodyDef rightWallDef = b2DefaultBodyDef();
    rightWallDef.position = (b2Vec2){5.0f, 7.5f};
    b2BodyId rightWallId = b2CreateBody(worldId, &rightWallDef);
    b2Body_SetType(rightWallId, b2_staticBody);
    b2Polygon rightWallShape = b2MakeBox(0.5f, 15.0f);
    b2CreatePolygonShape(rightWallId, &shapeDef, &rightWallShape);

    // Crear partículas
    std::vector<ParticleInfo> particles;
    std::vector<b2BodyId> particleBodyIds;
    
    // Tamaño de partículas poligonales
    const float polygonSideLength = 0.35f;
    const float polygonCircumRadius = polygonSideLength; // Distancia del centro a un vértice para un hexágono regular
    const int polygonVertexCount = 6;
    
    b2Vec2 inputPolygonVertices[polygonVertexCount];
    for (int i = 0; i < polygonVertexCount; ++i) {
        float angle = 2.0f * M_PI * i / polygonVertexCount;
        inputPolygonVertices[i] = (b2Vec2){polygonCircumRadius * cos(angle), polygonCircumRadius * sin(angle)};
    }

    // Espacio de generación
    const float minX = -3.0f;
    const float maxX = 3.0f;
    const float minY = 0.5f;
    const float maxY = 18.0f;

    // Crear partículas grandes (círculos)
    for (int i = 0; i < numLargeParticles; ++i) {
        float randomX = minX + (maxX - minX) * static_cast<float>(rand()) / RAND_MAX;
        float randomY = minY + (maxY - minY) * static_cast<float>(rand()) / RAND_MAX;

        b2BodyDef particleDef = b2DefaultBodyDef();
        particleDef.type = b2_dynamicBody;
        particleDef.position = (b2Vec2){randomX, randomY};
        b2BodyId particleId = b2CreateBody(worldId, &particleDef);

        b2Circle circle = {};
        circle.radius = particleRadius;

        b2ShapeDef particleShapeDef = b2DefaultShapeDef();
        particleShapeDef.density = 1.0f;
        particleShapeDef.material.friction = 0.6f;
        particleShapeDef.material.restitution = 0.1f;

        b2CreateCircleShape(particleId, &particleShapeDef, &circle);

        b2MassData massData = b2Body_GetMassData(particleId);
        particles.push_back({particleId, CIRCLE, circle.radius, massData.mass});
        particleBodyIds.push_back(particleId);
    }

    // Crear partículas pequeñas (círculos)
    for (int i = 0; i < numSmallParticles; ++i) {
        float randomX = minX + (maxX - minX) * static_cast<float>(rand()) / RAND_MAX;
        float randomY = minY + (maxY - minY) * static_cast<float>(rand()) / RAND_MAX;

        b2BodyDef smallparticleDef = b2DefaultBodyDef();
        smallparticleDef.type = b2_dynamicBody;
        smallparticleDef.position = (b2Vec2){randomX, randomY};
        b2BodyId smallparticleId = b2CreateBody(worldId, &smallparticleDef);
        
        b2Circle circle2 = {};
        circle2.radius = particlesmallRadius;

        b2ShapeDef particleShapeDef = b2DefaultShapeDef();
        particleShapeDef.density = 1.0f;
        particleShapeDef.material.friction = 0.6f;
        particleShapeDef.material.restitution = 0.1f;

        b2CreateCircleShape(smallparticleId, &particleShapeDef, &circle2);

        b2MassData massData = b2Body_GetMassData(smallparticleId);
        particles.push_back({smallparticleId, CIRCLE, circle2.radius, massData.mass});
        particleBodyIds.push_back(smallparticleId);
    }

    // Crear partículas poligonales
    for (int i = 0; i < NUM_POLYGON_PARTICLES; ++i) {
        float randomX = minX + (maxX - minX) * static_cast<float>(rand()) / RAND_MAX;
        float randomY = minY + (maxY - minY) * static_cast<float>(rand()) / RAND_MAX;

        b2BodyDef polygonParticleDef = b2DefaultBodyDef();
        polygonParticleDef.type = b2_dynamicBody;
        polygonParticleDef.position = (b2Vec2){randomX, randomY};
        b2BodyId polygonParticleId = b2CreateBody(worldId, &polygonParticleDef);

        b2ShapeDef polygonShapeDef = b2DefaultShapeDef();
        polygonShapeDef.density = 1.0f;
        polygonShapeDef.material.friction = 0.6f;
        polygonShapeDef.material.restitution = 0.1f;

        b2Hull hull = b2ComputeHull(inputPolygonVertices, polygonVertexCount);
        b2Polygon polygonShape = b2MakePolygon(&hull, 0.005f);
        b2CreatePolygonShape(polygonParticleId, &polygonShapeDef, &polygonShape);

        b2MassData massData = b2Body_GetMassData(polygonParticleId);
        particles.push_back({polygonParticleId, POLYGON, polygonCircumRadius, massData.mass});
        particleBodyIds.push_back(polygonParticleId);
    }

    // Variables de simulación
    int particlesExitedTotalCount = 0;
    float simulationTime = 0.0f;
    lastParticleExitTime = simulationTime;
    blockageStartTime = simulationTime;

    // Bucle principal de simulación
    while (avalancheCount < MAX_AVALANCHES) {
        shockAppliedThisFrame = false; // Resetear flag de golpe
        
        // Aplicar fuerzas aleatorias constantes a todas las partículas
        applyRandomForces(particles);
        
        // Paso de simulación
        b2World_Step(worldId, TIME_STEP, SUB_STEP_COUNT);
        
        // Actualizar tiempo de simulación
        frameCounter++;
        simulationTime = frameCounter * TIME_STEP;
        
        // Guardar estado actual (cada 10 frames para reducir I/O) solo si se debe guardar
        if (SAVE_SIMULATION_DATA && frameCounter % 10 == 0) {
            saveSimulationState(worldId, particles, groundIdleft, groundIdright, leftWallId, rightWallId);
        }
        
        // Manejo de golpes durante bloqueos (ANTES de gestionar partículas)
        if (inBlockage) {
            // Verificar si es momento de aplicar un nuevo golpe
            //if ((simulationTime - lastShockTime >= SHOCK_INTERVAL) &&
            //    (shockAttempts < MAX_SHOCK_ATTEMPTS)) {
                
                // Calcular magnitud del golpe (creciente)
            //    float shockMagnitude = BASE_SHOCK_MAGNITUDE * (1.0f + shockAttempts * 0.1f);
                
                // Aplicar golpe
            //    applyBlockageShock(particles, shockMagnitude);
            //    lastShockTime = simulationTime;
            //    shockAttempts++;
            //    shockAppliedThisFrame = true;
                
                // Debug: informar golpe aplicado
            //    std::cout << "Golpe #" << shockAttempts << " aplicado (magnitud: "
            //              << shockMagnitude << ") en t=" << simulationTime << "s\n";
            //}
            //else if (shockAttempts >= MAX_SHOCK_ATTEMPTS) {
                // Si los golpes no funcionan, detectar y reinyectar partículas del arco
            //    std::cout << "Los golpes no funcionaron. Usando raycast para romper el arco en t=" << simulationTime << "s\n";
                
                // Guardar duración del bloqueo actual
            //    previousBlockageDuration = simulationTime - blockageStartTime;
                
                // Llamada a la función de ray-casting.
            //    detectAndReinjectArchViaRaycast(worldId);
                
                // Forzar inicio de avalancha (asumiendo que la reinyección romperá el atasco)
            //    inBlockage = false;
            //    inAvalanche = true;
            //    avalancheStartTime = simulationTime;
            //    particlesInCurrentAvalanche = 0;
            //    lastExitDuringAvalanche = simulationTime; // Inicializar para evitar duración negativa
            //    shockAttempts = 0;
            //    lastParticleExitTime = simulationTime; // Resetear contador de bloqueo
            //}

            previousBlockageDuration = simulationTime - blockageStartTime;
            detectAndReinjectArchViaRaycast(worldId); // Llamada a la función de ray-casting
            inBlockage = false; // Asumimos que la reinyección rompe el atasco
            inAvalanche = true; // Forzar inicio de avalancha
            avalancheStartTime = simulationTime;
            particlesInCurrentAvalanche = 0; // Reiniciar contador de avalancha
            lastExitDuringAvalanche = simulationTime; // Inicializar para evitar duración negativa
            shockAttempts = 0; // Reiniciar intentos de golpe
            lastParticleExitTime = simulationTime; // Resetear contador de bloqueo
        }
        
        // Gestión de partículas (devuelve número de partículas y masa que salieron)
        auto [exitedCount, exitedMass] = manageParticles(worldId, particleBodyIds,
                                                         simulationTime, lastParticleExitTime, particles);
        
        // Registrar datos de flujo
        recordFlowData(simulationTime, exitedCount, exitedMass);
        
        particlesExitedTotalCount += exitedCount;
        
        // Calcular tiempo desde última salida
        float timeSinceLastExit = simulationTime - lastParticleExitTime;
        
        // LÓGICA PRINCIPAL CORREGIDA - DETECCIÓN DE ESTADOS
        if (exitedCount > 0) {
            // Hay flujo de partículas
            if (inBlockage) {
                // Transición: Bloqueo -> Avalancha
                inBlockage = false;
                inAvalanche = true;
                avalancheStartTime = simulationTime;
                particlesInCurrentAvalanche = exitedCount;
                lastExitDuringAvalanche = simulationTime;  // Registrar primera salida
                
                // Registrar duración del bloqueo anterior
                previousBlockageDuration = simulationTime - blockageStartTime;
                
                // Resetear contador de golpes
                shockAttempts = 0;
            }
            else if (inAvalanche) {
                // Continuación de avalancha existente
                particlesInCurrentAvalanche += exitedCount;
                lastExitDuringAvalanche = simulationTime;  // Actualizar última salida
            }
        }
        else {
            // No hay flujo de partículas
            if (!inBlockage && timeSinceLastExit >= BLOCKAGE_THRESHOLD) {
                // Transición: Avalancha -> Bloqueo
                inBlockage = true;
                inAvalanche = false;
                blockageStartTime = simulationTime - BLOCKAGE_THRESHOLD;
                shockAttempts = 0;
                
                // Calcular duración REAL de la avalancha (hasta última salida)
                float avalancheDuration = lastExitDuringAvalanche - avalancheStartTime;
                
                // Registrar la avalancha que acaba de terminar
                avalancheCount++;
                avalancheDataFile << avalancheCount << " "
                                  << particlesInCurrentAvalanche << " "
                                  << avalancheDuration << " "
                                  << previousBlockageDuration << "\n";
                
                // Actualizar tiempos acumulados
                totalFlowingTime += avalancheDuration;
                totalBlockageTime += previousBlockageDuration;
                
                // Resetear contador de partículas para próxima avalancha
                particlesInCurrentAvalanche = 0;
            }
            // Detectar si un golpe rompió el atasco temporalmente
            else if (shockAppliedThisFrame && exitedCount == 0 && inBlockage) {
                // El golpe aplicado en este frame no rompió el atasco - considerar como continuación
                // No reiniciar el contador de tiempo de atasco
                std::cout << "Golpe aplicado pero no se rompió el atasco en t=" << simulationTime << "s\n";
            }
        }
        
        // Debug: imprimir estado cada 5 segundos
        if (frameCounter % 300 == 0) {
            std::cout << "Tiempo: " << simulationTime << "s | "
                      << "Estado: " << (inAvalanche ? "Avalancha" : (inBlockage ? "Bloqueo" : "Flujo"))
                      << " | Ultima salida hace: " << timeSinceLastExit << "s\n";
            if (inAvalanche) {
                std::cout << "  Particulas en avalancha actual: " << particlesInCurrentAvalanche << "\n";
                std::cout << "  Tiempo ultima salida: " << lastExitDuringAvalanche << "\n";
            }
        }
    }

    // Registrar última avalancha si quedó activa
    if (inAvalanche) {
        float avalancheDuration = lastExitDuringAvalanche - avalancheStartTime;
        avalancheCount++;
        avalancheDataFile << avalancheCount << " "
                          << particlesInCurrentAvalanche << " "
                          << avalancheDuration << " "
                          << previousBlockageDuration << "\n";
        
        totalFlowingTime += avalancheDuration;
    }

    // Registrar último bloqueo si estamos en bloqueo
    if (inBlockage) {
        float currentBlockageDuration = simulationTime - blockageStartTime;
        totalBlockageTime += currentBlockageDuration;
    }
    
    // Calcular tiempo total de simulación
    float totalSimulationTime = simulationTime;
    
    // Verificar consistencia de tiempos
    float timeDiscrepancy = std::abs(totalSimulationTime - (totalFlowingTime + totalBlockageTime));
    
    // Escribir tiempos acumulados al final del archivo de avalanchas
    avalancheDataFile << "\n# ===== RESUMEN FINAL =====\n";
    avalancheDataFile << "# Tiempo total de simulación: " << totalSimulationTime << " s\n";
    avalancheDataFile << "# Tiempo total en avalanchas: " << totalFlowingTime << " s\n";
    avalancheDataFile << "# Tiempo total en atascos: " << totalBlockageTime << " s\n";
    avalancheDataFile << "# Diferencia de tiempo: " << timeDiscrepancy << " s\n";
    avalancheDataFile << "# Suma de estados: " << (totalFlowingTime + totalBlockageTime) << " s\n";
    
    // Registrar cualquier dato de flujo pendiente
    if (accumulatedMass > 0 || accumulatedParticles > 0) {
        recordFlowData(simulationTime, 0, 0);
    }
    
    // Cerrar archivos
    if (SAVE_SIMULATION_DATA) {
        simulationDataFile.close();
    }
    avalancheDataFile.close();
    flowDataFile.close();

    // Limpiar recursos Box2D
    b2DestroyWorld(worldId);

    std::cout << "\n===== SIMULACIÓN COMPLETADA =====\n";
    std::cout << "Frames simulados: " << frameCounter << "\n";
    std::cout << "Avalanchas registradas: " << avalancheCount << "\n";
    std::cout << "Tiempo total de simulación: " << totalSimulationTime << " s\n";
    std::cout << "Tiempo en avalanchas: " << totalFlowingTime << " s\n";
    std::cout << "Tiempo en atascos: " << totalBlockageTime << " s\n";
    std::cout << "Fuerza aleatoria aplicada: " << RANDOM_FORCE_MAX << " N\n";
    std::cout << "Datos guardados en: " << outputDir << "\n";
    std::cout << "Archivo de flujo generado: flow_data.csv\n";

    return 0;
}