// src/Initialization.cpp
#include "Initialization.h"
#include "Constants.h"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cstring>



#include <unordered_map>
#include <random>
#include <cmath>


// Frecuencias configurables definidas en main.cpp
extern int EXIT_CHECK_EVERY_STEPS;
extern int SAVE_FRAME_EVERY_STEPS;

static void printUsage() {
    std::cout << "Uso: silo_simulator [opciones]\n";
    std::cout << "Opciones principales:\n";
    std::cout << "  --base-radius <val>        Radio base de partículas\n";
    std::cout << "  --size-ratio <val>         Razón de tamaño (r)\n";
    std::cout << "  --chi <val>                Fracción de partículas pequeñas (χ)\n";
    std::cout << "  --total-particles <N>      Total de partículas\n";
    std::cout << "  --num-large-circles <N>    Cant. discos grandes\n";
    std::cout << "  --num-small-circles <N>    Cant. discos chicos\n";
    std::cout << "  --num-polygon-particles <N> Cant. partículas poligonales\n";
    std::cout << "  --num-sides <N>            Lados de los polígonos\n";
    std::cout << "  --current-sim <i>          Índice de simulación actual\n";
    std::cout << "  --total-sims <N>           Cantidad total de simulaciones\n";
    std::cout << "  --save-sim-data <0|1>      Guardar simulation_data.csv\n";
    std::cout << "  --silo-height <val>        Altura del silo\n";
    std::cout << "  --silo-width <val>         Ancho del silo\n";
    std::cout << "  --outlet-width <val>       Abertura del silo\n";
    std::cout << "  --exit-check-every <N>     Verifica salida de partículas cada N pasos (default 10)\n";
    std::cout << "  --save-frame-every <M>     Guarda frames cada M pasos (default 100)\n";
}

bool parseAndValidateArgs(int argc, char** argv) {
    if (argc <= 1) {
        return true;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);

        if (arg == "--help" || arg == "-h") {
            printUsage();
            return false;
        }
        else if (arg == "--base-radius" && i + 1 < argc) {
            BASE_RADIUS = std::stof(argv[++i]);
        }
        else if (arg == "--size-ratio" && i + 1 < argc) {
            SIZE_RATIO = std::stof(argv[++i]);
        }
        else if (arg == "--chi" && i + 1 < argc) {
            CHI = std::stof(argv[++i]);
        }
        else if (arg == "--total-particles" && i + 1 < argc) {
            TOTAL_PARTICLES = std::stoi(argv[++i]);
        }
        else if (arg == "--num-large-circles" && i + 1 < argc) {
            NUM_LARGE_CIRCLES = std::stoi(argv[++i]);
        }
        else if (arg == "--num-small-circles" && i + 1 < argc) {
            NUM_SMALL_CIRCLES = std::stoi(argv[++i]);
        }
        else if (arg == "--num-polygon-particles" && i + 1 < argc) {
            NUM_POLYGON_PARTICLES = std::stoi(argv[++i]);
        }
        else if (arg == "--num-sides" && i + 1 < argc) {
            NUM_SIDES = std::stoi(argv[++i]);
        }
        else if (arg == "--current-sim" && i + 1 < argc) {
            CURRENT_SIMULATION = std::stoi(argv[++i]);
        }
        else if (arg == "--total-sims" && i + 1 < argc) {
            TOTAL_SIMULATIONS = std::stoi(argv[++i]);
        }
        else if (arg == "--save-sim-data" && i + 1 < argc) {
            SAVE_SIMULATION_DATA = (std::stoi(argv[++i]) != 0);
        }
        else if (arg == "--silo-height" && i + 1 < argc) {
            silo_height = std::stof(argv[++i]);
        }
        else if (arg == "--silo-width" && i + 1 < argc) {
            SILO_WIDTH = std::stof(argv[++i]);
        }
        else if (arg == "--outlet-width" && i + 1 < argc) {
            OUTLET_WIDTH = std::stof(argv[++i]);
        }
        // === NUEVOS FLAGS ===
        else if (arg == "--exit-check-every" && i + 1 < argc) {
            EXIT_CHECK_EVERY_STEPS = std::max(1, std::stoi(argv[++i]));
        }
        else if (arg == "--save-frame-every" && i + 1 < argc) {
            SAVE_FRAME_EVERY_STEPS = std::max(1, std::stoi(argv[++i]));
        }
        else {
            std::cerr << "Argumento desconocido: " << arg << "\n";
            printUsage();
            return false;
        }
    }

    return true;
}

bool calculateDerivedParameters() {

    OUTLET_X_HALF_WIDTH = OUTLET_WIDTH / 2.0f;

    if (TOTAL_PARTICLES > 0) {

        bool usePolygonsAsLargeComponent = (NUM_SIDES > 0 || NUM_POLYGON_PARTICLES > 0);
        int N_ref = TOTAL_PARTICLES;

        NUM_LARGE_CIRCLES = 0;
        NUM_SMALL_CIRCLES = 0;
        NUM_POLYGON_PARTICLES = 0;

        if (SIZE_RATIO < 1e-3) {

            if (usePolygonsAsLargeComponent) {
                NUM_POLYGON_PARTICLES = N_ref;
            } else {
                NUM_LARGE_CIRCLES = N_ref;
            }
            NUM_SMALL_CIRCLES = 0;
        }
        else {
            // K = Razón de Masa = (M_large / M_small) = 1 / (SIZE_RATIO)^2
            float K_mass_ratio = 1.0f / (SIZE_RATIO * SIZE_RATIO);

            // 1. Calcular N_L (Número de partículas grandes)
            float N_L_float = CHI * (float)N_ref;
            // Redondear al entero más cercano
            int N_L = static_cast<int>(std::round(N_L_float));
            // Asegurar que N_L está dentro de los límites [0, N_ref]
            N_L = std::max(0, std::min(N_ref, N_L));

            // 2. Calcular N_S (Número de partículas pequeñas)
            // N_S = K * (N_ref - N_L)
            int N_L_final = N_L;
            float N_S_float = K_mass_ratio * (float)(N_ref - N_L_final);
            int N_S = static_cast<int>(std::round(N_S_float));

            // 3. Asignar y actualizar el número total de partículas
            TOTAL_PARTICLES = N_L + N_S;

            if (usePolygonsAsLargeComponent) {
                NUM_POLYGON_PARTICLES = N_L;
                NUM_LARGE_CIRCLES = 0;
                NUM_SMALL_CIRCLES = N_S;
            } else {
                NUM_LARGE_CIRCLES = N_L;
                NUM_POLYGON_PARTICLES = 0;
                NUM_SMALL_CIRCLES = N_S;
            }
        }
    } else {

        TOTAL_PARTICLES = NUM_LARGE_CIRCLES + NUM_SMALL_CIRCLES + NUM_POLYGON_PARTICLES;
    }


    if (NUM_POLYGON_PARTICLES > 0 && POLYGON_PERIMETER == 0.0f) {
        if (NUM_SIDES < 3) {
            std::cerr << "Error: Un polígono debe tener al menos 3 lados (NUM_SIDES).\n";
            return false;
        }

        // Área deseada = Área del Círculo Grande (Radio = BASE_RADIUS)
        const float desired_area = M_PI * BASE_RADIUS * BASE_RADIUS;



        float tan_pi_over_N = tanf(M_PI / (float)NUM_SIDES);

        // Se calcula el cuadrado del lado (side length squared)
        float s_squared = (4.0f * desired_area * tan_pi_over_N) / (float)NUM_SIDES;

        // s = sqrt(s_squared)
        float side_length = std::sqrt(s_squared);

        // Perímetro = N * s
        POLYGON_PERIMETER = (float)NUM_SIDES * side_length;

        std::cout << "Calculando Perímetro del Polígono (" << NUM_SIDES << " lados) para igualar el Área del Círculo (R=" << BASE_RADIUS << "): " << POLYGON_PERIMETER << "\n";
    }
    // =========================================================================


    if (CURRENT_SIMULATION > 10) {
        SAVE_SIMULATION_DATA = false;
    }

    return true;
}

b2WorldId createWorldAndWalls(b2BodyId& outletBlockIdRef) {

    // Configurar mundo Box2D
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = (b2Vec2){0.0f, -9.81f};
    b2WorldId newWorldId = b2CreateWorld(&worldDef);

    worldId = newWorldId;

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.filter.categoryBits = 0x0001;
    shapeDef.filter.maskBits = 0xFFFF;
    shapeDef.material.friction = 0.5f;
    shapeDef.material.restitution = 0.9f;

    // Crear estructuras del silo
    const float wall_thickness = WALL_THICKNESS;
    const float ground_level_y = GROUND_LEVEL_Y;

    // Pared izquierda
    b2BodyDef leftWallDef = b2DefaultBodyDef();
    leftWallDef.position = (b2Vec2){-(SILO_WIDTH / 2.0f) - (wall_thickness / 2.0f), ground_level_y + (silo_height / 2.0f)};
    b2BodyId leftWallId = b2CreateBody(worldId, &leftWallDef);
    b2Body_SetType(leftWallId, b2_staticBody);
    b2Polygon leftWallShape = b2MakeBox(wall_thickness / 2.0f, silo_height / 2.0f);
    b2CreatePolygonShape(leftWallId, &shapeDef, &leftWallShape);

    // Pared derecha
    b2BodyDef rightWallDef = b2DefaultBodyDef();
    rightWallDef.position = (b2Vec2){(SILO_WIDTH / 2.0f) + (wall_thickness / 2.0f), ground_level_y + (silo_height / 2.0f)};
    b2BodyId rightWallId = b2CreateBody(worldId, &rightWallDef);
    b2Body_SetType(rightWallId, b2_staticBody);
    b2Polygon rightWallShape = b2MakeBox(wall_thickness / 2.0f, silo_height / 2.0f);
    b2CreatePolygonShape(rightWallId, &shapeDef, &rightWallShape);

    // Fondo izquierdo
    b2BodyDef groundLeftDef = b2DefaultBodyDef();
    groundLeftDef.position = (b2Vec2){(-SILO_WIDTH / 2.0f + -OUTLET_X_HALF_WIDTH) / 2.0f, ground_level_y - (wall_thickness / 2.0f)};
    b2BodyId groundLeftId = b2CreateBody(worldId, &groundLeftDef);
    b2Body_SetType(groundLeftId, b2_staticBody);
    b2Polygon groundLeftShape = b2MakeBox((SILO_WIDTH / 2.0f - OUTLET_X_HALF_WIDTH) / 2.0f, wall_thickness / 2.0f);
    b2CreatePolygonShape(groundLeftId, &shapeDef, &groundLeftShape);

    // Fondo derecho
    b2BodyDef groundRightDef = b2DefaultBodyDef();
    groundRightDef.position = (b2Vec2){(OUTLET_X_HALF_WIDTH + SILO_WIDTH / 2.0f) / 2.0f, ground_level_y - (wall_thickness / 2.0f)};
    b2BodyId groundRightId = b2CreateBody(worldId, &groundRightDef);
    b2Body_SetType(groundRightId, b2_staticBody);
    b2Polygon groundRightShape = b2MakeBox((SILO_WIDTH / 2.0f - OUTLET_X_HALF_WIDTH) / 2.0f, wall_thickness / 2.0f);
    b2CreatePolygonShape(groundRightId, &shapeDef, &groundRightShape);

    // OUTLET TEMPORAL BLOQUEADO
    b2BodyDef outletBlockDef = b2DefaultBodyDef();
    outletBlockDef.position = (b2Vec2){0.0f, ground_level_y - (wall_thickness / 2.0f)};
    b2BodyId tempOutletBlockId = b2CreateBody(worldId, &outletBlockDef);
    b2Body_SetType(tempOutletBlockId, b2_staticBody);
    b2Polygon outletBlockShape = b2MakeBox(OUTLET_X_HALF_WIDTH, wall_thickness / 2.0f);
    b2CreatePolygonShape(tempOutletBlockId, &shapeDef, &outletBlockShape);

    outletBlockIdRef = tempOutletBlockId;

    return newWorldId;
}


static void settleUntilStable(b2WorldId worldId,
                              float maxTime = 1.5f,
                              float checkInterval = 0.5f,
                              float dt = 1.0f/240.0f,
                              int subSteps = 4,
                              float stabilityThreshold = 0.1f,
                              int requiredChecks = 2)
{
    float t = 0.0f, lastCheck = 0.0f;
    float prevKE = 1e9f;
    int ok = 0;

    while (t < maxTime && ok < requiredChecks) {
        b2World_Step(worldId, dt, subSteps);
        t += dt;

        if (t - lastCheck >= checkInterval) {
            float KE = 0.0f;
            for (const auto& p : particles) {
                b2Vec2 v = b2Body_GetLinearVelocity(p.bodyId);
                KE += 0.5f * p.mass * (v.x*v.x + v.y*v.y);
            }
            float dE = std::fabs(KE - prevKE);
            ok = (dE < stabilityThreshold) ? ok+1 : 0;
            prevKE = KE;
            lastCheck = t;
        }
    }
}



void createParticles(b2WorldId worldId) {

    // =================== Parámetros de generación por tandas ===================
    const int   GEN_BATCH_SIZE    = 250;         // cantidad por tanda
    const int   GEN_RELAX_STEPS   = 240;         // steps de relax entre tandas
    const float GEN_RELAX_DT      = 1.0f / 240;  // dt de cada step
    const int   GEN_SUB_STEPS     = 4;           // substeps por step (ajustar según tu build)

    // Banda de spawneo (estrecha, cerca del techo) para que caigan
    const float SPAWN_BAND_HEIGHT = 2.5f * BASE_RADIUS; // altura de la banda superior

    // (Si tu firma de b2World_Step difiere, adaptá abajo la llamada.)
    auto relaxWorld = [&](int nSteps){
        for (int s = 0; s < nSteps; ++s){
            b2World_Step(worldId, GEN_RELAX_DT, GEN_SUB_STEPS);
        }
    };
    // ==========================================================================

    particles.clear();
    particleBodyIds.clear();

    const float largeCircleRadius = BASE_RADIUS;
    const float smallCircleRadius = BASE_RADIUS * SIZE_RATIO;

    // --------- util local: 2D, SAT, grilla ----------
    struct Vec2 {
        float x, y;
        Vec2() : x(0), y(0) {}
        Vec2(float X, float Y) : x(X), y(Y) {}
        Vec2 operator+(const Vec2& o) const { return {x+o.x, y+o.y}; }
        Vec2 operator-(const Vec2& o) const { return {x-o.x, y-o.y}; }
    };
    auto dot = [](const Vec2& a, const Vec2& b){ return a.x*b.x + a.y*b.y; };
    auto length2 = [&](const Vec2& v){ return dot(v,v); };

    struct Poly {
        std::vector<Vec2> local; // CCW
        float R = 0.f;           // circunradio
    };
    auto make_regular_ngon = [&](int n, float R){
        Poly p; p.local.resize(n); p.R = R;
        for (int i=0;i<n;++i){
            float a = 2.0f*(float)M_PI*i/n;
            p.local[i] = {R*std::cos(a), R*std::sin(a)};
        }
        return p;
    };
    auto poly_from_circumradius = [&](int sides, float R){
        if (sides < 3) sides = 3;
        return make_regular_ngon(sides, R);
    };
    auto worldVerts = [&](const Poly& P, const Vec2& pos, float ang){
        float c = std::cos(ang), s = std::sin(ang);
        std::vector<Vec2> out; out.reserve(P.local.size());
        for (auto &v : P.local){
            Vec2 r{ c*v.x - s*v.y, s*v.x + c*v.y };
            out.push_back(r + pos);
        }
        return out;
    };

    // === SAT (ejes normalizados + EPS) ===
    auto projectOnAxisNorm = [&](const std::vector<Vec2>& V, const Vec2& axisNorm,
                                 float& minP, float& maxP){
        float p0 = dot(V[0], axisNorm);
        minP = maxP = p0;
        for (size_t i=1;i<V.size();++i){
            float p = dot(V[i], axisNorm);
            if (p < minP) minP = p;
            if (p > maxP) maxP = p;
        }
    };

    auto overlapSAT = [&](const std::vector<Vec2>& A, const std::vector<Vec2>& B)->bool{
        const float EPS = 1e-5f;
        auto testAxes = [&](const std::vector<Vec2>& V)->bool{
            for (size_t i=0;i<V.size();++i){
                Vec2 p0 = V[i];
                Vec2 p1 = V[(i+1)%V.size()];
                Vec2 e{p1.x - p0.x, p1.y - p0.y};
                float len = std::sqrt(e.x*e.x + e.y*e.y);
                if (len <= 0.0f) continue;
                Vec2 axisNorm{-e.y/len, e.x/len};
                float minA, maxA, minB, maxB;
                projectOnAxisNorm(A, axisNorm, minA, maxA);
                projectOnAxisNorm(B, axisNorm, minB, maxB);
                if (maxA <= minB + EPS || maxB <= minA + EPS) return false;
            }
            return true;
        };
        return testAxes(A) && testAxes(B);
    };

    struct CellKey { int cx, cy; };
    struct CellKeyHash {
        size_t operator()(const CellKey& k) const noexcept {
            return std::hash<long long>()(((long long)k.cx<<32) ^ (long long)(k.cy));
        }
    };
    struct CellKeyEq {
        bool operator()(const CellKey& a, const CellKey& b) const noexcept {
            return a.cx==b.cx && a.cy==b.cy;
        }
    };
    auto cellOf = [&](const Vec2& p, float cellSize){
        int cx = (int)std::floor(p.x / cellSize);
        int cy = (int)std::floor(p.y / cellSize);
        return CellKey{cx, cy};
    };

    // --------- catálogo de formas ----------
    const int N_DISK = 20; // disco como 20-gon para precolocación

    float polyCircumRadius = 0.0f;
    if (NUM_POLYGON_PARTICLES > 0) {
        int ns = std::max(3, NUM_SIDES);
        polyCircumRadius = POLYGON_PERIMETER / (2.0f * ns * std::sin((float)M_PI / ns));
    }

    std::vector<Poly> catalog;
    int CAT_LARGE_DISK = -1, CAT_SMALL_DISK = -1, CAT_POLY = -1;

    if (NUM_LARGE_CIRCLES > 0) {
        CAT_LARGE_DISK = (int)catalog.size();
        catalog.push_back(make_regular_ngon(N_DISK, largeCircleRadius));
    }
    if (NUM_SMALL_CIRCLES > 0) {
        CAT_SMALL_DISK = (int)catalog.size();
        catalog.push_back(make_regular_ngon(N_DISK, smallCircleRadius));
    }
    if (NUM_POLYGON_PARTICLES > 0) {
        CAT_POLY = (int)catalog.size();
        catalog.push_back(poly_from_circumradius(std::max(3, NUM_SIDES), polyCircumRadius));
    }

    if ((int)catalog.size() == 0 && TOTAL_PARTICLES == 0) {
        std::cout << "Sin partículas para crear.\n";
        return;
    }

    // Radio máximo para grilla
    float maxR = 0.f;
    for (auto &p : catalog) maxR = std::max(maxR, p.R);
    if (maxR <= 0.f) {
        maxR = std::max(largeCircleRadius, std::max(smallCircleRadius, polyCircumRadius));
    }
    const float cellSize  = std::max(2.0f*maxR, 1e-4f);
    const float clearance = 1.02f;

    // --------- pool de tipos a crear ----------
    std::vector<ParticleShapeType> particleTypesToCreate;
    particleTypesToCreate.reserve(TOTAL_PARTICLES);
    for (int i = 0; i < NUM_LARGE_CIRCLES;    ++i) particleTypesToCreate.push_back(CIRCLE);
    for (int i = 0; i < NUM_SMALL_CIRCLES;    ++i) particleTypesToCreate.push_back(CIRCLE);
    for (int i = 0; i < NUM_POLYGON_PARTICLES;++i) particleTypesToCreate.push_back(POLYGON);
    std::shuffle(particleTypesToCreate.begin(), particleTypesToCreate.end(), randomEngine);

    // Caja global disponible
    float Rmax_place = std::max(largeCircleRadius, std::max(smallCircleRadius, polyCircumRadius));
    const float pad = Rmax_place + 0.01f;
    const float minX_gen = -SILO_WIDTH / 2.0f + pad;
    const float maxX_gen =  SILO_WIDTH / 2.0f - pad;

    // Banda superior para spawnear (más angosta en Y)
    const float bandTop    = GROUND_LEVEL_Y + silo_height - pad;
    const float bandBottom = std::max(GROUND_LEVEL_Y + pad, bandTop - SPAWN_BAND_HEIGHT);

    std::uniform_real_distribution<float> Ux(minX_gen, maxX_gen);
    std::uniform_real_distribution<float> Uy(bandBottom, bandTop);
    std::uniform_real_distribution<float> Uang(0.f, 2.f*(float)M_PI);

    // Mapeo tipo -> índice de catálogo y circunradio
    auto getCatalogIndexAndR = [&](ParticleShapeType t, bool isLargeCircle)->std::pair<int,float>{
        if (t == CIRCLE) {
            if (isLargeCircle) return {CAT_LARGE_DISK, largeCircleRadius};
            else               return {CAT_SMALL_DISK, smallCircleRadius};
        } else {
            return {CAT_POLY, polyCircumRadius};
        }
    };

    struct Placed { Vec2 pos; float ang; ParticleShapeType t; bool large; };

    std::vector<Placed> placed; placed.reserve(TOTAL_PARTICLES);
    std::unordered_map<CellKey, std::vector<int>, CellKeyHash, CellKeyEq> grid;

    const int MAX_ATTEMPTS = 2000;

    int nLargeRemaining = NUM_LARGE_CIRCLES;
    int nSmallRemaining = NUM_SMALL_CIRCLES;

    // =================== BUCLE POR TANDAS ===================
    for (int base = 0; base < TOTAL_PARTICLES; base += GEN_BATCH_SIZE) {
        int batchEnd = std::min(base + GEN_BATCH_SIZE, TOTAL_PARTICLES);

        for (int i = base; i < batchEnd; ++i) {
            ParticleShapeType t = particleTypesToCreate[i];
            bool isLarge = false;
            if (t == CIRCLE) {
                if (nLargeRemaining > 0) { isLarge = true;  --nLargeRemaining; }
                else                     { isLarge = false; --nSmallRemaining; }
            }

            auto [catIdx, Rcur] = getCatalogIndexAndR(t, isLarge);
            if (catIdx < 0) { // fallback
                catIdx = CAT_SMALL_DISK;
                Rcur = smallCircleRadius;
                t = CIRCLE; isLarge = false;
            }
            const Poly& P = catalog[catIdx];

            bool ok = false;
            int attempts = 0;
            Vec2 pos;
            float ang = Uang(randomEngine); // también para círculos (20-gon del SAT)

            while (!ok && attempts < MAX_ATTEMPTS) {
                ++attempts;

                // Elegimos dentro de la banda superior para que caigan
                pos = {Ux(randomEngine), Uy(randomEngine)};
                if (t != CIRCLE) ang = Uang(randomEngine);

                // broad-phase con grilla (considera TODO lo ya colocado en tandas previas)
                bool overlaps = false;
                CellKey c = cellOf(pos, cellSize);
                for (int dy=-1; dy<=1 && !overlaps; ++dy){
                    for (int dx=-1; dx<=1 && !overlaps; ++dx){
                        CellKey nb{c.cx+dx, c.cy+dy};
                        auto it = grid.find(nb);
                        if (it == grid.end()) continue;
                        for (int idx : it->second){
                            const Placed& Q = placed[idx];
                            auto [catIdxQ, RQ] = getCatalogIndexAndR(Q.t, Q.large);
                            const Poly& PQ = catalog[catIdxQ];

                            float Rsum = clearance*(P.R + PQ.R);
                            Vec2 d{pos.x - Q.pos.x, pos.y - Q.pos.y};
                            if (length2(d) > Rsum*Rsum) continue;

                            auto A = worldVerts(P, pos, ang);
                            auto B = worldVerts(PQ, Q.pos, Q.ang);
                            if (overlapSAT(A, B)) { overlaps = true; break; }
                        }
                    }
                }

                if (!overlaps) {
                    int newIndex = (int)placed.size();
                    placed.push_back({pos, ang, t, isLarge});
                    grid[c].push_back(newIndex);

                    // ========= Crear cuerpo inmediatamente (por tanda) =========
                    b2BodyDef particleDef = b2DefaultBodyDef();
                    particleDef.type = b2_dynamicBody;
                    particleDef.position = (b2Vec2){pos.x, pos.y};
                    b2Rot rot = { std::cos(ang/2.0f), std::sin(ang/2.0f) };
                    particleDef.rotation = rot;
                    particleDef.isBullet = false;
                    b2BodyId particleId = b2CreateBody(worldId, &particleDef);

                    b2ShapeDef particleShapeDef = b2DefaultShapeDef();
                    particleShapeDef.density = Density;
                    particleShapeDef.material.friction = 0.5f;
                    particleShapeDef.material.restitution = 0.9f;

                    if (t == CIRCLE) {
                        float r = isLarge ? largeCircleRadius : smallCircleRadius;
                        b2Circle circle = {};
                        circle.radius = r;
                        b2CreateCircleShape(particleId, &particleShapeDef, &circle);

                        b2MassData massData = b2Body_GetMassData(particleId);
                        particles.push_back({particleId, CIRCLE, r, massData.mass, isLarge, 0});
                    } else {
                        int ns = std::max(3, NUM_SIDES);
                        float R = polyCircumRadius;
                        const float POLYGON_SKIN_RADIUS = 0.0f;

                        b2Vec2 vertices[BOX2D_MAX_POLYGON_VERTICES];
                        int actualNumSides = std::min(ns, BOX2D_MAX_POLYGON_VERTICES);
                        for (int j = 0; j < actualNumSides; ++j) {
                            float a = 2.0f * (float)M_PI * j / actualNumSides;
                            vertices[j] = (b2Vec2){R * std::cos(a), R * std::sin(a)};
                        }
                        b2Hull hull = b2ComputeHull(vertices, actualNumSides);
                        b2Polygon polygonShape = b2MakePolygon(&hull, POLYGON_SKIN_RADIUS);
                        b2CreatePolygonShape(particleId, &particleShapeDef, &polygonShape);

                        b2MassData massData = b2Body_GetMassData(particleId);
                        particles.push_back({particleId, POLYGON, R, massData.mass, true, actualNumSides});
                    }

                    particleBodyIds.push_back(particleId);
                    ok = true;
                }
            }

            if (!ok) {
                // escape: spawnear un poco por encima de la banda y dejar caer
                Vec2 posEsc = {Ux(randomEngine), bandTop + 3.0f*maxR};
                float angEsc = Uang(randomEngine);

                int newIndex = (int)placed.size();
                placed.push_back({posEsc, angEsc, t, isLarge});
                grid[cellOf(posEsc, cellSize)].push_back(newIndex);

                b2BodyDef particleDef = b2DefaultBodyDef();
                particleDef.type = b2_dynamicBody;
                particleDef.position = (b2Vec2){posEsc.x, posEsc.y};
                b2Rot rot = { std::cos(angEsc/2.0f), std::sin(angEsc/2.0f) };
                particleDef.rotation = rot;
                particleDef.isBullet = false;
                b2BodyId particleId = b2CreateBody(worldId, &particleDef);

                b2ShapeDef particleShapeDef = b2DefaultShapeDef();
                particleShapeDef.density = Density;
                particleShapeDef.material.friction = 0.5f;
                particleShapeDef.material.restitution = 0.9f;

                if (t == CIRCLE) {
                    float r = isLarge ? largeCircleRadius : smallCircleRadius;
                    b2Circle circle = {};
                    circle.radius = r;
                    b2CreateCircleShape(particleId, &particleShapeDef, &circle);

                    b2MassData massData = b2Body_GetMassData(particleId);
                    particles.push_back({particleId, CIRCLE, r, massData.mass, isLarge, 0});
                } else {
                    int ns = std::max(3, NUM_SIDES);
                    float R = polyCircumRadius;
                    const float POLYGON_SKIN_RADIUS = 0.0f;

                    b2Vec2 vertices[BOX2D_MAX_POLYGON_VERTICES];
                    int actualNumSides = std::min(ns, BOX2D_MAX_POLYGON_VERTICES);
                    for (int j = 0; j < actualNumSides; ++j) {
                        float a = 2.0f * (float)M_PI * j / actualNumSides;
                        vertices[j] = (b2Vec2){R * std::cos(a), R * std::sin(a)};
                    }
                    b2Hull hull = b2ComputeHull(vertices, actualNumSides);
                    b2Polygon polygonShape = b2MakePolygon(&hull, POLYGON_SKIN_RADIUS);
                    b2CreatePolygonShape(particleId, &particleShapeDef, &polygonShape);

                    b2MassData massData = b2Body_GetMassData(particleId);
                    particles.push_back({particleId, POLYGON, R, massData.mass, true, actualNumSides});
                }
                particleBodyIds.push_back(particleId);
            }
        } // fin for i en tanda

        // --------- Relax: dejamos caer y acomodar antes de la próxima tanda ----------
        // relaxWorld(GEN_RELAX_STEPS);
        settleUntilStable(worldId, /*maxTime*/ 1.5f, /*checkInterval*/ 0.5f,
                  /*dt*/ 1.0f/240.0f, /*subSteps*/ 4,
                  /*stabilityThreshold*/ 0.1f, /*requiredChecks*/ 2);
    } // fin for por tandas

    std::cout << "Generación por tandas sin superposición: " << TOTAL_PARTICLES
              << " partículas (mezcla discos/polígonos) con orientación aleatoria\n";
}

// Antes: void runSedimentation(b2WorldId worldId)
bool runSedimentation(b2WorldId worldId) {
    std::cout << "Dejando que " << TOTAL_PARTICLES << " partículas se sedimenten por gravedad\n";

    const float MAX_SEDIMENTATION_TIME   = 60.0f;
    const float STABILITY_CHECK_INTERVAL = 0.5f;
    const int   REQUIRED_CHECKS          = 3;

    const float KE_ABS_PER_PART_EPS      = 1e-3f;
    const float KE_DELTA_EPS             = 1e-2f;
    const float V_SLOW_EPS               = 0.05f;
    const float W_SLOW_EPS               = 0.2f;
    const float SLOW_FRACTION_REQUIRED   = 0.95f;

    float Rmax = BASE_RADIUS;
    if (SIZE_RATIO > 0.f) Rmax = std::max(Rmax, BASE_RADIUS * SIZE_RATIO);
    if (NUM_POLYGON_PARTICLES > 0 && NUM_SIDES >= 3) {
        float ns = std::max(3, NUM_SIDES);
        float polyR = POLYGON_PERIMETER / (2.0f * ns * std::sin(float(M_PI)/ns));
        Rmax = std::max(Rmax, polyR);
    }
    const float pad = Rmax + 0.01f;
    const float bandTop    = GROUND_LEVEL_Y + silo_height - pad;
    const float bandMargin = 2.0f * Rmax;
    const float yCeiling   = bandTop - bandMargin;

    float t = 0.0f, lastCheck = 0.0f;
    float prevKE = 1e9f;
    int stableCount = 0;

    while (t < MAX_SEDIMENTATION_TIME) {
        b2World_Step(worldId, TIME_STEP, SUB_STEP_COUNT);
        t += TIME_STEP;

        if (t - lastCheck >= STABILITY_CHECK_INTERVAL) {
            float KE = 0.0f;
            int slowCount = 0;
            float yMax = -1e30f;

            for (const auto& p : particles) {
                b2Vec2 v = b2Body_GetLinearVelocity(p.bodyId);
                float w  = b2Body_GetAngularVelocity(p.bodyId);
                KE += 0.5f * p.mass * (v.x*v.x + v.y*v.y);
                if (std::sqrt(v.x*v.x + v.y*v.y) < V_SLOW_EPS && std::fabs(w) < W_SLOW_EPS)
                    ++slowCount;
                b2Vec2 pos = b2Body_GetPosition(p.bodyId);
                if (pos.y > yMax) yMax = pos.y;
            }

            const float KE_per_part = (TOTAL_PARTICLES > 0) ? KE / TOTAL_PARTICLES : 0.0f;
            const float dKE         = std::fabs(KE - prevKE);
            const float slowFrac    = (TOTAL_PARTICLES > 0) ? float(slowCount) / TOTAL_PARTICLES : 1.0f;
            const bool  bandClear   = (yMax <= yCeiling);

            bool stableKinetics = (KE_per_part < KE_ABS_PER_PART_EPS &&
                                   dKE         < KE_DELTA_EPS &&
                                   slowFrac    >= SLOW_FRACTION_REQUIRED);

            if (stableKinetics && bandClear) {
                if (++stableCount >= REQUIRED_CHECKS) {
                    std::cout << "Estabilización completa en " << t << " s\n";
                    return true;
                }
            } else {
                stableCount = 0;
            }

            prevKE   = KE;
            lastCheck = t;
        }
    }

    std::cout << "Sedimentación: timeout a " << MAX_SEDIMENTATION_TIME << " s (NO estable)\n";
    return false;
}


