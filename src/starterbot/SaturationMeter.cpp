#include "SaturationMeter.h"

void SaturationMeter::setWorkersPerMineral(int n)
{
    m_workersPerMineral = n;
    m_readySince = -1;
    m_windowStartFrame = -1;
    m_windowStartMinerals = 0;
}

void SaturationMeter::onFrame(bool configReady)
{
    const int now = BWAPI::Broodwar->getFrameCount();

    if (!configReady)
    {
        // Faltan obreros para completar la configuracion, o se rompio porque
        // murio alguno. En cualquier caso lo medido hasta aqui ya no corresponde
        // a una sola configuracion, asi que se descarta.
        m_readySince = -1;
        m_windowStartFrame = -1;
        return;
    }

    if (m_readySince < 0) { m_readySince = now; }
    if (m_windowStartFrame >= 0) { return; }   // la ventana ya esta abierta

    // Aun con todos los obreros presentes hay que dejarlos llegar a su campo
    // antes de empezar a contar.
    if (now - m_readySince >= WARMUP_FRAMES)
    {
        m_windowStartFrame = now;
        m_windowStartMinerals = BWAPI::Broodwar->self()->gatheredMinerals();
    }
}

int SaturationMeter::getElapsedFrames() const
{
    if (m_windowStartFrame < 0) { return 0; }
    return BWAPI::Broodwar->getFrameCount() - m_windowStartFrame;
}

int SaturationMeter::getGathered() const
{
    if (m_windowStartFrame < 0) { return 0; }
    return BWAPI::Broodwar->self()->gatheredMinerals() - m_windowStartMinerals;
}

double SaturationMeter::getRatePer1000Frames() const
{
    const int frames = getElapsedFrames();
    if (frames <= 0) { return 0.0; }
    return (double)getGathered() * 1000.0 / (double)frames;
}

void SaturationMeter::draw(int patches, int workers) const
{
    int y = 160;   // debajo del recuadro del ranking

    BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
        "SATURACION: %d obrero(s) por campo", m_workersPerMineral);
    y += 10;

    BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
        "  campos en base: %d   obreros asignados: %d", patches, workers);
    y += 10;

    if (m_windowStartFrame < 0)
    {
        if (m_readySince < 0)
        {
            BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
                "  esperando obreros (faltan %d)", patches * m_workersPerMineral - workers);
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
        "  ventana: %d frames   recolectado: %d", getElapsedFrames(), getGathered());
    y += 10;

    BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
        "  tasa: %.2f min/1000f", getRatePer1000Frames());
    y += 10;

    // El dato que decide el experimento. El total sube siempre al sumar obreros;
    // lo que cae cuando el campo se satura es cuanto aporta cada obrero.
    const double porObrero = (workers > 0) ? getRatePer1000Frames() / (double)workers : 0.0;
    BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
        "  por obrero: %.3f min/1000f", porObrero);
}
