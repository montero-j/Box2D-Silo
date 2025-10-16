#ifndef FLOW_CONTROL_H
#define FLOW_CONTROL_H

#include "Constants.h"
#include "DataHandling.h"
#include <random>

// =================================================================================================
// 1. FUNCIONES DE MANEJO DE FLUJO
// =================================================================================================

/**
 * Maneja las partículas que han salido del silo (las cuenta y las reinyecta).
 * @param worldId El ID del mundo Box2D.
 * @param particleIds Vector de IDs de los cuerpos de las partículas.
 * @param siloHeight Altura máxima del silo para la reinyección.
 */
void manageParticles(b2WorldId worldId);

/**
 * Callback usado por Box2D para raycast. Registra los cuerpos dinámicos golpeados.
 * @return La fracción del rayo para continuar la búsqueda.
 */
float RaycastCallback(b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context);

/**
 * Detecta y rompe un arco de atasco inyectando energía a las partículas golpeadas por raycast.
 * @param worldId El ID del mundo Box2D.
 * @param siloHeight Altura máxima del silo para la reinyección.
 */
void detectAndReinjectArchViaRaycast(b2WorldId worldId);

#endif // FLOW_CONTROL_H
