#pragma once

#include "MapTools.h"
#include "MineralField.h"

#include <BWAPI.h>
#include <unordered_map>

class StarterBot
{
    MapTools m_mapTools;

    // Un MineralField por cada mineral del mapa, indexado por el ID de la unidad
    std::unordered_map<int, MineralField> m_mineralFields;

    // Modo medicion: a lo mas UN obrero por mineral, para que el intervalo medido
    // dependa del campo y no de cuantos obreros lo estan golpeando. Se alterna
    // en el juego escribiendo /medir.
    bool m_measurementMode = true;

    // Asignacion fija obrero -> mineral mientras dura el modo medicion
    std::unordered_map<int, int> m_workerAssignment;

public:

    StarterBot();

    // helper functions to get you started with bot programming and learn the API
    void sendIdleWorkersToMinerals();
    void trainAdditionalWorkers();
    void buildAdditionalSupply();
    void drawDebugInformation();
    void updateMineralFields();
    void drawMineralRanking();
    void assignOneWorkerPerMineral();

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