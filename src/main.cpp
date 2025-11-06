// src/main.cpp

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

// Headers del proyecto
#include "Constants.h"
#include "Initialization.h"
#include "DataHandling.h"

// ==== Definiciones globales necesarias por otros .cpp ====
// (Asegúrate de que en tus headers estén como `extern`)
#include <box2d/box2d.h>

// Coincidir exactamente con Initialization.h:
b2WorldId worldId = b2_nullWorldId;
std::vector<ParticleInfo>  particles;       // ⟵ corregido: ParticleInfo
std::vector<b2BodyId>      particleBodyIds;

// === Control de frecuencias de pasos ===
// Se pueden sobrescribir por CLI:
//   --exit-check-every <N>
//   --save-frame-every <M>
int EXIT_CHECK_EVERY_STEPS = 10;
int SAVE_FRAME_EVERY_STEPS = 100;


// =========================================================
// FUNCIÓN PRINCIPAL
// =========================================================
int main(int argc, char** argv) {
    // Bloque temporal para tapar la salida durante sedimentación
    b2BodyId tempOutletBlockId = b2_nullBodyId;

    bool simulationInterrupted = false;

    // 1) Argumentos
    if (!parseAndValidateArgs(argc, argv)) {
        return 1;
    }

    // 2) Parámetros derivados
    calculateDerivedParameters();

    // 3) Archivos de salida
    initializeDataFiles();

    // 4) Impresión de parámetros iniciales
    const float largeCircleRadius = BASE_RADIUS;
    const float smallCircleRadius = BASE_RADIUS * SIZE_RATIO;

    std::cout << "=== INICIO SIMULACIÓN GRANULAR ===\n";
    std::cout << "Radio base (r0): " << BASE_RADIUS << " m\n";
    std::cout << "Razón de tamaño (r): " << SIZE_RATIO << "\n";
    std::cout << "Chi (fracción chicas): " << CHI << "\n";
    std::cout << "Partículas circulares grandes: " << NUM_LARGE_CIRCLES
              << " (R=" << largeCircleRadius << ")\n";
    std::cout << "Partículas circulares pequeñas: " << NUM_SMALL_CIRCLES
              << " (R=" << smallCircleRadius << ")\n";
    std::cout << "Partículas poligonales: " << NUM_POLYGON_PARTICLES
              << " (Lados=" << NUM_SIDES << ", Perímetro=" << POLYGON_PERIMETER << ")\n";
    std::cout << "Total de partículas: " << TOTAL_PARTICLES << "\n";
    std::cout << "Ancho silo: " << SILO_WIDTH << " m\n";
    std::cout << "Altura silo: " << silo_height << " m\n";
    std::cout << "Abertura silo: " << OUTLET_WIDTH
              << " m (" << (OUTLET_WIDTH / (2.f * BASE_RADIUS)) << " diámetros base)\n";
    std::cout << "Máx. avalanchas: " << MAX_AVALANCHES << "\n";
    std::cout << "EXIT_CHECK_EVERY_STEPS = " << EXIT_CHECK_EVERY_STEPS << "\n";
    std::cout << "SAVE_FRAME_EVERY_STEPS = " << SAVE_FRAME_EVERY_STEPS << "\n";
    std::cout << "Simulaciones: " << TOTAL_SIMULATIONS << "\n\n";

    // 5) Bucle de simulaciones
    for (CURRENT_SIMULATION = 1; CURRENT_SIMULATION <= TOTAL_SIMULATIONS; ++CURRENT_SIMULATION) {
        std::cout << "\n--- SIMULACIÓN " << CURRENT_SIMULATION << " / "
                  << TOTAL_SIMULATIONS << " ---\n";

        // 6) Mundo/Geometría
        worldId = createWorldAndWalls(tempOutletBlockId);

        // 7) Partículas
        createParticles(worldId);

        // 8) Sedimentación (con salida tapada)
        // runSedimentation(worldId);

        // tras createParticles(worldId);
        bool settled = runSedimentation(worldId);

        // Si NO estabilizó, forzar extensión hasta estabilizar (sin abrir aún)
        if (!settled) {
            std::cout << "Extendiendo sedimentación hasta cumplir criterio...\n";
            // pequeño helper in-line (no hace falta declarar función aparte)
            auto isStable = [&](void)->bool{
                // reutiliza exactamente el MISMO criterio que adentro de runSedimentation
                // para no duplicar lógica; acá solo hacemos un “check rápido”
                const float KE_ABS_PER_PART_EPS    = 1e-3f;
                const float V_SLOW_EPS             = 0.05f;
                const float W_SLOW_EPS             = 0.2f;
                const float SLOW_FRACTION_REQUIRED = 0.95f;

                float Rmax = BASE_RADIUS;
                if (SIZE_RATIO > 0.f) Rmax = std::max(Rmax, BASE_RADIUS * SIZE_RATIO);
                if (NUM_POLYGON_PARTICLES > 0 && NUM_SIDES >= 3) {
                    float ns = std::max(3, NUM_SIDES);
                    float polyR = POLYGON_PERIMETER / (2.0f * ns * std::sin(float(M_PI)/ns));
                    Rmax = std::max(Rmax, polyR);
                }
                const float pad = Rmax + 0.01f;
                const float bandTop    = GROUND_LEVEL_Y + silo_height - pad;
                const float yCeiling   = bandTop - 2.0f*Rmax;

                float KE = 0.0f; int slow=0; float yMax=-1e30f;
                for (const auto& p : particles) {
                    b2Vec2 v = b2Body_GetLinearVelocity(p.bodyId);
                    float w  = b2Body_GetAngularVelocity(p.bodyId);
                    KE += 0.5f * p.mass * (v.x*v.x + v.y*v.y);
                    if (std::sqrt(v.x*v.x + v.y*v.y) < V_SLOW_EPS && std::fabs(w) < W_SLOW_EPS) ++slow;
                    b2Vec2 pos = b2Body_GetPosition(p.bodyId);
                    if (pos.y > yMax) yMax = pos.y;
                }
                float KEp = (TOTAL_PARTICLES>0)? KE/TOTAL_PARTICLES : 0.f;
                float slowFrac = (TOTAL_PARTICLES>0)? float(slow)/TOTAL_PARTICLES : 1.f;
                bool bandClear = (yMax <= yCeiling);
                return (KEp < KE_ABS_PER_PART_EPS && slowFrac >= SLOW_FRACTION_REQUIRED && bandClear);
            };

            // Esperar hasta que isStable() sea true (sin límite, o con un cap adicional si querés)
            int guardIters = 0, guardMax = 120000; // ~ 120000 * TIME_STEP si TIME_STEP=1/240 -> 500 s máx
            while (!isStable() && guardIters++ < guardMax) {
                b2World_Step(worldId, TIME_STEP, SUB_STEP_COUNT);
            }
            if (guardIters >= guardMax) {
                std::cout << "Advertencia: guardMax alcanzado; abro igual.\n";
            } else {
                std::cout << "Listo: condición de sedimentación lograda antes de abrir.\n";
            }
        }


        // 9) Apertura del silo
        std::cout << "ABRIENDO SILO: eliminando bloqueo temporal...\n";
        b2DestroyBody(tempOutletBlockId);
        std::cout << "SILO ABIERTO: iniciando fase de flujo.\n";

        // Reiniciar tiempo y contadores tras sedimentación
        simulationTime = 0.0f;

        int   exitedTotalCount    = 0;
        float exitedTotalMass     = 0.0f;
        int   exitedOriginalCount = 0;
        float exitedOriginalMass  = 0.0f;
        float timeSinceLastExit   = 0.0f;

        // 10) Bucle principal
        while (avalancheCount < MAX_AVALANCHES && !simulationInterrupted) {
            // Integración
            b2World_Step(worldId, TIME_STEP, SUB_STEP_COUNT);
            simulationTime += TIME_STEP;
            frameCounter++;

            // Manejo de partículas salientes (cada N pasos)
            if (frameCounter % EXIT_CHECK_EVERY_STEPS == 0) {
                manageParticles(
                    worldId, simulationTime, silo_height,
                    exitedTotalCount, exitedTotalMass,
                    exitedOriginalCount, exitedOriginalMass
                );
            }

            // Registro de flujo
            recordFlowData(
                simulationTime,
                exitedTotalCount, exitedTotalMass,
                exitedOriginalCount, exitedOriginalMass
            );

            // Estado (avalanchas / bloqueos)
            timeSinceLastExit = simulationTime - lastParticleExitTime;
            checkFlowStatus(worldId, timeSinceLastExit);

            // Interrupción por bloqueo persistente
            if (inBlockage && blockageRetryCount > MAX_BLOCKAGE_RETRIES) {
                simulationInterrupted = true;
            }

            // Log periódico (~cada 5s sim)
            if (simulationTime - lastPrintTime >= 5.0f) {
                std::string state = inAvalanche ? "AVALANCHA" : (inBlockage ? "BLOQUEO" : "INICIAL");
                std::cout << "t=" << std::fixed << std::setprecision(2) << simulationTime
                          << "s | salieron=" << totalExitedParticles
                          << " | avalanchas=" << avalancheCount << "/" << MAX_AVALANCHES
                          << " | estado=" << state << "\n";
                lastPrintTime = simulationTime;
            }

            // Guardado de "frames" de partículas cada M pasos
            if (SAVE_SIMULATION_DATA && (frameCounter % SAVE_FRAME_EVERY_STEPS == 0)) {
                simulationDataFile << std::fixed << std::setprecision(5) << simulationTime;
                for (const auto& p : particles) {     // p es ParticleInfo
                    b2Vec2 pos   = b2Body_GetPosition(p.bodyId);
                    float  angle = b2Rot_GetAngle(b2Body_GetRotation(p.bodyId));
                    simulationDataFile << ","
                                       << pos.x << "," << pos.y << ","
                                       << p.shapeType << ","
                                       << p.size << ","
                                       << p.numSides << ","
                                       << angle;
                }
                simulationDataFile << "\n";
            }
        } // while

        // 11) Cierre de archivos y destrucción del mundo
        finalizeDataFiles(simulationInterrupted);
        b2DestroyWorld(worldId);
    } // for

    std::cout << "\n=== FIN DE TODAS LAS SIMULACIONES ===\n";
    return 0;
}
