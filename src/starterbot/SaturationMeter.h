#pragma once

#include <BWAPI.h>
#include <vector>

// Resultado de una fase del experimento: una configuracion medida de principio a fin.
struct SaturationResult
{
    int    workersPerMineral = 0;
    int    patches = 0;
    int    workers = 0;
    int    frames = 0;
    int    gathered = 0;
    double rate = 0.0;        // minerales por cada 1000 frames
    double perWorker = 0.0;   // lo mismo, dividido entre los obreros
    bool   ok = true;         // false si la fase se abandono sin poder medir
};

// Pregunta 2: mide el INGRESO DE MINERALES bajo distintas cantidades de obreros
// por campo, para responder cuantos obreros por mineral conviene poner.
//
// El total recolectado siempre sube al agregar obreros, porque hay mas gente
// trabajando; eso no prueba nada. Lo que interesa es el aporte POR OBRERO, que
// cae conforme los obreros empiezan a hacer cola sobre el mismo campo. Ese punto
// donde sumar un obrero ya casi no aporta es la saturacion.
//
// Por defecto el experimento corre solo: pasa por 1, 2 y 3 obreros por campo,
// mide cada configuracion y al terminar deja la tabla en pantalla. Con /sat N se
// puede fijar una configuracion a mano.
class SaturationMeter
{
    // Frames que se descartan tras completar una configuracion. Aunque ya esten
    // todos los obreros, tienen que caminar hasta su campo antes de que la tasa
    // signifique algo.
    static const int WARMUP_FRAMES = 500;

    // Duracion de la ventana de medicion de cada configuracion. Ajustada hacia
    // abajo a proposito: el bot no se defiende, y entrenar hasta 27 obreros ya
    // consume miles de frames, asi que ventanas largas hacen que el rival llegue
    // antes de terminar la tercera fase. Con 3000 frames la tasa ya es estable
    // (son varias decenas de viajes por obrero) y el experimento cabe.
    static const int MEASURE_FRAMES = 3000;

    // Si una configuracion no logra reunir sus obreros en este tiempo (suministro
    // bloqueado, obreros muriendo), se abandona y se pasa a la siguiente, en vez
    // de dejar el experimento colgado sin explicacion.
    static const int STALL_FRAMES = 15000;

    std::vector<int> m_plan;              // configuraciones a recorrer: 1, 2, 3
    std::vector<SaturationResult> m_results;

    size_t m_phase = 0;
    bool   m_auto = true;
    bool   m_finished = false;
    bool   m_started = false;   // false hasta la primera llamada dentro de la partida

    int m_workersPerMineral = 1;
    int m_phaseStartFrame = 0;      // frame en que arranco la fase actual
    int m_readySince = -1;          // frame desde el que la configuracion esta completa
    int m_windowStartFrame = -1;    // -1 mientras no hay ventana abierta
    int m_windowStartMinerals = 0;

    void beginPhase(size_t phase);
    void recordAndAdvance(int patches, int workers, bool ok);

public:

    SaturationMeter();

    int  getWorkersPerMineral() const { return m_workersPerMineral; }

    // Obreros por campo de la configuracion mas grande que falta medir. Sirve
    // para entrenar por adelantado y que ninguna fase espere por obreros.
    int  getMaxWorkersPerMineral() const;

    // Agrega una linea al archivo de resultados (saturacion_resultados.txt, en la
    // carpeta desde la que se lanza el bot). Asi los numeros no dependen de
    // alcanzar a tomar una captura antes de que termine la partida.
    static void appendLog(const char* format, ...);

    // Fija una configuracion a mano y abandona el recorrido automatico.
    void setWorkersPerMineral(int n);

    // Reinicia el recorrido automatico desde la primera configuracion.
    void restartAuto();

    // Llamar una vez por frame.
    //
    // configReady indica si en este frame hay exactamente los obreros que pide la
    // configuracion. Mientras no los haya no se mide: al pasar de 1 a 2 obreros
    // por campo el bot tiene que entrenar mas, y contar esos frames mezclaria una
    // economia de 9 obreros con una de 18. Si la configuracion se rompe a mitad
    // de camino, la ventana se descarta y la fase vuelve a empezar.
    void onFrame(bool configReady, int patches, int workers);

    void draw(int patches, int workers) const;

    int    getElapsedFrames() const;
    int    getGathered() const;
    double getRatePer1000Frames() const;
};
