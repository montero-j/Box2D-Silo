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
#include <cstdint> // Para b2BodyId, etc.
#include <tuple>   // Para std::tuple

#include "box2d/box2d.h"

// --- Constantes de simulación (GLOBALES) ---
const float TIME_STEP = 0.001f; // Reducido a 1 ms como en el artículo
const int SUB_STEP_COUNT = 4;
const float BLOCKAGE_THRESHOLD = 5.0f; // Se mantiene, ya coincide
const float SHOCK_INTERVAL = 0.1f; // Frecuencia de los "kicks" (cada 0.1 s)
const float RANDOM_FORCE_MAGNITUDE = 5.0e-5f; // MAGNITUD MÁXIMA del IMPULSO (5e-5 Ns como en el artículo)
const float OUTLET_X_HALF_WIDTH = 2.3f * 0.065f; // 2.3 * r0, donde r0 es BASE_RADIUS
const float RECORD_INTERVAL = 0.01f; // Intervalo para registrar datos de flujo

// --- Parámetros ajustables (GLOBALES, con valores por defecto del artículo) ---
float BASE_RADIUS = 0.065f; // 6.5 cm como en el artículo
float SIZE_RATIO = 0.4f; // Valor de r=0.4 para empezar a explorar
float CHI = 0.2f; // Valor de chi=0.2 para empezar a explorar
int TOTAL_PARTICLES = 250; // Total de partículas fijado en 250
int NUM_POLYGON_PARTICLES = 0; // El artículo usa solo círculos

// --- Variables de estado de simulación (GLOBALES) ---
float simulationTime = 0.0f;
float lastPrintTime = 0.0f; // Para imprimir debug
int frameCounter = 0;

// --- Variables para guardado de datos (GLOBALES) ---
std::ofstream simulationDataFile;
std::ofstream avalancheDataFile;
std::ofstream flowDataFile;
bool SAVE_SIMULATION_DATA = false;
int CURRENT_SIMULATION = 1; // Para seguimiento de simulación actual
int TOTAL_SIMULATIONS = 1; // Para seguimiento del total de simulaciones

// --- Variables para detección de avalanchas/bloqueos (GLOBALES) ---
int avalancheCount = 0;
float totalFlowingTime = 0.0f;
float totalBlockageTime = 0.0f;
bool inAvalanche = false;
bool inBlockage = false;
float blockageStartTime = 0.0f;
float avalancheStartTime = 0.0f;
int particlesInCurrentAvalanche = 0;
float lastExitDuringAvalanche = 0.0f;
float lastParticleExitTime = 0.0f;
float previousBlockageDuration = 0.0f; // Duración del bloqueo previo

// --- Variables para registro de flujo (GLOBALES) ---
float totalExitedMass = 0.0f;
int totalExitedParticles = 0;
float totalExitedOriginalMass = 0.0f;
int totalExitedOriginalParticles = 0;
float lastRecordedTime = -0.01f; // Inicializar para que la primera grabación sea en 0
float accumulatedMass = 0.0f;
int accumulatedParticles = 0;
float accumulatedOriginalMass = 0.0f;
int accumulatedOriginalParticles = 0;

// --- Variables para el sistema de impulsos (GLOBALES) ---
float lastShockTime = 0.0f;
std::mt19937 randomEngine(time(NULL));
std::uniform_real_distribution<> angleDistribution(0.0f, 2.0f * M_PI);
std::uniform_real_distribution<> impulseMagnitudeDistribution(0.0f, RANDOM_FORCE_MAGNITUDE);

// --- Estructuras de datos para partículas (GLOBALES) ---
enum ParticleShapeType { CIRCLE, POLYGON };

struct ParticleInfo {
    b2BodyId bodyId;
    ParticleShapeType shapeType;
    float size;
    float mass;
    bool isOriginal; // true si es partícula original (grande), false si es añadida (pequeña)
};

std::vector<ParticleInfo> particles;
std::vector<b2BodyId> particleBodyIds;

// --- Custom Comparator for b2BodyId for std::set ---
// b2BodyId no tiene operator< por defecto, por lo que se necesita un comparador para usarlo en std::set
struct BodyIdComparator {
    bool operator()(const b2BodyId& a, const b2BodyId& b) const {
        // En Box2D C API, b2BodyId es típicamente un struct con un miembro 'index' o 'index1' de tipo uint32_t.
        // La comparación debe realizarse sobre ese índice.
        // Basado en el error anterior ('has no member named 'index'; did you mean 'index1'?'),
        // utilizamos 'index1'.
        return a.index1 < b.index1;
    }
};

// --- Callback para Raycast (GLOBAL) ---
struct RaycastUserData {
    std::set<b2BodyId, BodyIdComparator> hitBodies; // Usa el comparador personalizado
};

float RaycastCallback(b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context) {
    b2BodyId bodyId = b2Shape_GetBody(shapeId);

    if (b2Body_GetType(bodyId) == b2_dynamicBody) {
        RaycastUserData* data = static_cast<RaycastUserData*>(context);
        data->hitBodies.insert(bodyId);
    }
    // Retorna 1.0f para que el raycast continúe a través de todos los cuerpos en su camino.
    return 1.0f;
}

// --- Declaraciones de funciones (para que main pueda llamarlas) ---
void applyRandomImpulses(std::vector<ParticleInfo>& particles);
std::tuple<int, float, int, float> manageParticles(b2WorldId worldId, std::vector<b2BodyId>& particleIds,
                                                   float currentTime, float& lastExitTimeRef,
                                                   const std::vector<ParticleInfo>& particles);
void recordFlowData(float currentTime, int exitedTotalCount, float exitedTotalMass,
                    int exitedOriginalCount, float exitedOriginalMass);
void detectAndReinjectArchViaRaycast(b2WorldId worldId);


// --- Implementaciones de funciones auxiliares ---

// Función para aplicar impulsos aleatorios
void applyRandomImpulses(std::vector<ParticleInfo>& particles) {
    if (simulationTime - lastShockTime >= SHOCK_INTERVAL) {
        for (const auto& particle : particles) {
            float magnitude = impulseMagnitudeDistribution(randomEngine);
            float angle = angleDistribution(randomEngine);
            b2Vec2 impulse = {
                magnitude * cos(angle),
                magnitude * sin(angle)
            };
            b2Body_ApplyLinearImpulseToCenter(particle.bodyId, impulse, true);
        }
        lastShockTime = simulationTime;
    }
}

// Función para gestionar partículas que salen del silo
std::tuple<int, float, int, float> manageParticles(b2WorldId worldId, std::vector<b2BodyId>& particleIds,
                                                   float currentTime, float& lastExitTimeRef,
                                                   const std::vector<ParticleInfo>& particles) {

    // Las coordenadas de reinyección coinciden con las especificadas para el silo de fondo plano
    const float EXIT_BELOW_Y = -1.5f; // Punto de salida inferior
    const float EXIT_LEFT_X = -5.0f; // Punto de salida lateral (amplio para evitar problemas)
    const float EXIT_RIGHT_X = 5.0f; // Punto de salida lateral

    // Coordenadas de reinyección en la parte superior del silo
    const float REINJECT_MIN_X = -0.4f; // Dentro del ancho del silo
    const float REINJECT_MAX_X = 0.4f;
    const float REINJECT_MIN_Y = 6.0f; // Altura superior del silo (6.36m, un poco más bajo para inyectar)
    const float REINJECT_MAX_Y = 6.2f;

    int exitedTotalCount = 0;
    float exitedTotalMass = 0.0f;
    int exitedOriginalCount = 0;
    float exitedOriginalMass = 0.0f;

    for (size_t i = 0; i < particleIds.size(); ++i) {
        b2BodyId particleId = particleIds[i];
        b2Vec2 pos = b2Body_GetPosition(particleId);

        if (pos.y < EXIT_BELOW_Y || pos.x < EXIT_LEFT_X || pos.x > EXIT_RIGHT_X) {
            float randomX = REINJECT_MIN_X + (REINJECT_MAX_X - REINJECT_MIN_X) * static_cast<float>(rand()) / RAND_MAX;
            float randomY = REINJECT_MIN_Y + (REINJECT_MAX_Y - REINJECT_MIN_Y) * static_cast<float>(rand()) / RAND_MAX;

            // Corregido: usar inicialización directa de b2Rot para la rotación nula
            b2Body_SetTransform(particleId, (b2Vec2){randomX, randomY}, (b2Rot){0.0f, 1.0f});
            b2Body_SetLinearVelocity(particleId, (b2Vec2){0.0f, 0.0f});
            b2Body_SetAngularVelocity(particleId, 0.0f);

            exitedTotalCount++;
            exitedTotalMass += particles[i].mass;
            lastExitTimeRef = currentTime;

            if (particles[i].isOriginal) {
                exitedOriginalCount++;
                exitedOriginalMass += particles[i].mass;
            }
        }
    }

    return {exitedTotalCount, exitedTotalMass, exitedOriginalCount, exitedOriginalMass};
}

// Función para registrar datos de flujo
void recordFlowData(float currentTime, int exitedTotalCount, float exitedTotalMass,
                    int exitedOriginalCount, float exitedOriginalMass) {
    accumulatedMass += exitedTotalMass;
    accumulatedParticles += exitedTotalCount;
    // Corregido: Usar 'exitedOriginalCount' en lugar de 'exitedOriginalParticles'
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
                     << totalExitedMass << ","
                     << massFlowRate << ","
                     << totalExitedParticles << ","
                     << particleFlowRate << ","
                     << totalExitedOriginalMass << ","
                     << originalMassFlowRate << ","
                     << totalExitedOriginalParticles << ","
                     << originalParticleFlowRate << "\n";

        accumulatedMass = 0.0f;
        accumulatedParticles = 0;
        accumulatedOriginalMass = 0.0f;
        accumulatedOriginalParticles = 0;
        lastRecordedTime = currentTime;
    }
}

// Función para detectar y reinyectar arcos vía raycast
void detectAndReinjectArchViaRaycast(b2WorldId worldId) {
    const float REINJECT_MIN_X = -0.4f;
    const float REINJECT_MAX_X = 0.4f;
    const float REINJECT_MIN_Y = 6.0f;
    const float REINJECT_MAX_Y = 6.2f;

    RaycastUserData raycastData;

    const int numRays = 25;
    const float maxAngle = M_PI / 3.0f; // 60 grados a cada lado (total 120 grados)
    const float rayLength = 2.0f; // Longitud del rayo
    const b2Vec2 origin = {0.0f, 0.0f}; // Origen de los rayos, justo en el centro de la abertura del silo

    for (int i = 0; i < numRays; ++i) {
        float angle = -maxAngle + (2.0f * maxAngle * i) / (numRays - 1);
        b2Vec2 translation = {rayLength * sin(angle), rayLength * cos(angle)};
        b2World_CastRay(worldId, origin, translation, b2DefaultQueryFilter(), RaycastCallback, &raycastData);
    }

    if (!raycastData.hitBodies.empty()) {
        std::cout << "Raycast detectó " << raycastData.hitBodies.size() << " partículas en el arco. Reinyectando...\n";

        for (b2BodyId bodyToReinject : raycastData.hitBodies) {
            float randomX = REINJECT_MIN_X + (REINJECT_MAX_X - REINJECT_MIN_X) * static_cast<float>(rand()) / RAND_MAX;
            float randomY = REINJECT_MIN_Y + (REINJECT_MAX_Y - REINJECT_MIN_Y) * static_cast<float>(rand()) / RAND_MAX;

            b2Rot currentRotation = b2Body_GetRotation(bodyToReinject);
            b2Body_SetTransform(bodyToReinject, {randomX, randomY}, currentRotation);
            b2Body_SetLinearVelocity(bodyToReinject, {0.0f, 0.0f});
            b2Body_SetAngularVelocity(bodyToReinject, 0.0f);
        }
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
        else if (strcmp(argv[i], "--total-particles") == 0 && i + 1 < argc) {
            TOTAL_PARTICLES = std::stoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--polygon-particles") == 0 && i + 1 < argc) {
            NUM_POLYGON_PARTICLES = std::stoi(argv[++i]); // Esto debería ser 0 por defecto
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

    // Solo guardar datos de simulación detallados en las primeras 5 simulaciones
    if (CURRENT_SIMULATION > 5) {
        SAVE_SIMULATION_DATA = false;
    }

    srand(time(NULL));

    // Calcular parámetros de partículas basados en constantes
    const float particleRadius = BASE_RADIUS;
    const float particlesmallRadius = BASE_RADIUS * SIZE_RATIO;

    const int numSmallParticles = static_cast<int>(CHI * TOTAL_PARTICLES);
    const int numLargeParticles = TOTAL_PARTICLES - numSmallParticles;

    // Crear directorio de resultados
    std::string outputDir = "results/"; // Directorio por defecto para resultados finales
    if (SAVE_SIMULATION_DATA) {
        outputDir = "./sim_data/"; // Directorio para datos detallados de simulación
    } else {
        outputDir = "./"; // Directorio actual para el caso de no guardar datos detallados
    }
    std::filesystem::create_directories(outputDir);

    // Abrir archivos de salida
    if (SAVE_SIMULATION_DATA) {
        simulationDataFile.open(outputDir + "simulation_data.csv");
    }
    avalancheDataFile.open(outputDir + "avalanche_data.csv");
    flowDataFile.open(outputDir + "flow_data.csv");

    // Imprimir parámetros al inicio del log
    std::cout << "Inicio de simulación granular\n";
    std::cout << "Radio grande (r_0): " << particleRadius << " m\n";
    std::cout << "Radio pequeño (r_a): " << particlesmallRadius << " m (Razón: " << SIZE_RATIO << ")\n";
    std::cout << "Fracción partículas pequeñas (χ): " << CHI << "\n";
    std::cout << "Número partículas grandes (Originales): " << numLargeParticles << "\n";
    std::cout << "Número partículas pequeñas (Añadidas): " << numSmallParticles << "\n";
    std::cout << "Total partículas: " << (numLargeParticles + numSmallParticles) << "\n";
    std::cout << "Abertura del silo: " << (OUTLET_X_HALF_WIDTH * 2) << " m (" << (OUTLET_X_HALF_WIDTH * 2 / (2 * particleRadius)) << " diámetros grandes)\n";
    std::cout << "Duración de simulación: 150 segundos\n";
    std::cout << "Magnitud máxima de impulso aleatorio: " << RANDOM_FORCE_MAGNITUDE << " Ns\n";
    std::cout << "Current Sim: " << CURRENT_SIMULATION << " / " << TOTAL_SIMULATIONS << "\n";
    std::cout << "Save Sim Data: " << (SAVE_SIMULATION_DATA ? "Yes" : "No") << "\n";

    // Encabezado para flow_data.csv
    flowDataFile << "Time,MassTotal,MassFlowRate,NoPTotal,NoPFlowRate,MassOriginalTotal,MassOriginalFlowRate,NoPOriginalTotal,NoPOriginalFlowRate\n";

    // Configurar mundo Box2D
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = (b2Vec2){0.0f, -10.0f}; // Gravedad estándar
    b2WorldId worldId = b2CreateWorld(&worldDef);

    // Definición de la forma de las paredes
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.material.friction = 0.5f; // Fricción de las paredes
    shapeDef.material.restitution = 0.1f; // Coeficiente de restitución de las paredes

    // --- Crear estructuras del silo: Fondo Plano ---
    const float silo_height = 6.36f; // Altura del silo (como en el artículo)
    const float silo_width = 1.0f;   // Ancho del silo (como en el artículo)
    const float wall_thickness = 0.1f; // Grosor de las paredes

    const float ground_level_y = 0.0f; // El fondo estará en y = 0

    // 1. Pared izquierda (vertical)
    b2BodyDef leftWallDef = b2DefaultBodyDef();
    // La pared debe centrarse en Y en su punto medio. Si su base es ground_level_y,
    // su centro Y es ground_level_y + (silo_height / 2.0f).
    // Su posición X es el borde izquierdo del silo - la mitad del grosor de la pared.
    leftWallDef.position = (b2Vec2){-(silo_width / 2.0f) - (wall_thickness / 2.0f), ground_level_y + (silo_height / 2.0f)};
    b2BodyId leftWallId = b2CreateBody(worldId, &leftWallDef);
    b2Body_SetType(leftWallId, b2_staticBody);
    // b2MakeBox toma MEDIO ANCHO y MEDIO ALTO.
    // Ancho real de la pared: wall_thickness. Medio ancho: wall_thickness / 2.0f.
    // Alto real de la pared: silo_height. Medio alto: silo_height / 2.0f.
    b2Polygon leftWallShape = b2MakeBox(wall_thickness / 2.0f, silo_height / 2.0f);
    b2CreatePolygonShape(leftWallId, &shapeDef, &leftWallShape);

    // 2. Pared derecha (vertical)
    b2BodyDef rightWallDef = b2DefaultBodyDef();
    // Similar a la izquierda, posición X es el borde derecho del silo + la mitad del grosor de la pared.
    rightWallDef.position = (b2Vec2){(silo_width / 2.0f) + (wall_thickness / 2.0f), ground_level_y + (silo_height / 2.0f)};
    b2BodyId rightWallId = b2CreateBody(worldId, &rightWallDef);
    b2Body_SetType(rightWallId, b2_staticBody);
    // b2MakeBox toma MEDIO ANCHO y MEDIO ALTO.
    b2Polygon rightWallShape = b2MakeBox(wall_thickness / 2.0f, silo_height / 2.0f);
    b2CreatePolygonShape(rightWallId, &shapeDef, &rightWallShape);

    // 3. Fondo izquierdo (parte del suelo a la izquierda de la abertura)
    b2BodyDef groundLeftDef = b2DefaultBodyDef();
    // El centro X de esta sección es el punto medio entre -silo_width/2 y -OUTLET_X_HALF_WIDTH.
    // Es decir: ((-silo_width / 2.0f) + (-OUTLET_X_HALF_WIDTH)) / 2.0f
    // El centro Y de esta sección es ground_level_y - (wall_thickness / 2.0f) para que su parte superior esté en ground_level_y.
    groundLeftDef.position = (b2Vec2){(-silo_width / 2.0f + -OUTLET_X_HALF_WIDTH) / 2.0f, ground_level_y - (wall_thickness / 2.0f)};
    b2BodyId groundLeftId = b2CreateBody(worldId, &groundLeftDef);
    b2Body_SetType(groundLeftId, b2_staticBody);
    // *** CORRECCIÓN CRÍTICA AQUÍ ***:
    // El ancho REAL de esta sección es (silo_width / 2.0f - OUTLET_X_HALF_WIDTH).
    // b2MakeBox necesita el MEDIO ANCHO, por lo que se divide entre 2.0f.
    // El alto REAL es wall_thickness. b2MakeBox necesita el MEDIO ALTO.
    b2Polygon groundLeftShape = b2MakeBox((silo_width / 2.0f - OUTLET_X_HALF_WIDTH) / 2.0f, wall_thickness / 2.0f);
    b2CreatePolygonShape(groundLeftId, &shapeDef, &groundLeftShape);

    // 4. Fondo derecho (parte del suelo a la derecha de la abertura)
    b2BodyDef groundRightDef = b2DefaultBodyDef();
    // El centro X de esta sección es el punto medio entre OUTLET_X_HALF_WIDTH y silo_width/2.
    // Es decir: (OUTLET_X_HALF_WIDTH + silo_width / 2.0f) / 2.0f
    // El centro Y es ground_level_y - (wall_thickness / 2.0f).
    groundRightDef.position = (b2Vec2){(OUTLET_X_HALF_WIDTH + silo_width / 2.0f) / 2.0f, ground_level_y - (wall_thickness / 2.0f)};
    b2BodyId groundRightId = b2CreateBody(worldId, &groundRightDef);
    b2Body_SetType(groundRightId, b2_staticBody);
    // *** CORRECCIÓN CRÍTICA AQUÍ ***:
    // Ancho REAL: (silo_width / 2.0f - OUTLET_X_HALF_WIDTH). Medio Ancho: / 2.0f.
    // Alto REAL: wall_thickness. Medio Alto: / 2.0f.
    b2Polygon groundRightShape = b2MakeBox((silo_width / 2.0f - OUTLET_X_HALF_WIDTH) / 2.0f, wall_thickness / 2.0f);
    b2CreatePolygonShape(groundRightId, &shapeDef, &groundRightShape);

    // Las llamadas a SetTransform al final del bloque de creación de la base
    // están redundantes si la posición ya se estableció en b2BodyDef,
    // pero si se mantienen, asegúrate de que usen las mismas posiciones calculadas.
    // Sin embargo, recomiendo eliminarlas y confiar en la definición de b2BodyDef.
    /*
    b2Body_SetTransform(groundLeftId, (b2Vec2){(-silo_width / 2.0f + OUTLET_X_HALF_WIDTH) / 2.0f, ground_level_y - (wall_thickness / 2.0f)}, (b2Rot){0.0f, 1.0f});
    b2Body_SetTransform(groundRightId, (b2Vec2){(OUTLET_X_HALF_WIDTH + silo_width / 2.0f) / 2.0f, ground_level_y - (wall_thickness / 2.0f)}, (b2Rot){0.0f, 1.0f});
    */

    // Crear partículas (rangos de generación ajustados para silo de fondo plano)
    // Se generan dentro del ancho del silo, y por encima del nivel del suelo pero con margen
    const float minX_gen = -silo_width / 2.0f + BASE_RADIUS; // Mínimo X para generación
    const float maxX_gen = silo_width / 2.0f - BASE_RADIUS;  // Máximo X para generación
    const float minY_gen = ground_level_y + BASE_RADIUS * 2.0f; // Asegurarse de que no se generen dentro del suelo
    const float maxY_gen = silo_height - BASE_RADIUS * 2.0f; // Hasta casi el tope del silo

    // Crear partículas grandes (círculos) - "Originales"
    for (int i = 0; i < numLargeParticles; ++i) {
        float randomX = minX_gen + (maxX_gen - minX_gen) * static_cast<float>(rand()) / RAND_MAX;
        float randomY = minY_gen + (maxY_gen - minY_gen) * static_cast<float>(rand()) / RAND_MAX;

        b2BodyDef particleDef = b2DefaultBodyDef();
        particleDef.type = b2_dynamicBody;
        particleDef.position = (b2Vec2){randomX, randomY};
        b2BodyId particleId = b2CreateBody(worldId, &particleDef);

        b2Circle circle = {};
        circle.radius = particleRadius;

        b2ShapeDef particleShapeDef = b2DefaultShapeDef();
        particleShapeDef.density = 1.0f; // Densidad del material
        particleShapeDef.material.friction = 0.5f; // Fricción del artículo
        particleShapeDef.material.restitution = 0.1f; // Restitución del artículo

        b2CreateCircleShape(particleId, &particleShapeDef, &circle);

        b2MassData massData = b2Body_GetMassData(particleId);
        particles.push_back({particleId, CIRCLE, circle.radius, massData.mass, true});
        particleBodyIds.push_back(particleId);
    }

    // Crear partículas pequeñas (círculos) - "Añadidas"
    for (int i = 0; i < numSmallParticles; ++i) {
        float randomX = minX_gen + (maxX_gen - minX_gen) * static_cast<float>(rand()) / RAND_MAX;
        float randomY = minY_gen + (maxY_gen - minY_gen) * static_cast<float>(rand()) / RAND_MAX;

        b2BodyDef smallparticleDef = b2DefaultBodyDef();
        smallparticleDef.type = b2_dynamicBody;
        smallparticleDef.position = (b2Vec2){randomX, randomY};
        b2BodyId smallparticleId = b2CreateBody(worldId, &smallparticleDef);

        b2Circle circle2 = {};
        circle2.radius = particlesmallRadius;

        b2ShapeDef particleShapeDef = b2DefaultShapeDef();
        particleShapeDef.density = 1.0f;
        particleShapeDef.material.friction = 0.5f;
        particleShapeDef.material.restitution = 0.1f;

        b2CreateCircleShape(smallparticleId, &particleShapeDef, &circle2);

        b2MassData massData = b2Body_GetMassData(smallparticleId);
        particles.push_back({smallparticleId, CIRCLE, circle2.radius, massData.mass, false});
        particleBodyIds.push_back(smallparticleId);
    }
    // NOTA: Se ha eliminado la generación de partículas poligonales
    // ya que el artículo se centra en círculos y para simplificar el código.
    // Si necesitas polígonos, deberás reintroducir esa lógica y definir
    // las variables como inputPolygonVertices, polygonVertexCount, polygonCircumRadius.

    // Bucle principal de simulación
    while (simulationTime < 150.0f) { // Se mantiene la duración de 150s
        // Aplicar impulsos aleatorios (NO fuerzas constantes)
        applyRandomImpulses(particles); // Llama a la función de impulsos

        // Paso de simulación Box2D
        b2World_Step(worldId, TIME_STEP, SUB_STEP_COUNT);
        simulationTime += TIME_STEP;
        frameCounter++;

        // Manejo de golpes durante bloqueos (Adaptar si es necesario)
        // La lógica de `inBlockage` y `detectAndReinjectArchViaRaycast` se mantiene,
        // ya que el artículo también "rompe" los atascos para seguir el flujo.
        if (inBlockage) {
            previousBlockageDuration = simulationTime - blockageStartTime;
            detectAndReinjectArchViaRaycast(worldId); // Reinyecta partículas que forman el arco
            inBlockage = false;
            inAvalanche = true;
            avalancheStartTime = simulationTime;
            particlesInCurrentAvalanche = 0;
            lastExitDuringAvalanche = simulationTime;
            lastParticleExitTime = simulationTime;
        }

        // Gestión de partículas (Actualizar llamada con nuevos retornos)
        auto [exitedTotalCount, exitedTotalMass, exitedOriginalCount, exitedOriginalMass] =
            manageParticles(worldId, particleBodyIds, simulationTime, lastParticleExitTime, particles);

        // Registrar datos de flujo (Actualizar llamada con nuevos parámetros)
        recordFlowData(simulationTime, exitedTotalCount, exitedTotalMass, exitedOriginalCount, exitedOriginalMass);

        // Lógica de detección de estados (se mantiene)
        float timeSinceLastExit = simulationTime - lastParticleExitTime;

        if (inAvalanche) {
            if (timeSinceLastExit > BLOCKAGE_THRESHOLD) {
                // La avalancha ha terminado y hay un bloqueo
                float currentAvalancheDuration = simulationTime - avalancheStartTime - BLOCKAGE_THRESHOLD;
                totalFlowingTime += currentAvalancheDuration;
                avalancheDataFile << "Avalancha " << (avalancheCount + 1) << ","
                                  << avalancheStartTime << ","
                                  << (simulationTime - BLOCKAGE_THRESHOLD) << ","
                                  << currentAvalancheDuration << ","
                                  << particlesInCurrentAvalanche << "\n";
                avalancheCount++;
                inAvalanche = false;
                inBlockage = true;
                blockageStartTime = simulationTime - BLOCKAGE_THRESHOLD;
            } else if (exitedTotalCount > 0) { // Si las partículas han salido en este paso
                particlesInCurrentAvalanche += exitedTotalCount;
                lastExitDuringAvalanche = simulationTime;
            }
        } else if (inBlockage) {
            if (exitedTotalCount > 0) {
                // El bloqueo se ha roto, comienza una nueva avalancha
                totalBlockageTime += (simulationTime - blockageStartTime);
                inBlockage = false;
                inAvalanche = true;
                avalancheStartTime = simulationTime;
                particlesInCurrentAvalanche = exitedTotalCount;
                lastExitDuringAvalanche = simulationTime;
            }
        } else {
            // Ni en avalancha ni en bloqueo, estado inicial o de reposo.
            if (exitedTotalCount > 0) {
                inAvalanche = true;
                avalancheStartTime = simulationTime;
                particlesInCurrentAvalanche = exitedTotalCount;
                lastExitDuringAvalanche = simulationTime;
            } else if (timeSinceLastExit > BLOCKAGE_THRESHOLD && simulationTime > BLOCKAGE_THRESHOLD) {
                // Si no hay flujo y el tiempo de simulación es suficiente, asumir bloqueo
                inBlockage = true;
                blockageStartTime = simulationTime - BLOCKAGE_THRESHOLD;
            }
        }

        // Debug: imprimir estado cada 5 segundos
        if (simulationTime - lastPrintTime >= 5.0f || simulationTime >= 150.0f - TIME_STEP) {
            std::cout << "Tiempo: " << std::fixed << std::setprecision(2) << simulationTime
                      << "s, Partículas Salientes: " << totalExitedParticles
                      << ", Masa Saliente: " << totalExitedMass
                      << ", Originales Salientes: " << totalExitedOriginalParticles
                      << "\n";
            lastPrintTime = simulationTime;
        }

        // Si se guarda la simulación, escribir la posición de las partículas en cada frame
        if (SAVE_SIMULATION_DATA) {
            simulationDataFile << simulationTime;
            for (const auto& particle : particles) {
                b2Vec2 pos = b2Body_GetPosition(particle.bodyId);
                simulationDataFile << "," << pos.x << "," << pos.y;
            }
            simulationDataFile << "\n";
        }
    }

    // --- Resumen Final y Cierre de Archivos ---

    // Registrar última avalancha si quedó activa
    if (inAvalanche) {
        float currentAvalancheDuration = simulationTime - avalancheStartTime;
        totalFlowingTime += currentAvalancheDuration;
        avalancheDataFile << "Avalancha " << (avalancheCount + 1) << ","
                          << avalancheStartTime << ","
                          << simulationTime << ","
                          << currentAvalancheDuration << ","
                          << particlesInCurrentAvalanche << "\n";
        avalancheCount++;
    }

    // Registrar último bloqueo si estamos en bloqueo
    if (inBlockage) {
        totalBlockageTime += (simulationTime - blockageStartTime);
    }

    // Calcular la discrepancia de tiempo
    float totalSimulationTime = simulationTime;
    float timeDiscrepancy = totalSimulationTime - (totalFlowingTime + totalBlockageTime);

    // Escribir tiempos acumulados al final del archivo de avalanchas
    avalancheDataFile << "\n===== RESUMEN FINAL =====\n";
    avalancheDataFile << "# Tiempo total de simulación: " << totalSimulationTime << " s\n";
    avalancheDataFile << "# Tiempo total en avalanchas: " << totalFlowingTime << " s\n";
    avalancheDataFile << "# Tiempo total en atascos: " << totalBlockageTime << " s\n";
    avalancheDataFile << "# Diferencia de tiempo: " << timeDiscrepancy << " s\n";
    avalancheDataFile << "# Suma de estados: " << (totalFlowingTime + totalBlockageTime) << " s\n";

    // Registrar cualquier dato de flujo pendiente antes de cerrar
    if (accumulatedMass > 0 || accumulatedParticles > 0 || accumulatedOriginalMass > 0 || accumulatedOriginalParticles > 0) {
        recordFlowData(simulationTime, 0, 0, 0, 0); // Pasar 0 para que solo vuelque lo acumulado
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
    std::cout << "Partículas totales salientes: " << totalExitedParticles << "\n";
    std::cout << "Masa total de partículas salientes: " << totalExitedMass << "\n";
    std::cout << "Partículas originales totales salientes: " << totalExitedOriginalParticles << "\n";
    std::cout << "Masa total de partículas originales salientes: " << totalExitedOriginalMass << "\n";
    std::cout << "Datos de flujo (incluyendo originales) en: " << outputDir << "flow_data.csv\n";


    return 0;
}