#pragma once

#include <BWAPI.h>
#include <deque>

// Mantiene el estado de UN campo de minerales (mineral field):
// cuantos frames pasan entre una extraccion (-8 unidades) y la siguiente.
class MineralField
{
    static const size_t MAX_HISTORY = 20;   // promedio de los ultimos 20 conteos

    BWAPI::Unit     m_unit = nullptr;
    BWAPI::Position m_position;             // centro del mineral, para ubicar el texto

    int  m_lastResources = 0;               // cantidad de minerales en el frame anterior
    int  m_framesSinceDrop = 0;             // frames transcurridos desde la ultima bajada
    bool m_timingStarted = false;           // true despues de ver la 1ra bajada (ver nota en .cpp)
    int  m_lastCount = 0;                   // ultimo conteo completado antes de reiniciar
    std::deque<int> m_history;              // ultimos conteos completados (max 20)

public:

    explicit MineralField(BWAPI::Unit unit);

    // Llamar una vez por frame: compara con el frame anterior, actualiza contadores.
    void onFrame();

    // Dibuja el texto sobre el mineral (drawTextMap).
    void draw() const;

    // Olvida los conteos registrados y vuelve a esperar una primera bajada.
    // Se usa al terminar el split, cuyos repartos no son de un obrero por campo.
    void resetHistory();

    // Para las estadisticas de la exposicion (ranking de eficiencia).
    double          getAverage() const;     // promedio de los ultimos <=20 conteos
    int             getLastCount() const { return m_lastCount; }
    int             getFramesSinceDrop() const { return m_framesSinceDrop; }
    int             getSamples() const { return (int)m_history.size(); }
    BWAPI::Position getPosition() const { return m_position; }
    BWAPI::Unit     getUnit() const { return m_unit; }
};
