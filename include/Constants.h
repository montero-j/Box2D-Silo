// include/Constants.h

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <random>
#include <set>
#include <cstdint>
#include <tuple>
#include <algorithm>
#include "box2d/box2d.h"

// =================================================================================================
// 1. CONSTANTES GLOBALES DE LA SIMULACIÓN
// =================================================================================================

// Parámetros ajustables (configurables por línea de comandos)
extern float BASE_RADIUS;
extern float SIZE_RATIO;
extern float CHI;
extern int TOTAL_PARTICLES;
extern float OUTLET_WIDTH;
extern float SILO_WIDTH;
extern float silo_height;

// Constantes físicas y temporales (const)
const float TIME_STEP = 0.0005f; //0.005
const int SUB_STEP_COUNT = 40;
const float BLOCKAGE_THRESHOLD = 5.0f;
const float RECORD_INTERVAL = 0.01f;
const float MIN_AVALANCHE_DURATION = 0.5f;
const float RAYCAST_COOLDOWN = 0.5f;
const float SHOCK_INTERVAL = 0.1f;
const int MAX_BLOCKAGE_RETRIES = 100;
extern int MAX_AVALANCHES; 

// Constantes físicas internas
const float Density = 1.0f;
const int BOX2D_MAX_POLYGON_VERTICES = 8;
const float POLYGON_SKIN_RADIUS = 0.0f;
const float WALL_THICKNESS = 0.1f;
const float GROUND_LEVEL_Y = 0.0f;
const float EXIT_BELOW_Y = -1.5f;

// Parámetros de reinyección configurables
extern float REINJECT_HEIGHT_RATIO;
extern float REINJECT_HEIGHT_VARIATION;
extern float REINJECT_WIDTH_RATIO;

// =================================================================================================
// 2. VARIABLES DE ESTADO Y DATOS GLOBALES (extern)
// =================================================================================================

// Parámetros de partículas
extern int NUM_LARGE_CIRCLES;
extern int NUM_SMALL_CIRCLES;
extern int NUM_POLYGON_PARTICLES;
extern int NUM_SIDES;
extern float POLYGON_PERIMETER;

// Variables derivadas
extern float OUTLET_X_HALF_WIDTH;


// Variables de estado y registro
extern float simulationTime;
extern float lastPrintTime;
extern float lastRaycastTime;
extern float lastShockTime;
extern int frameCounter;
extern bool SAVE_SIMULATION_DATA;
extern int CURRENT_SIMULATION;
extern int TOTAL_SIMULATIONS;

// Archivos de salida
extern std::ofstream simulationDataFile;
extern std::ofstream avalancheDataFile;
extern std::ofstream flowDataFile;

// Variables de flujo
extern int avalancheCount;
extern float totalFlowingTime;
extern float totalBlockageTime;
extern bool inAvalanche;
extern bool inBlockage;
extern float blockageStartTime;
extern float avalancheStartTime;
extern int particlesInCurrentAvalanche;
extern int avalancheStartParticleCount;
extern float lastExitDuringAvalanche;
extern float lastParticleExitTime;
extern float previousBlockageDuration;
extern int blockageRetryCount;

// Variables de registro de flujo
extern float totalExitedMass;
extern int totalExitedParticles;
extern float totalExitedOriginalMass;
extern int totalExitedOriginalParticles;
extern float lastRecordedTime;
extern float accumulatedMass;
extern int accumulatedParticles;
extern float accumulatedOriginalMass;
extern int accumulatedOriginalParticles;

// Variables para seguimiento de progreso real del flujo
extern int lastTotalExitedCount;
extern float lastProgressTime;
extern bool waitingForFlowConfirmation;

// RNG Engine y distribuciones
extern std::mt19937 randomEngine;
extern std::uniform_real_distribution<> angleDistribution;
extern std::uniform_real_distribution<> impulseMagnitudeDistribution;

// Comparador para b2BodyId en std::set
struct BodyIdComparator {
    bool operator()(const b2BodyId& a, const b2BodyId& b) const {
        return a.index1 < b.index1;
    }
};

extern std::set<b2BodyId, BodyIdComparator> particlesExitedInCurrentAvalanche;

#endif // CONSTANTS_H