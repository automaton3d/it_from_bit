// geometry.cpp
// Geometric helper functions for spherical topology with antipodal wrapping
// These functions may use floating-point internally, but return integer results
// for deterministic CA behavior.

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdio>
#include <cassert>
#include <random>
#include <vector>
#include <algorithm>
#include "model/simulation.h"
#include "model/geometry.h"

namespace automaton
{
    /**
     * Calculates geodesic distance on a sphere with antipodal wrapping.
     * For points inside the sphere: distance = arcsin(r/R) * R
     * For points outside: distance = πR - arcsin(R/r) * R (path through antipode)
     * 
     * @param x,y,z Coordinates in cube space [0, EL-1]
     * @return Geodesic distance from center (0 to RMAX)
     */
    unsigned geodesicDistance(int x, int y, int z)
    {
        const double R = EL / 2.0;           // Sphere radius
        const double C = (EL - 1) / 2.0;     // Center coordinate
        
        double dx = x - C;
        double dy = y - C;
        double dz = z - C;
        
        double euclidean_r = sqrt(dx*dx + dy*dy + dz*dz);
        
        if (euclidean_r <= 1e-6) {
            return 0;                       // Exact center
        }
        
        if (euclidean_r <= R) {
            // Inside sphere: arc from center to point
            double theta = asin(euclidean_r / R);
            return (unsigned)round(R * theta);
        } else {
            // Outside sphere: path through antipode
            double theta = 3.14159265358979323846 - asin(R / euclidean_r);
            return (unsigned)round(R * theta);
        }
    }
    
    /**
     * Checks whether a point lies inside the spherical domain.
     * Uses a small tolerance for boundary cases.
     * 
     * @param x,y,z Coordinates in cube space [0, EL-1]
     * @return true if inside or on sphere surface
     */
    bool isInsideSphere(int x, int y, int z)
    {
        const double R = EL / 2.0;           // Sphere radius
        const double C = (EL - 1) / 2.0;     // Center coordinate
        
        double dx = x - C;
        double dy = y - C;
        double dz = z - C;
        
        return (dx*dx + dy*dy + dz*dz <= R*R + 0.5);
    }
    
    /**
     * Antipodal spherical wrapping for torus-to-sphere topology.
     * When a point exits the sphere, it re-enters from the antipodal side.
     * This preserves geodesic distance across the sphere's surface.
     * 
     * Algorithm:
     * 1. Keep points inside/on surface unchanged
     * 2. For outside points: project radially to surface, then invert through center
     * 
     * @param x,y,z Coordinates to wrap (modified in place)
     */
/**
 * Antipodal spherical wrapping - VERSÃO MAIS SIMPLES E PREVISÍVEL.
 * 
 * Princípio: Em uma esfera, quando um ponto sai pela superfície,
 * ele re-entra pelo ponto exatamente oposto (antípoda).
 * 
 * Implementação:
 * 1. Coordenadas são primeiro trazidas para o cubo [0, EL-1] via wrap periódico
 * 2. Se estiver dentro da esfera, mantém
 * 3. Se estiver fora, aplica reflexão pelo centro: P' = 2*C - P
 * 4. Garante que o resultado está dentro do cubo
 */
/**
 * Antipodal spherical wrapping (discreto)
 *
 * A identificação antipodal correta em uma grade
 * 0..EL-1 é:
 *
 *      x' = EL - 1 - x
 *
 * Isso funciona corretamente para:
 *  - EL par
 *  - EL ímpar
 *
 * sem gerar índices inválidos.
 */
inline void spherical_wrap(int& x, int& y, int& z)
{
    int dx = x - CENTER;
    int dy = y - CENTER;
    int dz = z - CENTER;

    int r2 = dx*dx + dy*dy + dz*dz;

    if (r2 <= RMAX*RMAX)
        return;

    // Antípoda radial REAL
    dx = -dx;
    dy = -dy;
    dz = -dz;

    x = CENTER + dx;
    y = CENTER + dy;
    z = CENTER + dz;
}

void spherical_wrap_xxxxx(int& x, int& y, int& z)
{
    // Passo 1: Wrap periódico para garantir coordenadas no cubo [0, EL-1]
    // Isso é necessário porque os deslocamentos podem ser grandes
    while (x < 0) x += EL;
    while (x >= (int)EL) x -= EL;
    while (y < 0) y += EL;
    while (y >= (int)EL) y -= EL;
    while (z < 0) z += EL;
    while (z >= (int)EL) z -= EL;
    
    // Passo 2: Verificar se está dentro da esfera
    int dx = x - (int)CENTER;
    int dy = y - (int)CENTER;
    int dz = z - (int)CENTER;
    int r2 = dx*dx + dy*dy + dz*dz;
    int R2 = (int)(RMAX * RMAX);
    
    // Se dentro da esfera, mantém
    if (r2 <= R2) {
        return;
    }
    
    // Passo 3: Reflexão pelo centro (antípoda)
    // Fórmula: P' = 2*C - P
    int nx = 2 * (int)CENTER - x;
    int ny = 2 * (int)CENTER - y;
    int nz = 2 * (int)CENTER - z;
    
    // Passo 4: Garantir que está no cubo
    while (nx < 0) nx += EL;
    while (nx >= (int)EL) nx -= EL;
    while (ny < 0) ny += EL;
    while (ny >= (int)EL) ny -= EL;
    while (nz < 0) nz += EL;
    while (nz >= (int)EL) nz -= EL;
    
    x = nx;
    y = ny;
    z = nz;
    
    // Passo 5: Verificação final (opcional, para debug)
    // O ponto refletido DEVE estar dentro da esfera
    dx = x - (int)CENTER;
    dy = y - (int)CENTER;
    dz = z - (int)CENTER;
    r2 = dx*dx + dy*dy + dz*dz;
    
    if (r2 > R2) {
        // Se ainda está fora, algo deu errado - log para debug
        static int warning_count = 0;
        if (warning_count++ < 10) {
            printf("WARNING: After antipodal wrap, point still outside sphere!\n");
        }
        // Força para dentro da esfera (projeção radial simples)
        double r = sqrt((double)r2);
        double scale = RMAX / r;
        x = (int)CENTER + (int)round(dx * scale);
        y = (int)CENTER + (int)round(dy * scale);
        z = (int)CENTER + (int)round(dz * scale);
    }
}
}