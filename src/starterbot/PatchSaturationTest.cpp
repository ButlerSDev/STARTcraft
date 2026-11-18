#include "PatchSaturationTest.h"

#include <cstdio>
#include <cstring>
#include <string>

void PatchSaturationTest::setup(const std::vector<BWAPI::Unit>& patches, int rotation)
{
    // Cuadrado latino: en la rotacion r, el campo i recibe ((i + r) % 3) + 1
    // obreros. Rotacion 0: 1,2,3. Rotacion 1: 2,3,1. Rotacion 2: 3,1,2.
    m_patches.clear();
    m_rotation = rotation % 3;
    for (size_t i = 0; i < patches.size() && i < 3; i++)
    {
        Patch p;
        p.unit = patches[i];
        p.workers = (int)((i + m_rotation) % 3) + 1;
        m_patches.push_back(p);
    }
    m_readySince = -1;
    m_windowStart = -1;
    m_windowEnd = -1;
    m_finished = false;
}

int PatchSaturationTest::countRecordedGames()
{
    // Cada partida con datos tiene exactamente una linea con obreros=1.
    int partidas = 0;
    if (std::FILE* f = std::fopen("saturacion_4retro.txt", "r"))
    {
        char linea[512];
        while (std::fgets(linea, sizeof(linea), f))
        {
            if (std::strstr(linea, "obreros=1 ")) { partidas++; }
        }
        std::fclose(f);
    }
    return partidas;
}

int PatchSaturationTest::capFor(BWAPI::Unit mineral) const
{
    for (const Patch& p : m_patches)
    {
        if (p.unit == mineral) { return p.workers; }
    }
    return 0;
}

int PatchSaturationTest::totalWorkers() const
{
    int total = 0;
    for (const Patch& p : m_patches) { total += p.workers; }
    return total;
}

int PatchSaturationTest::elapsed() const
{
    if (m_windowStart < 0) { return 0; }
    const int fin = (m_windowEnd >= 0) ? m_windowEnd : BWAPI::Broodwar->getFrameCount();
    return fin - m_windowStart;
}

int PatchSaturationTest::gathered(const Patch& p) const
{
    if (m_windowStart < 0) { return 0; }
    if (m_windowEnd >= 0) { return p.startResources - p.endResources; }
    if (!p.unit || !p.unit->exists()) { return 0; }
    return p.startResources - p.unit->getResources();
}

void PatchSaturationTest::finish(const char* motivo)
{
    m_windowEnd = BWAPI::Broodwar->getFrameCount();
    for (Patch& p : m_patches)
    {
        p.endResources = (p.unit && p.unit->exists()) ? p.unit->getResources() : p.startResources;
    }
    m_finished = true;
    writeResults(motivo);
    BWAPI::Broodwar->printf("Saturacion en paralelo: resultado anotado (%s)", motivo);
}

double PatchSaturationTest::ratePer1000(const Patch& p) const
{
    const int frames = elapsed();
    if (frames <= 0) { return 0.0; }
    return (double)gathered(p) * 1000.0 / (double)frames;
}

void PatchSaturationTest::onFrame(bool configReady)
{
    if (m_patches.empty() || m_finished) { return; }

    const int now = BWAPI::Broodwar->getFrameCount();

    if (!configReady)
    {
        // Falta un obrero o murio alguno. Si ya habia una ventana larga, se anota
        // lo medido hasta aqui; si era corta, se descarta y se vuelve a esperar.
        if (m_windowStart >= 0 && elapsed() >= MIN_PARTIAL_FRAMES)
        {
            finish("ventana parcial, se rompio la configuracion");
            return;
        }
        m_readySince = -1;
        m_windowStart = -1;
        return;
    }

    if (m_readySince < 0) { m_readySince = now; }

    if (m_windowStart < 0)
    {
        if (now - m_readySince >= WARMUP_FRAMES)
        {
            m_windowStart = now;
            for (Patch& p : m_patches) { p.startResources = p.unit->getResources(); }
        }
        return;
    }

    if (elapsed() >= MEASURE_FRAMES) { finish("ventana completa"); }
}

void PatchSaturationTest::draw() const
{
    int y = 145;
    BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
        "SATURACION EN PARALELO - rotacion %d (cada campo con 1, 2 o 3 obreros)", m_rotation);
    y += 10;

    if (m_windowStart < 0)
    {
        if (m_readySince < 0)
        {
            BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
                "  esperando a reunir %d obreros", totalWorkers());
        }
        else
        {
            const int faltan = WARMUP_FRAMES - (BWAPI::Broodwar->getFrameCount() - m_readySince);
            BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
                "  calentando, faltan %d frames", faltan > 0 ? faltan : 0);
        }
        return;
    }

    if (m_finished)
    {
        BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
            "  TERMINADO: %d frames medidos, anotado en saturacion_4retro.txt", elapsed());
    }
    else
    {
        BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
            "  midiendo %d/%d frames", elapsed(), MEASURE_FRAMES);
    }
    y += 10;
    BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
        "  obreros  tile      recolectado  min/1000f  por obrero");
    y += 10;

    for (const Patch& p : m_patches)
    {
        const BWAPI::TilePosition tile(p.unit->getPosition());
        const double rate = ratePer1000(p);
        BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
            "  %d        (%d,%d)    %4d         %7.2f    %6.2f",
            p.workers, tile.x, tile.y, gathered(p), rate, rate / p.workers);
        y += 10;

        // Sobre el campo, debajo del contador de la Pregunta 1.
        const BWAPI::Position pos = p.unit->getPosition();
        BWAPI::Broodwar->drawTextMap(BWAPI::Position(pos.x - 30, pos.y + 6),
            "%d obr: %.1f/obr", p.workers, rate / p.workers);
    }
}

void PatchSaturationTest::writeResults(const char* motivo) const
{
    if (m_patches.empty()) { return; }

    std::FILE* f = std::fopen("saturacion_4retro.txt", "a");
    if (!f) { return; }

    const std::string raza = BWAPI::Broodwar->self()->getRace().getName();
    const BWAPI::Position base = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
    const char* lado = base.x < BWAPI::Broodwar->mapWidth() * 16 ? "izquierda" : "derecha";
    if (m_windowStart < 0)
    {
        std::fprintf(f, "rotacion=%d  raza=%s  lado=%s  sin datos (%s, frame %d)\n",
                     m_rotation, raza.c_str(), lado, motivo, BWAPI::Broodwar->getFrameCount());
        std::fclose(f);
        return;
    }

    for (const Patch& p : m_patches)
    {
        const BWAPI::TilePosition tile(p.unit->getPosition());
        const double rate = ratePer1000(p);
        std::fprintf(f, "rotacion=%d  raza=%s  lado=%s  tile=(%d,%d)  obreros=%d  frames=%d  recolectado=%d  min_1000f=%.2f  por_obrero=%.2f  (%s)\n",
                     m_rotation, raza.c_str(), lado, tile.x, tile.y, p.workers, elapsed(), gathered(p),
                     rate, rate / p.workers, motivo);
    }
    std::fclose(f);
}
