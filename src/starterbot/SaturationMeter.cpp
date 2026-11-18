#include "SaturationMeter.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>

SaturationMeter::SaturationMeter()
{
    // Solo se arma el plan. No se toca BWAPI::Broodwar aqui: el objeto se
    // construye antes de que exista la partida, y la primera llamada a onFrame
    // es la que arranca la fase 1 con el frame real.
    m_plan.push_back(1);
    m_plan.push_back(2);
    m_plan.push_back(3);
    m_workersPerMineral = m_plan[0];
}

int SaturationMeter::getMaxWorkersPerMineral() const
{
    if (!m_auto) { return m_workersPerMineral; }
    return *std::max_element(m_plan.begin(), m_plan.end());
}

void SaturationMeter::appendLog(const char* format, ...)
{
    std::FILE* f = std::fopen("saturacion_resultados.txt", "a");
    if (!f) { return; }

    char linea[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(linea, sizeof(linea), format, args);
    va_end(args);

    std::fprintf(f, "%s\n", linea);
    std::fclose(f);
}

void SaturationMeter::beginPhase(size_t phase)
{
    m_phase = phase;
    if (phase < m_plan.size()) { m_workersPerMineral = m_plan[phase]; }
    m_phaseStartFrame = BWAPI::Broodwar->getFrameCount();
    m_readySince = -1;
    m_windowStartFrame = -1;
    m_windowStartMinerals = 0;
}

void SaturationMeter::restartAuto()
{
    // Encabezado de la corrida en el archivo: mapa y raza, para que cada bloque
    // de resultados diga de donde salio.
    appendLog("--- Saturacion en %s, raza %s, desde el frame %d ---",
              BWAPI::Broodwar->mapFileName().c_str(),
              BWAPI::Broodwar->self()->getRace().getName().c_str(),
              BWAPI::Broodwar->getFrameCount());

    m_started = true;
    m_auto = true;
    m_finished = false;
    m_results.clear();
    beginPhase(0);
}

void SaturationMeter::setWorkersPerMineral(int n)
{
    appendLog("--- Saturacion manual en %s, raza %s, %d obrero(s) por campo, desde el frame %d ---",
              BWAPI::Broodwar->mapFileName().c_str(),
              BWAPI::Broodwar->self()->getRace().getName().c_str(),
              n, BWAPI::Broodwar->getFrameCount());

    m_started = true;
    m_auto = false;
    m_finished = false;
    m_workersPerMineral = n;
    m_phaseStartFrame = BWAPI::Broodwar->getFrameCount();
    m_readySince = -1;
    m_windowStartFrame = -1;
    m_windowStartMinerals = 0;
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

void SaturationMeter::recordAndAdvance(int patches, int workers, bool ok)
{
    SaturationResult r;
    r.workersPerMineral = m_workersPerMineral;
    r.patches = patches;
    r.workers = workers;
    r.ok = ok;

    if (ok)
    {
        r.frames = getElapsedFrames();
        r.gathered = getGathered();
        r.rate = getRatePer1000Frames();
        r.perWorker = (workers > 0) ? r.rate / (double)workers : 0.0;

        BWAPI::Broodwar->printf("Saturacion %d obrero(s)/campo: %.2f min/1000f, %.3f por obrero",
                                r.workersPerMineral, r.rate, r.perWorker);
        appendLog("N=%d  campos=%d  obreros=%d  frames=%d  recolectado=%d  min/1000f=%.2f  por_obrero=%.3f  (fin en frame %d)",
                  r.workersPerMineral, r.patches, r.workers, r.frames, r.gathered, r.rate, r.perWorker,
                  BWAPI::Broodwar->getFrameCount());
    }
    else
    {
        BWAPI::Broodwar->printf("Saturacion %d obrero(s)/campo: abandonada, no se reunieron los obreros",
                                r.workersPerMineral);
        appendLog("N=%d  abandonada: no se reunieron los obreros (frame %d)",
                  r.workersPerMineral, BWAPI::Broodwar->getFrameCount());
    }

    m_results.push_back(r);

    if (m_phase + 1 < m_plan.size())
    {
        beginPhase(m_phase + 1);
    }
    else
    {
        m_finished = true;
        BWAPI::Broodwar->printf("Saturacion: experimento completo");
    }
}

void SaturationMeter::onFrame(bool configReady, int patches, int workers)
{
    // Primera llamada dentro de la partida: recien aqui hay frame que anotar.
    if (!m_started)
    {
        m_started = true;
        beginPhase(0);
    }

    if (m_finished) { return; }

    const int now = BWAPI::Broodwar->getFrameCount();

    if (!configReady)
    {
        // Faltan obreros, o se rompio la configuracion porque murio alguno. Lo
        // medido hasta aqui ya no corresponde a una sola configuracion.
        m_readySince = -1;
        m_windowStartFrame = -1;

        // Si lleva demasiado tiempo sin poder reunirlos, no tiene sentido esperar
        // mas: se anota como abandonada y el experimento sigue con la siguiente.
        if (m_auto && now - m_phaseStartFrame > STALL_FRAMES)
        {
            recordAndAdvance(patches, workers, false);
        }
        return;
    }

    if (m_readySince < 0) { m_readySince = now; }

    if (m_windowStartFrame < 0)
    {
        if (now - m_readySince >= WARMUP_FRAMES)
        {
            m_windowStartFrame = now;
            m_windowStartMinerals = BWAPI::Broodwar->self()->gatheredMinerals();
        }
        return;
    }

    // Ventana cumplida: se anota el resultado y, en automatico, se pasa a la
    // siguiente configuracion. En manual la medicion sigue corriendo.
    if (m_auto && getElapsedFrames() >= MEASURE_FRAMES)
    {
        recordAndAdvance(patches, workers, true);
    }
}

void SaturationMeter::draw(int patches, int workers) const
{
    int y = 160;

    // El titulo se arma sin ternarios dentro del formato. Una captura anterior
    // mostro "COMPLETO" junto a "fase 1/3", que son estados incompatibles, asi
    // que conviene que cada linea diga una sola cosa.
    if (m_finished)
    {
        BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y), "SATURACION - COMPLETO");
    }
    else if (m_auto)
    {
        BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y), "SATURACION - automatico");
    }
    else
    {
        BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y), "SATURACION - manual");
    }
    y += 10;

    // Estado interno en crudo, para diagnosticar. Si una captura vuelve a
    // mostrar algo imposible, estos numeros dicen por que.
    BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
        "  [fin=%d res=%d fase=%d ready=%d win=%d frame=%d]",
        m_finished ? 1 : 0, (int)m_results.size(), (int)m_phase,
        m_readySince, m_windowStartFrame, BWAPI::Broodwar->getFrameCount());
    y += 10;

    if (!m_finished)
    {
        BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
            "  fase %d/%d: %d obrero(s) por campo   campos: %d   asignados: %d",
            (int)m_phase + 1, (int)m_plan.size(), m_workersPerMineral, patches, workers);
        y += 10;

        if (m_windowStartFrame < 0)
        {
            if (m_readySince < 0)
            {
                const int faltan = patches * m_workersPerMineral - workers;
                BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
                    "  esperando obreros (faltan %d)", faltan > 0 ? faltan : 0);
            }
            else
            {
                const int faltan = WARMUP_FRAMES - (BWAPI::Broodwar->getFrameCount() - m_readySince);
                BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
                    "  calentando, faltan %d frames", faltan > 0 ? faltan : 0);
            }
        }
        else
        {
            BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
                "  midiendo %d/%d frames   tasa %.2f   por obrero %.3f",
                getElapsedFrames(), MEASURE_FRAMES, getRatePer1000Frames(),
                (workers > 0) ? getRatePer1000Frames() / (double)workers : 0.0);
        }
        y += 10;
    }

    if (m_results.empty()) { return; }

    // Tabla de resultados. La columna que responde la pregunta es la ultima:
    // si cae al sumar obreros, el campo se esta saturando.
    y += 5;
    BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
        "  N  obreros  min/1000f  por obrero");
    y += 10;

    for (size_t i = 0; i < m_results.size(); i++)
    {
        const SaturationResult& r = m_results[i];
        if (r.ok)
        {
            BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
                "  %d    %3d     %7.2f     %6.3f", r.workersPerMineral, r.workers, r.rate, r.perWorker);
        }
        else
        {
            BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
                "  %d    %3d     (abandonada)", r.workersPerMineral, r.workers);
        }
        y += 10;
    }
}
