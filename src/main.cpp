// src/main.cpp

// INCLUSIONES ESTÁNDAR DE C++ (Completas)
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

// INCLUSIONES DE TUS MÓDULOS
#include "Constants.h"
#include "Initialization.h"
#include "DataHandling.h"

// =========================================================
// FUNCIÓN PRINCIPAL (Lógica del bucle principal extraída de silo-simulation.cpp)
// =========================================================

int main(int argc, char** argv) {
    // ID del bloque de salida temporal
    b2BodyId tempOutletBlockId = b2_nullBodyId;
    
    // Bandera de interrupción
    bool simulationInterrupted = false;

    // 1. Manejo de Argumentos y Validación
    if (!parseAndValidateArgs(argc, argv)) {
        return 1;
    }

    // 2. Cálculo de Parámetros Derivados 
    calculateDerivedParameters();
    
    // 3. Inicialización de Archivos de Datos 
    initializeDataFiles();

    // 4. Impresión de parámetros iniciales
    const float largeCircleRadius = BASE_RADIUS;
    const float smallCircleRadius = BASE_RADIUS * SIZE_RATIO;
    
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

    // 5. Bucle de Simulaciones 
    for (CURRENT_SIMULATION = 1; CURRENT_SIMULATION <= TOTAL_SIMULATIONS; ++CURRENT_SIMULATION) {
        
        // Reinicialización de variables de estado (omito para simplicidad, en un bucle real esto sería necesario)
        
        // 6. Inicialización del Mundo, Muros y Bloqueo
        worldId = createWorldAndWalls(tempOutletBlockId);

        // 7. Creación de Partículas
        createParticles(worldId);

        // 8. Sedimentación
        runSedimentation(worldId);

        // 9. Apertura del Silo
        std::cout << "\nABRIENDO SILO - Eliminando bloqueo temporal\n";
        b2DestroyBody(tempOutletBlockId);
        std::cout << "SILO ABIERTO - Iniciando simulación de flujo granular\n\n";

        // Reiniciar tiempo después de sedimentación
        simulationTime = 0.0f;
        
        // Variables locales del bucle principal
        int exitedTotalCount = 0;
        float exitedTotalMass = 0.0f;
        int exitedOriginalCount = 0;
        float exitedOriginalMass = 0.0f;
        float timeSinceLastExit = 0.0f;
        
        // 10. BUCLE PRINCIPAL DE SIMULACIÓN
        while (avalancheCount < MAX_AVALANCHES && !simulationInterrupted) {
            
            // Pasos de la simulación
            b2World_Step(worldId, TIME_STEP, SUB_STEP_COUNT);
            simulationTime += TIME_STEP;
            frameCounter++;

            // Aplicar impulsos aleatorios
            //applyRandomImpulses(); 

            // Manejar partículas que salen
            manageParticles(worldId, simulationTime, silo_height, 
                            exitedTotalCount, exitedTotalMass, 
                            exitedOriginalCount, exitedOriginalMass);
            
            // Registrar datos de flujo (acumula y escribe periódicamente)
            recordFlowData(simulationTime, exitedTotalCount, exitedTotalMass, 
                           exitedOriginalCount, exitedOriginalMass);

            timeSinceLastExit = simulationTime - lastParticleExitTime;
            
            // Lógica de control de flujo, avalancha y atasco
            checkFlowStatus(worldId, timeSinceLastExit);

            // Verificar interrupción por atasco persistente 
            if (inBlockage && blockageRetryCount > MAX_BLOCKAGE_RETRIES) {
                 simulationInterrupted = true;
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
        
        // 11. Finalización de Archivos y Mundo (global)
        finalizeDataFiles(simulationInterrupted);
        b2DestroyWorld(worldId);
    }
    
    return 0;
}