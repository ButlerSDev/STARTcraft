#pragma once

#include <BWAPI.h>

// Pregunta 2: mide el INGRESO DE MINERALES bajo una configuracion fija de
// obreros por campo, para responder cuantos obreros por mineral conviene poner.
//
// La idea del experimento: el total recolectado siempre sube al agregar obreros,
// porque hay mas gente trabajando. Lo que interesa es el aporte POR OBRERO, que
// cae conforme los obreros empiezan a hacer cola sobre el mismo campo. Ese punto
// donde agregar un obrero mas casi no suma es la saturacion.
//
// Cada corrida se hace con un valor distinto de obreros por campo (/sat 1, 2, 3)
// y se anota la tasa resultante.
class SaturationMeter
{
    // Frames que se descartan tras fijar una configuracion. Al cambiar el numero
    // de obreros por campo, el reparto tarda en estabilizarse (los obreros tienen
    // que caminar a su nuevo campo), y contar esos frames ensucia la tasa.
    static const int WARMUP_FRAMES = 1000;

    int m_workersPerMineral = 1;
    int m_readySince = -1;          // frame desde el que la configuracion esta completa
    int m_windowStartFrame = -1;    // -1 mientras no hay ventana abierta
    int m_windowStartMinerals = 0;  // minerales acumulados al abrir la ventana

public:

    // Fija los obreros por campo y reinicia la medicion desde cero.
    void setWorkersPerMineral(int n);
    int  getWorkersPerMineral() const { return m_workersPerMineral; }

    // Llamar una vez por frame.
    //
    // configReady indica si en este frame hay exactamente los obreros que pide la
    // configuracion. Mientras no los haya, no se mide: al cambiar de /sat 1 a
    // /sat 2 el bot tiene que entrenar obreros nuevos, y contar esos frames
    // mezclaria una economia de 9 obreros con una de 18. Si la configuracion se
    // rompe a mitad de camino (muere un obrero), la ventana se descarta y vuelve
    // a empezar, porque ya no mide lo mismo.
    void onFrame(bool configReady);

    // patches  = campos de minerales de nuestra base
    // workers  = obreros efectivamente asignados a un campo
    void draw(int patches, int workers) const;

    bool   isMeasuring() const { return m_windowStartFrame >= 0; }
    int    getElapsedFrames() const;
    int    getGathered() const;

    // Tasa en minerales por cada 1000 frames. Se usa frames y no segundos porque
    // el frame es la unidad del juego y no depende de la velocidad configurada.
    double getRatePer1000Frames() const;
};
