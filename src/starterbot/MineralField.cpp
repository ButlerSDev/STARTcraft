#include "MineralField.h"

MineralField::MineralField(BWAPI::Unit unit)
    : m_unit(unit)
    , m_position(unit->getPosition())
    , m_lastResources(unit->getResources())
{
}

void MineralField::onFrame()
{
    if (!m_unit || !m_unit->exists()) { return; }

    const int current = m_unit->getResources();

    if (current < m_lastResources)
    {
        // Se extrajeron minerales de este campo en este frame.
        // El conteo previo a la PRIMERA bajada que vemos no es un intervalo real
        // (empezo cuando empezamos a mirar, no en una extraccion), asi que no se
        // guarda: solo arranca la medicion. Desde la 2da bajada en adelante, cada
        // conteo si es un intervalo completo entre dos extracciones.
        if (m_timingStarted)
        {
            m_lastCount = m_framesSinceDrop;
            m_history.push_back(m_framesSinceDrop);
            if (m_history.size() > MAX_HISTORY) { m_history.pop_front(); }
        }

        m_timingStarted = true;
        m_framesSinceDrop = 0;
    }
    else
    {
        m_framesSinceDrop++;
    }

    m_lastResources = current;
}

double MineralField::getAverage() const
{
    if (m_history.empty()) { return 0.0; }

    // Con menos de 20 conteos se promedian los que haya.
    int sum = 0;
    for (int c : m_history) { sum += c; }
    return (double)sum / (double)m_history.size();
}

void MineralField::draw() const
{
    if (!m_unit || !m_unit->exists()) { return; }

    // El texto se ubica a la izquierda del centro del mineral para no taparlo
    // ni pisar a los vecinos. Ajustar el offset viendolo en pantalla.
    const BWAPI::Position textPos(m_position.x - 32, m_position.y - 16);

    // Sin acentos: la fuente de StarCraft no los dibuja bien.
    if (m_history.empty())
    {
        BWAPI::Broodwar->drawTextMap(textPos, "Ultimo: -\nProm: -\nAhora: %d", m_framesSinceDrop);
    }
    else
    {
        BWAPI::Broodwar->drawTextMap(textPos, "Ultimo: %d\nProm: %.2f\nAhora: %d",
                                     m_lastCount, getAverage(), m_framesSinceDrop);
    }
}
