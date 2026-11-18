#pragma once

#include <BWAPI.h>
#include <vector>

// Pregunta 2 en 4Retro: saturacion medida en paralelo.
//
// En 4Retro la partida dura unos 2 minutos, que no alcanzan para medir 1, 2 y 3
// obreros por campo una configuracion detras de otra. Aqui se miden las tres a
// la vez: tres campos de la base reciben 1, 2 y 3 obreros, y el ingreso de cada
// campo se calcula con lo que baja su cantidad de minerales durante la ventana.
//
// Los tres campos no estan a la misma distancia, asi que la asignacion rota de
// una partida a la siguiente (cuadrado latino): en tres partidas cada cantidad
// de obreros pasa por cada campo y la diferencia de distancia se compensa.
class PatchSaturationTest
{
public:
    // Frames que se descartan despues de reunir los 6 obreros, para que lleguen a
    // su campo antes de empezar a contar.
    static const int WARMUP_FRAMES = 300;

    // Duracion de la ventana. Al cumplirse se anota el resultado y la medicion
    // termina, sin esperar al fin de la partida: si se esperara, el ataque final
    // del rival mata obreros, rompe la configuracion y se perderia todo.
    static const int MEASURE_FRAMES = 3000;

    // Si la configuracion se rompe con al menos esta ventana ya medida, se anota
    // como resultado parcial en vez de descartarla.
    static const int MIN_PARTIAL_FRAMES = 1000;

    // Prepara la medicion sobre tres campos. rotation (0, 1 o 2) decide que campo
    // recibe 1, 2 y 3 obreros.
    void setup(const std::vector<BWAPI::Unit>& patches, int rotation);

    bool isSetUp() const { return !m_patches.empty(); }
    int  getRotation() const { return m_rotation; }

    // Tope de obreros para un campo: el que le toca en esta partida, o 0 si el
    // campo no participa en la medicion.
    int  capFor(BWAPI::Unit mineral) const;
    int  totalWorkers() const;

    // Llamar una vez por frame. configReady indica si cada campo tiene
    // exactamente sus obreros; si se rompe, la ventana se descarta y se reinicia.
    void onFrame(bool configReady);

    // Panel en pantalla y texto sobre cada campo medido.
    void draw() const;

    // Anota en saturacion_4retro.txt una linea por campo con lo medido.
    void writeResults(const char* motivo) const;

    bool isFinished() const { return m_finished; }

    // Cuantas partidas de saturacion con datos hay ya anotadas en el archivo.
    // Decide la rotacion de la siguiente, para que las tres salgan aunque alguna
    // partida se pierda sin datos.
    static int countRecordedGames();

private:
    struct Patch
    {
        BWAPI::Unit unit = nullptr;
        int workers = 0;
        int startResources = 0;   // cantidad al abrir la ventana
        int endResources = 0;     // cantidad al cerrar la ventana
    };

    std::vector<Patch> m_patches;
    int m_rotation = 0;
    int m_readySince = -1;        // frame desde el que la configuracion esta completa
    int m_windowStart = -1;       // frame en que se abrio la ventana, -1 si no hay
    int m_windowEnd = -1;         // frame en que se cerro, -1 mientras sigue abierta
    bool m_finished = false;

    void finish(const char* motivo);

    int    gathered(const Patch& p) const;
    double ratePer1000(const Patch& p) const;
    int    elapsed() const;
};
