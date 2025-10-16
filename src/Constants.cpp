// src/Constants.cpp

#include "Constants.h"
#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <ctime>

// =================================================================================================
// 1. CONSTANTES GLOBALES DE LA SIMULACIÓN (DEFINICIÓN CON VALORES POR DEFECTO)
// =================================================================================================

// Parámetros ajustables
float BASE_RADIUS = 0.5f;
float SIZE_RATIO = 0.0f;
float CHI = 0.0f;
int TOTAL_PARTICLES = 2000;
float OUTLET_WIDTH = 3.9f*2*BASE_RADIUS;
float SILO_WIDTH = 20.2f*2*BASE_RADIUS;
float silo_height = 120*2*BASE_RADIUS;

// Variables de conteo y control
int MAX_AVALANCHES = 50; 

// Parámetros de reinyección configurables
float REINJECT_HEIGHT_RATIO = 1.0f;
float REINJECT_HEIGHT_VARIATION = 0.043f;
float REINJECT_WIDTH_RATIO = 0.31f;

// =================================================================================================
// 2. VARIABLES DE ESTADO Y DATOS (DEFINICIÓN CON VALOR INICIAL)
// =================================================================================================

// Parámetros de partículas
int NUM_LARGE_CIRCLES = 0;
int NUM_SMALL_CIRCLES = 0;
int NUM_POLYGON_PARTICLES = 0;
int NUM_SIDES = 5;
float POLYGON_PERIMETER = 0.0f;

// Variables derivadas
float OUTLET_X_HALF_WIDTH = 0.0f;


// Variables de estado y registro
float simulationTime = 0.0f;
float lastPrintTime = 0.0f;
float lastRaycastTime = -0.5f; // Inicializado con -RAYCAST_COOLDOWN
float lastShockTime = 0.0f;
int frameCounter = 0;
bool SAVE_SIMULATION_DATA = false;
int CURRENT_SIMULATION = 1;
int TOTAL_SIMULATIONS = 1;

// Archivos de salida
std::ofstream simulationDataFile;
std::ofstream avalancheDataFile;
std::ofstream flowDataFile;

// Variables de flujo
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

// Variables de registro de flujo
float totalExitedMass = 0.0f;
int totalExitedParticles = 0;
float totalExitedOriginalMass = 0.0f;
int totalExitedOriginalParticles = 0;
float lastRecordedTime = -0.01f;
float accumulatedMass = 0.0f;
int accumulatedParticles = 0;
float accumulatedOriginalMass = 0.0f;
int accumulatedOriginalParticles = 0;

// Variables para seguimiento de progreso real del flujo
int lastTotalExitedCount = 0;
float lastProgressTime = 0.0f;
bool waitingForFlowConfirmation = false;

// RNG Engine y distribuciones
std::mt19937 randomEngine(time(NULL));
std::uniform_real_distribution<> angleDistribution(0.0f, 2.0f * M_PI);
std::uniform_real_distribution<> impulseMagnitudeDistribution(0.0f, 1.0f);

// Set para rastrear partículas que ya salieron en la avalancha actual
std::set<b2BodyId, BodyIdComparator> particlesExitedInCurrentAvalanche;