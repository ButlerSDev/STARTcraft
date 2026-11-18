#pragma once

#include <BWAPI.h>
#include <string>

// Datos medidos en 4Retro1.2_1.16-Backport.scx con el modo de investigacion del
// bot (18/11/2026). Van como constantes porque el mapa no cambia: el modo de
// exposicion los muestra en pantalla y reparte los obreros con ellos, sin
// depender de que existan los archivos de ranking en la carpeta.
//
// Ranking: frames promedio entre dos bajadas de 8 minerales, con un obrero por
// campo, de mas a menos eficiente. Cada raza y cada lado del mapa se midieron
// en partidas propias, porque el obrero de cada raza tiene otra velocidad y la
// formacion del otro lado es la base del rival.
namespace MeasuredData
{
    struct Field
    {
        int x, y;            // tile del centro del campo
        double average;      // frames entre bajadas
        int samples;
    };

    struct Ranking
    {
        const char* race;    // "Terran", "Protoss", "Zerg"
        bool left;           // formacion izquierda (true) o derecha (false)
        Field fields[9];
    };

    const Ranking RANKINGS[] =
    {
        { "Terran", true, {
            {3, 11, 150.90, 29}, {4, 13, 156.03, 33}, {3, 6, 172.80, 20},
            {3, 8, 173.88, 32},  {3, 9, 174.15, 20},  {2, 10, 180.30, 20},
            {2, 7, 181.45, 20},  {6, 13, 182.72, 32}, {3, 12, 188.90, 20} } },
        { "Terran", false, {
            {125, 11, 149.10, 60}, {126, 6, 162.90, 60}, {125, 9, 171.73, 60},
            {125, 7, 172.25, 60},  {126, 8, 172.82, 60}, {124, 13, 173.13, 60},
            {125, 12, 173.35, 60}, {126, 10, 174.48, 60}, {125, 5, 198.53, 60} } },
        { "Protoss", true, {
            {4, 13, 153.40, 20}, {3, 8, 154.40, 20},  {3, 6, 154.85, 20},
            {3, 9, 155.35, 20},  {6, 13, 163.35, 20}, {3, 11, 170.90, 20},
            {3, 12, 170.95, 20}, {2, 7, 173.15, 20},  {2, 10, 175.00, 20} } },
        { "Protoss", false, {
            {125, 9, 149.15, 60},  {125, 11, 153.50, 60}, {125, 12, 154.35, 60},
            {126, 6, 154.84, 60},  {126, 10, 155.80, 60}, {125, 7, 163.37, 60},
            {125, 5, 171.67, 60},  {126, 8, 173.02, 60},  {124, 13, 173.60, 60} } },
        { "Zerg", true, {
            {3, 11, 154.74, 58}, {3, 6, 162.86, 57},  {3, 9, 163.33, 57},
            {3, 8, 163.49, 56},  {6, 13, 169.85, 53}, {4, 13, 171.15, 54},
            {2, 7, 173.28, 54},  {3, 12, 174.65, 52}, {2, 10, 176.64, 50} } },
        { "Zerg", false, {
            {125, 12, 148.95, 20}, {125, 9, 153.15, 20}, {125, 7, 164.70, 20},
            {126, 6, 169.75, 20},  {125, 11, 171.65, 20}, {126, 8, 173.60, 20},
            {125, 5, 173.70, 20},  {124, 13, 182.15, 20}, {126, 10, 182.15, 20} } },
    };
    const int RANKING_COUNT = sizeof(RANKINGS) / sizeof(RANKINGS[0]);

    // Pregunta 2: saturacion en paralelo, promedio de las tres rotaciones del
    // cuadrado latino (Zerg derecha, Protoss izquierda, Zerg derecha). Minerales
    // por cada 1000 frames en un campo con 1, 2 y 3 obreros.
    const double SATURATION_TOTAL[3] = { 48.89, 82.67, 91.56 };

    // Ranking medido para una raza y un lado, o nullptr si no hay.
    inline const Ranking* find(const std::string& race, bool left)
    {
        for (int i = 0; i < RANKING_COUNT; i++)
        {
            if (race == RANKINGS[i].race && RANKINGS[i].left == left) { return &RANKINGS[i]; }
        }
        return nullptr;
    }
}
