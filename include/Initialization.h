// include/Initialization.h

#ifndef INITIALIZATION_H
#define INITIALIZATION_H

#include "box2d/box2d.h"
#include <vector>
#include <string>
#include "Constants.h"

// Variables de Box2D Globales
extern b2WorldId worldId;

// Estructuras de datos específicas de partículas
enum ParticleShapeType { CIRCLE, POLYGON };

struct ParticleInfo {
    b2BodyId bodyId;
    ParticleShapeType shapeType;
    float size;
    float mass;
    bool isOriginal;
    int numSides;
};

// Contenedores globales
extern std::vector<ParticleInfo> particles;
extern std::vector<b2BodyId> particleBodyIds;

// Declaraciones de funciones
bool parseAndValidateArgs(int argc, char** argv);
bool calculateDerivedParameters();
b2WorldId createWorldAndWalls(b2BodyId& outletBlockIdRef);
void createParticles(b2WorldId worldId);
void runSedimentation(b2WorldId worldId);

#endif // INITIALIZATION_H