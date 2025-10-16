// include/DataHandling.h

#ifndef DATAHANDLING_H
#define DATAHANDLING_H

#include "box2d/box2d.h"
#include "Constants.h"
#include "Initialization.h"

// Datos para raycast (estructuras locales a DataHandling)
struct RaycastUserData {
    std::set<b2BodyId, BodyIdComparator> hitBodies;
    std::vector<std::pair<b2Vec2, b2Vec2>> raySegments;
};

// Declaraciones de funciones
float RaycastCallback(b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context);

void initializeDataFiles();
void finalizeDataFiles(bool simulationInterrupted);

void applyRandomImpulses();
void manageParticles(b2WorldId worldId, float currentTime, float siloHeight,
                     int& exitedTotalCount, float& exitedTotalMass,
                     int& exitedOriginalCount, float& exitedOriginalMass);
void recordFlowData(float currentTime, int exitedTotalCount, float exitedTotalMass,
                    int exitedOriginalCount, float exitedOriginalMass);
void detectAndReinjectArchViaRaycast(b2WorldId worldId, float siloHeight);
void finalizeAvalanche();
void startAvalanche(); 
void startBlockage(); 
void checkFlowStatus(b2WorldId worldId, float timeSinceLastExit);

#endif // DATAHANDLING_H