#pragma once

#include "MapTools.h"
#include "MineralField.h"
#include "SaturationMeter.h"

#include <BWAPI.h>
#include <unordered_map>
#include <string>
#include <vector>

class StarterBot
{
    // Radio en pixeles alrededor del centro de mando dentro del cual consideramos
    // que un campo de minerales pertenece a NUESTRA base. Solo medimos sobre esos:
    // los de otras bases no estan bajo nuestro control. Ajustable si el mapa tiene
    // yacimientos mas extendidos.
    static const int BASE_RADIUS = 400;

    // Pregunta 2: la saturacion es un fenomeno de cada campo, no de la base
    // entera, asi que se mide sobre los campos mas cercanos al centro de mando
    // en vez de sobre todos. Con 3 campos la configuracion mas cara (3 por campo)
    // pide 9 obreros: cabe en el suministro inicial de las tres razas y el
    // experimento completo dura minutos en vez de un cuarto de hora.
    static const int SATURATION_PATCHES = 3;

    MapTools m_mapTools;

    // Un MineralField por cada mineral del mapa, indexado por el ID de la unidad
    std::unordered_map<int, MineralField> m_mineralFields;

    // Modo medicion: el reparto de obreros lo controlamos nosotros, con un tope
    // de obreros por campo, en vez del comportamiento original de STARTcraft.
    // Se alterna en el juego escribiendo /medir.
    bool m_measurementMode = true;

    // Asignacion fija obrero -> mineral mientras dura el modo medicion
    std::unordered_map<int, int> m_workerAssignment;

    // Pregunta 2: medicion de saturacion. Apagada al empezar la partida, para
    // que la Pregunta 1 mida con un obrero por campo; se enciende con /sat auto
    // o /sat N.
    SaturationMeter m_saturation;
    bool m_saturationActive = false;

    // Ranking de eficiencia guardado de partidas anteriores, para este mapa, esta
    // raza y este lado del mapa. Se carga al empezar la partida y decide a que
    // campos van primero los obreros.
    struct RankEntry
    {
        BWAPI::TilePosition tile;
        double average = 0.0;   // frames entre bajadas
        int    samples = 0;     // muestras acumuladas en todas las partidas
    };
    std::vector<RankEntry> m_savedRanking;
    std::string m_rankingFile;          // vacio hasta que se sabe de que lado estamos

    // Bonus: el split. Durante los primeros SPLIT_FRAMES de cada partida en 4Retro
    // se reparten solo los 4 obreros iniciales, sin entrenar mas, con una de tres
    // politicas, y se anota cuanto recolectaron. La politica rota sola de una
    // partida a la siguiente.
    enum SplitPolicy { SPLIT_CHURCHILL = 0, SPLIT_DISTANCIA = 1, SPLIT_EFICIENCIA = 2, SPLIT_NONE = -1 };
    static const int SPLIT_FRAMES = 1000;
    int  m_splitPolicy = SPLIT_NONE;
    bool m_splitDone = false;
    bool m_ignoreRanking = false;       // true mientras el split reparte por distancia

public:

    StarterBot();

    // helper functions to get you started with bot programming and learn the API
    void sendIdleWorkersToMinerals();
    void trainAdditionalWorkers();
    void buildAdditionalSupply();
    void drawDebugInformation();
    void updateMineralFields();
    void drawMineralRanking();
    void assignWorkersToMinerals();
    std::vector<BWAPI::Unit> getBaseMinerals() const;
    std::vector<BWAPI::Unit> getMeasuredMinerals() const;
    int efficiencyRank(BWAPI::Unit mineral) const;
    void loadSavedRanking();
    void writeBaseRanking() const;
    bool splitRunning() const;
    void finishSplit();
    void drawSplit() const;

    // functions that are triggered by various BWAPI events from main.cpp
	void onStart();
	void onFrame();
	void onEnd(bool isWinner);
	void onUnitDestroy(BWAPI::Unit unit);
	void onUnitMorph(BWAPI::Unit unit);
	void onSendText(std::string text);
	void onUnitCreate(BWAPI::Unit unit);
	void onUnitComplete(BWAPI::Unit unit);
	void onUnitShow(BWAPI::Unit unit);
	void onUnitHide(BWAPI::Unit unit);
	void onUnitRenegade(BWAPI::Unit unit);
};