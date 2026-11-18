#include "PatchSaturationTest.h"

#include <cstdio>
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
    return BWAPI::Broodwar->getFrameCount() - m_windowStart;
}

int PatchSaturationTest::gathered(const Patch& p) const
{
    if (m_windowStart < 0 || !p.unit || !p.unit->exists()) { return 0; }
    return p.startResources - p.unit->getResources();
}

double PatchSaturationTest::ratePer1000(const Patch& p) const
{
    const int frames = elapsed();
    if (frames <= 0) { return 0.0; }
    return (double)gathered(p) * 1000.0 / (double)frames;
}

void PatchSaturationTest::onFrame(bool configReady)
{
    if (m_patches.empty()) { return; }

    const int now = BWAPI::Broodwar->getFrameCount();

    if (!configReady)
    {
        // Falta un obrero o murio alguno: lo medido ya no corresponde a la
        // configuracion, asi que la ventana se descarta.
        m_readySince = -1;
        m_windowStart = -1;
        return;
    }

    if (m_readySince < 0) { m_readySince = now; }

    if (m_windowStart < 0 && now - m_readySince >= WARMUP_FRAMES)
    {
        m_windowStart = now;
        for (Patch& p : m_patches) { p.startResources = p.unit->getResources(); }
    }
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

    BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
        "  midiendo desde hace %d frames", elapsed());
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
    if (m_windowStart < 0)
    {
        std::fprintf(f, "rotacion=%d  raza=%s  sin ventana completa (%s, frame %d)\n",
                     m_rotation, raza.c_str(), motivo, BWAPI::Broodwar->getFrameCount());
        std::fclose(f);
        return;
    }

    for (const Patch& p : m_patches)
    {
        const BWAPI::TilePosition tile(p.unit->getPosition());
        const double rate = ratePer1000(p);
        std::fprintf(f, "rotacion=%d  raza=%s  tile=(%d,%d)  obreros=%d  frames=%d  recolectado=%d  min_1000f=%.2f  por_obrero=%.2f  (%s)\n",
                     m_rotation, raza.c_str(), tile.x, tile.y, p.workers, elapsed(), gathered(p),
                     rate, rate / p.workers, motivo);
    }
    std::fclose(f);
}
