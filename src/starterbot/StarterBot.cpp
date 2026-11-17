#include "StarterBot.h"
#include "Tools.h"
#include "MapTools.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

StarterBot::StarterBot()
{
    
}

// Called when the bot starts!
void StarterBot::onStart()
{
    // Set our BWAPI options here    
	BWAPI::Broodwar->setLocalSpeed(10);
    BWAPI::Broodwar->setFrameSkip(0);
    
    // Enable the flag that tells BWAPI top let users enter input while bot plays
    BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);

    // Este bot solo mide datos: con informacion completa del mapa vemos TODOS los
    // minerales (no solo los 9 cercanos a la base) y su cantidad en todo momento.
    BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);

    // Call MapTools OnStart
    m_mapTools.onStart();
}

// Called on each frame of the game
void StarterBot::onFrame()
{
    // Update our MapTools information
    m_mapTools.onFrame();

    // En modo medicion repartimos un obrero por mineral; si no, el comportamiento
    // original de STARTcraft (cada obrero al mineral mas cercano).
    if (m_measurementMode) { assignWorkersToMinerals(); }
    else                   { sendIdleWorkersToMinerals(); }

    // Pregunta 2: solo medimos cuando estan exactamente los obreros que pide la
    // configuracion, ni uno mas ni uno menos.
    const int objetivo = (int)getBaseMinerals().size() * m_saturation.getWorkersPerMineral();
    const bool configCompleta = (objetivo > 0) && ((int)m_workerAssignment.size() == objetivo);
    m_saturation.onFrame(configCompleta);

    // Train more workers so we can gather more income
    trainAdditionalWorkers();

    // Build more supply if we are going to run out soon
    buildAdditionalSupply();

    // Medir los frames entre extracciones de cada campo de minerales
    updateMineralFields();

    // Draw unit health bars, which brood war unfortunately does not do
    Tools::DrawUnitHealthBars();

    // Ranking de los 4 minerales mas eficientes por formacion (para la exposicion)
    drawMineralRanking();

    // Panel de la Pregunta 2. Contamos los obreros ASIGNADOS, no todos los que
    // tenemos: los que sobran estan parados y no aportan a la tasa.
    if (m_measurementMode)
    {
        m_saturation.draw((int)getBaseMinerals().size(), (int)m_workerAssignment.size());
    }

    // Draw some relevent information to the screen to help us debug the bot
    drawDebugInformation();
}

// Modo medicion: a lo mas UN obrero por mineral.
//
// Por que: el intervalo entre bajadas de 8 depende de CUANTOS obreros golpean el
// campo, no solo de que tan lejos esta. Con 3 obreros en un mineral las bajadas
// llegan ~3 veces mas seguido y ese campo parece "mas eficiente" sin serlo. Para
// comparar campos entre si hay que fijar un obrero por campo.
// Campos de minerales de NUESTRA base: los que estan a menos de BASE_RADIUS del
// centro de mando. El experimento solo tiene sentido sobre estos, porque son los
// unicos cuyo numero de obreros controlamos.
std::vector<BWAPI::Unit> StarterBot::getBaseMinerals() const
{
    std::vector<BWAPI::Unit> resultado;

    const BWAPI::Unit depot = Tools::GetDepot();
    if (!depot) { return resultado; }

    for (const BWAPI::Unit mineral : BWAPI::Broodwar->getMinerals())
    {
        if (depot->getDistance(mineral) < BASE_RADIUS) { resultado.push_back(mineral); }
    }
    return resultado;
}

// Reparte los obreros sobre los campos de la base, con un tope de N obreros por
// campo. N lo fija el experimento de saturacion (/sat N); con N=1 cada campo se
// mide aislado, que es lo que necesita la Pregunta 1.
void StarterBot::assignWorkersToMinerals()
{
    const int perMineral = m_saturation.getWorkersPerMineral();
    const std::vector<BWAPI::Unit> baseMinerals = getBaseMinerals();
    if (baseMinerals.empty()) { return; }

    // 1) Limpiar asignaciones cuyo obrero o mineral ya no existe
    for (auto it = m_workerAssignment.begin(); it != m_workerAssignment.end(); )
    {
        const BWAPI::Unit worker  = BWAPI::Broodwar->getUnit(it->first);
        const BWAPI::Unit mineral = BWAPI::Broodwar->getUnit(it->second);

        if (!worker || !worker->exists() || !mineral || !mineral->exists())
        {
            it = m_workerAssignment.erase(it);
        }
        else { ++it; }
    }

    // 2) Cuantos obreros tiene ya asignados cada campo
    std::unordered_map<int, int> ocupacion;
    for (const auto& pair : m_workerAssignment) { ocupacion[pair.second]++; }

    // 3) Repartir los obreros sin asignacion a campos que no llegan al tope
    for (const BWAPI::Unit worker : BWAPI::Broodwar->self()->getUnits())
    {
        if (!worker->getType().isWorker()) { continue; }
        if (m_workerAssignment.count(worker->getID())) { continue; }

        // Entre los campos con cupo libre, el MAS CERCANO a este obrero.
        BWAPI::Unit mejorMineral = nullptr;
        int mejorDistancia = std::numeric_limits<int>::max();

        for (const BWAPI::Unit mineral : baseMinerals)
        {
            if (ocupacion[mineral->getID()] >= perMineral) { continue; }

            const int distancia = worker->getDistance(mineral);
            if (distancia < mejorDistancia)
            {
                mejorDistancia = distancia;
                mejorMineral = mineral;
            }
        }

        // Sin cupo libre el obrero se queda quieto a proposito: es un obrero de
        // mas para esta configuracion y ponerlo a minar falsearia la tasa.
        if (mejorMineral)
        {
            m_workerAssignment[worker->getID()] = mejorMineral->getID();
            ocupacion[mejorMineral->getID()]++;
            worker->rightClick(mejorMineral);
        }
    }

    // 4) Si un obrero asignado quedo quieto, lo volvemos a mandar a SU mineral
    for (const auto& pair : m_workerAssignment)
    {
        const BWAPI::Unit worker  = BWAPI::Broodwar->getUnit(pair.first);
        const BWAPI::Unit mineral = BWAPI::Broodwar->getUnit(pair.second);

        if (worker && worker->exists() && worker->isIdle() && mineral && mineral->exists())
        {
            worker->rightClick(mineral);
        }
    }
}

// Ranking de eficiencia: los 4 minerales con MENOR promedio de frames entre
// bajadas, separados por formacion izquierda / derecha del mapa.
void StarterBot::drawMineralRanking()
{
    // Minimo de conteos antes de rankear un mineral: con 1 o 2 muestras el
    // promedio todavia es ruido.
    const int MIN_SAMPLES = 3;

    // mapWidth() viene en build tiles de 32 px; el centro parte el mapa en dos.
    const int mapCenterX = (BWAPI::Broodwar->mapWidth() * 32) / 2;

    std::vector<const MineralField*> izquierda;
    std::vector<const MineralField*> derecha;

    for (const auto& pair : m_mineralFields)
    {
        const MineralField& field = pair.second;
        if (field.getSamples() < MIN_SAMPLES) { continue; }

        if (field.getPosition().x < mapCenterX) { izquierda.push_back(&field); }
        else                                    { derecha.push_back(&field); }
    }

    // Menor promedio = mas eficiente = primero
    const auto porPromedio = [](const MineralField* a, const MineralField* b)
    {
        return a->getAverage() < b->getAverage();
    };
    std::sort(izquierda.begin(), izquierda.end(), porPromedio);
    std::sort(derecha.begin(),   derecha.end(),   porPromedio);

    // La raza importa: el obrero de cada raza se mueve a distinta velocidad, asi
    // que el ranking hay que sacarlo en una partida por raza.
    const std::string raza = BWAPI::Broodwar->self()->getRace().getName();

    int y = 20;
    BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y), "TOP 4 MINERALES - raza: %s", raza.c_str());
    y += 20;

    const auto dibujarGrupo = [&](const char* titulo, const std::vector<const MineralField*>& grupo)
    {
        BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y), "%s", titulo);
        y += 10;

        if (grupo.empty())
        {
            BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y), "  (sin datos aun)");
            y += 10;
            return;
        }

        const int n = std::min((int)grupo.size(), 4);
        for (int i = 0; i < n; i++)
        {
            const MineralField* field = grupo[i];
            const BWAPI::TilePosition tile(field->getPosition());

            BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, y),
                "  %d) tile (%d,%d)  prom %.2f  (%d muestras)",
                i + 1, tile.x, tile.y, field->getAverage(), field->getSamples());
            y += 10;
        }
    };

    // De que lado esta NUESTRA base. Importa para leer los numeros: en nuestra
    // formacion controlamos la asignacion (un obrero por mineral), pero en la
    // del otro jugador no sabemos cuantos obreros golpean cada campo, asi que
    // sus promedios bajan solos conforme el rival suma obreros y no son
    // comparables con los nuestros.
    const BWAPI::Unit depot = Tools::GetDepot();
    const bool tenemosBase = (depot != nullptr);
    const bool nuestraEsIzquierda = tenemosBase && (depot->getPosition().x < mapCenterX);

    const char* etiquetaIzq = !tenemosBase     ? ""
                            : nuestraEsIzquierda ? " (nuestra base)"
                                                 : " (otro jugador)";
    const char* etiquetaDer = !tenemosBase     ? ""
                            : nuestraEsIzquierda ? " (otro jugador)"
                                                 : " (nuestra base)";

    char tituloIzq[64];
    char tituloDer[64];
    snprintf(tituloIzq, sizeof(tituloIzq), "FORMACION IZQUIERDA%s", etiquetaIzq);
    snprintf(tituloDer, sizeof(tituloDer), "FORMACION DERECHA%s", etiquetaDer);

    dibujarGrupo(tituloIzq, izquierda);
    y += 5;
    dibujarGrupo(tituloDer, derecha);

    // Marcar en el mapa los 4 mejores de cada formacion con un circulo
    const auto marcar = [](const std::vector<const MineralField*>& grupo)
    {
        const int n = std::min((int)grupo.size(), 4);
        for (int i = 0; i < n; i++)
        {
            BWAPI::Broodwar->drawCircleMap(grupo[i]->getPosition(), 20, BWAPI::Colors::Green);
        }
    };
    marcar(izquierda);
    marcar(derecha);
}

// Pregunta 1: actualiza y dibuja el contador de cada campo de minerales
void StarterBot::updateMineralFields()
{
    for (BWAPI::Unit mineral : BWAPI::Broodwar->getMinerals())
    {
        // Si es la primera vez que vemos este mineral, creamos su MineralField
        auto it = m_mineralFields.find(mineral->getID());
        if (it == m_mineralFields.end())
        {
            it = m_mineralFields.emplace(mineral->getID(), MineralField(mineral)).first;
        }

        it->second.onFrame();
        it->second.draw();
    }
}

// Send our idle workers to mine minerals so they don't just stand there
void StarterBot::sendIdleWorkersToMinerals()
{
    // Let's send all of our starting workers to the closest mineral to them
    // First we need to loop over all of the units that we (BWAPI::Broodwar->self()) own
    const BWAPI::Unitset& myUnits = BWAPI::Broodwar->self()->getUnits();
    for (auto& unit : myUnits)
    {
        // Check the unit type, if it is an idle worker, then we want to send it somewhere
        if (unit->getType().isWorker() && unit->isIdle())
        {
            // Get the closest mineral to this worker unit
            BWAPI::Unit closestMineral = Tools::GetClosestUnitTo(unit, BWAPI::Broodwar->getMinerals());

            // If a valid mineral was found, right click it with the unit in order to start harvesting
            if (closestMineral) { unit->rightClick(closestMineral); }
        }
    }
}

// Train more workers so we can gather more income
void StarterBot::trainAdditionalWorkers()
{
    const BWAPI::UnitType workerType = BWAPI::Broodwar->self()->getRace().getWorker();

    // En modo medicion el numero de obreros es parte del experimento, no una
    // decision libre: queremos exactamente (campos de la base) x (obreros por
    // campo). Un obrero de mas mina por su cuenta y falsea la tasa.
    int workersWanted = 20;
    if (m_measurementMode)
    {
        workersWanted = (int)getBaseMinerals().size() * m_saturation.getWorkersPerMineral();
    }

    const int workersOwned = Tools::CountUnitsOfType(workerType, BWAPI::Broodwar->self()->getUnits());
    if (workersOwned < workersWanted)
    {
        // get the unit pointer to my depot
        const BWAPI::Unit myDepot = Tools::GetDepot();

        // if we have a valid depot unit and it's currently not training something, train a worker
        // there is no reason for a bot to ever use the unit queueing system, it just wastes resources
        if (myDepot && !myDepot->isTraining()) { myDepot->train(workerType); }
    }
}

// Build more supply if we are going to run out soon
void StarterBot::buildAdditionalSupply()
{
    // Get the amount of supply supply we currently have unused
    const int unusedSupply = Tools::GetTotalSupply(true) - BWAPI::Broodwar->self()->supplyUsed();

    // If we have a sufficient amount of supply, we don't need to do anything
    if (unusedSupply >= 2) { return; }

    // Otherwise, we are going to build a supply provider
    const BWAPI::UnitType supplyProviderType = BWAPI::Broodwar->self()->getRace().getSupplyProvider();

    const bool startedBuilding = Tools::BuildBuilding(supplyProviderType);
    if (startedBuilding)
    {
        BWAPI::Broodwar->printf("Started Building %s", supplyProviderType.getName().c_str());
    }
}

// Draw some relevent information to the screen to help us debug the bot
void StarterBot::drawDebugInformation()
{
    BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, 10), "Hello, World!\n");
    Tools::DrawUnitCommands();
    Tools::DrawUnitBoundingBoxes();
}

// Called whenever the game ends and tells you if you won or not
void StarterBot::onEnd(bool isWinner)
{
    std::cout << "We " << (isWinner ? "won!" : "lost!") << "\n";
}

// Called whenever a unit is destroyed, with a pointer to the unit
void StarterBot::onUnitDestroy(BWAPI::Unit unit)
{
    // Un mineral agotado desaparece del mapa: liberamos su MineralField
    if (unit->getType().isMineralField())
    {
        m_mineralFields.erase(unit->getID());
    }

    // Si muere un obrero, soltamos su asignacion para que otro tome ese mineral
    m_workerAssignment.erase(unit->getID());
}

// Called whenever a unit is morphed, with a pointer to the unit
// Zerg units morph when they turn into other units
void StarterBot::onUnitMorph(BWAPI::Unit unit)
{
	
}

// Called whenever a text is sent to the game by a user
void StarterBot::onSendText(std::string text) 
{ 
    if (text == "/map")
    {
        m_mapTools.toggleDraw();
    }
    else if (text == "/medir")
    {
        // Alterna entre nuestro reparto controlado y el original de STARTcraft
        m_measurementMode = !m_measurementMode;
        m_workerAssignment.clear();
        BWAPI::Broodwar->printf("Modo medicion: %s", m_measurementMode ? "ON" : "OFF");
    }
    else if (text.rfind("/sat ", 0) == 0)
    {
        // /sat N fija N obreros por campo y reinicia la medicion de saturacion.
        // Una corrida por cada N: 1, 2, 3.
        const int n = std::atoi(text.substr(5).c_str());
        if (n >= 1 && n <= 10)
        {
            m_saturation.setWorkersPerMineral(n);
            m_workerAssignment.clear();   // el reparto se rehace con el nuevo tope
            BWAPI::Broodwar->printf("Saturacion: %d obrero(s) por campo. Medicion reiniciada.", n);
        }
        else
        {
            BWAPI::Broodwar->printf("Uso: /sat N, con N entre 1 y 10");
        }
    }
}

// Called whenever a unit is created, with a pointer to the destroyed unit
// Units are created in buildings like barracks before they are visible, 
// so this will trigger when you issue the build command for most units
void StarterBot::onUnitCreate(BWAPI::Unit unit)
{ 
	
}

// Called whenever a unit finished construction, with a pointer to the unit
void StarterBot::onUnitComplete(BWAPI::Unit unit)
{
	
}

// Called whenever a unit appears, with a pointer to the destroyed unit
// This is usually triggered when units appear from fog of war and become visible
void StarterBot::onUnitShow(BWAPI::Unit unit)
{ 
	
}

// Called whenever a unit gets hidden, with a pointer to the destroyed unit
// This is usually triggered when units enter the fog of war and are no longer visible
void StarterBot::onUnitHide(BWAPI::Unit unit)
{ 
	
}

// Called whenever a unit switches player control
// This usually happens when a dark archon takes control of a unit
void StarterBot::onUnitRenegade(BWAPI::Unit unit)
{ 
	
}