#include "StarterBot.h"
#include "Tools.h"
#include "MapTools.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace
{
    // Campos mas eficientes de 4Retro1.2_1.16-Backport.scx para Terran, de mas a
    // menos eficiente. El mapa y la raza no cambian, asi que el ranking medido es
    // un dato fijo y va como constante.
    //
    // Origen: ranking de la Pregunta 1, formacion izquierda (nuestra base), un
    // obrero por campo, partida del 18/11/2026. Entre parentesis, el promedio de
    // frames entre bajadas y las muestras. Los dos primeros estan a 2 frames uno
    // del otro, dentro del ruido; entre el 2 y el 3 hay 16 frames de diferencia.
    // De esa captura solo se conocen los 4 primeros: el resto del orden sale de
    // ranking_minerales.txt.
    const BWAPI::TilePosition RETRO_TERRAN_IZQUIERDA[] =
    {
        BWAPI::TilePosition(3, 11),   // 153.56 frames, 9 muestras
        BWAPI::TilePosition(4, 13),   // 155.54 frames, 13 muestras
        BWAPI::TilePosition(6, 13),   // 171.75 frames, 12 muestras
        BWAPI::TilePosition(3, 8),    // 173.25 frames, 12 muestras
    };
    const int RETRO_TERRAN_IZQUIERDA_N = sizeof(RETRO_TERRAN_IZQUIERDA) / sizeof(RETRO_TERRAN_IZQUIERDA[0]);
}

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
    if (m_measurementMode && m_saturationActive)
    {
        const int campos = (int)getMeasuredMinerals().size();
        const int asignados = (int)m_workerAssignment.size();
        const int objetivo = campos * m_saturation.getWorkersPerMineral();
        const bool configCompleta = (objetivo > 0) && (asignados == objetivo);
        m_saturation.onFrame(configCompleta, campos, asignados);
    }

    // Train more workers so we can gather more income
    trainAdditionalWorkers();

    // Build more supply if we are going to run out soon. Durante la Pregunta 2
    // no: los 9 obreros caben en el suministro inicial, y construir obliga a
    // sacar un obrero de su campo, lo que rompe la configuracion que se mide.
    if (!(m_measurementMode && m_saturationActive)) { buildAdditionalSupply(); }

    // Medir los frames entre extracciones de cada campo de minerales
    updateMineralFields();

    // Cada 500 frames se reescribe el ranking completo de la base en un archivo,
    // para que quede aunque la partida termine de golpe.
    if (m_measurementMode && !m_saturationActive && BWAPI::Broodwar->getFrameCount() % 500 == 0)
    {
        writeBaseRanking();
    }

    // Draw unit health bars, which brood war unfortunately does not do
    Tools::DrawUnitHealthBars();

    // Ranking de los 4 minerales mas eficientes por formacion (para la exposicion)
    drawMineralRanking();

    // Panel de la Pregunta 2. Contamos los obreros ASIGNADOS, no todos los que
    // tenemos: los que sobran estan parados y no aportan a la tasa.
    if (m_measurementMode && m_saturationActive)
    {
        m_saturation.draw((int)getMeasuredMinerals().size(), (int)m_workerAssignment.size());
    }
    else if (m_measurementMode)
    {
        BWAPI::Broodwar->drawTextScreen(BWAPI::Position(10, 160),
            "Pregunta 2 apagada. Escribe /sat auto para medir saturacion.");
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

// Campos sobre los que se reparten los obreros. En la Pregunta 1 son todos los
// de la base. En la Pregunta 2 son los SATURATION_PATCHES mas cercanos al centro
// de mando: siempre los mismos durante toda la partida, de modo que las tres
// configuraciones se comparan sobre los mismos campos y la distancia no cambia.
std::vector<BWAPI::Unit> StarterBot::getMeasuredMinerals() const
{
    std::vector<BWAPI::Unit> campos = getBaseMinerals();
    if (!m_saturationActive) { return campos; }

    const BWAPI::Unit depot = Tools::GetDepot();
    if (!depot) { return campos; }

    std::sort(campos.begin(), campos.end(), [depot](BWAPI::Unit a, BWAPI::Unit b)
    {
        const int da = depot->getDistance(a);
        const int db = depot->getDistance(b);
        if (da != db) { return da < db; }
        return a->getID() < b->getID();   // desempate fijo, para no alternar campos
    });

    if ((int)campos.size() > SATURATION_PATCHES) { campos.resize(SATURATION_PATCHES); }
    return campos;
}

// Posicion del campo en el ranking de eficiencia (0 = el mejor), o -1 si el
// campo no esta en el ranking o si la partida no es Terran en 4Retro. La
// posicion se compara igual que en el ranking en pantalla: el tile del centro.
int StarterBot::efficiencyRank(BWAPI::Unit mineral) const
{
    if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran) { return -1; }
    if (BWAPI::Broodwar->mapFileName().find("4Retro") == std::string::npos) { return -1; }

    const BWAPI::TilePosition tile(mineral->getPosition());
    for (int i = 0; i < RETRO_TERRAN_IZQUIERDA_N; i++)
    {
        if (RETRO_TERRAN_IZQUIERDA[i] == tile) { return i; }
    }
    return -1;
}

// Escribe el ranking completo de los campos de NUESTRA base en
// ranking_minerales.txt, de mas a menos eficiente. En pantalla solo caben 4;
// este archivo da el orden completo para completar las constantes.
void StarterBot::writeBaseRanking() const
{
    std::vector<const MineralField*> campos;
    for (const BWAPI::Unit mineral : getBaseMinerals())
    {
        const auto it = m_mineralFields.find(mineral->getID());
        if (it != m_mineralFields.end() && it->second.getSamples() >= 3) { campos.push_back(&it->second); }
    }
    std::sort(campos.begin(), campos.end(), [](const MineralField* a, const MineralField* b)
    {
        return a->getAverage() < b->getAverage();
    });

    std::FILE* f = std::fopen("ranking_minerales.txt", "w");
    if (!f) { return; }

    const BWAPI::Unit depot = Tools::GetDepot();
    std::fprintf(f, "Mapa %s, raza %s, frame %d, base %s\n",
                 BWAPI::Broodwar->mapFileName().c_str(),
                 BWAPI::Broodwar->self()->getRace().getName().c_str(),
                 BWAPI::Broodwar->getFrameCount(),
                 !depot ? "?" : (depot->getPosition().x < BWAPI::Broodwar->mapWidth() * 16 ? "izquierda" : "derecha"));

    for (size_t i = 0; i < campos.size(); i++)
    {
        const BWAPI::TilePosition tile(campos[i]->getPosition());
        std::fprintf(f, "%d) tile (%d,%d)  prom %.2f  (%d muestras)\n",
                     (int)i + 1, tile.x, tile.y, campos[i]->getAverage(), campos[i]->getSamples());
    }
    std::fclose(f);
}

// Reparte los obreros sobre los campos de la base, con un tope de N obreros por
// campo. N lo fija el experimento de saturacion (/sat N); con N=1 cada campo se
// mide aislado, que es lo que necesita la Pregunta 1.
void StarterBot::assignWorkersToMinerals()
{
    const int perMineral = m_saturationActive ? m_saturation.getWorkersPerMineral() : 1;
    const std::vector<BWAPI::Unit> baseMinerals = getMeasuredMinerals();
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

        // Entre los campos con cupo libre: primero los del ranking de eficiencia,
        // en su orden; si ninguno tiene cupo, el MAS CERCANO a este obrero.
        BWAPI::Unit mejorMineral = nullptr;
        int mejorRango = std::numeric_limits<int>::max();
        int mejorDistancia = std::numeric_limits<int>::max();

        for (const BWAPI::Unit mineral : baseMinerals)
        {
            if (ocupacion[mineral->getID()] >= perMineral) { continue; }

            const int rango = efficiencyRank(mineral);
            const int clave = (rango >= 0) ? rango : std::numeric_limits<int>::max();
            const int distancia = worker->getDistance(mineral);

            if (clave < mejorRango || (clave == mejorRango && distancia < mejorDistancia))
            {
                mejorRango = clave;
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

    // 4) Un obrero sin asignacion que siga minando se detiene: pasa al cambiar de
    //    la Pregunta 1 a la 2, cuando los obreros de los demas campos se quedan
    //    sin campo. Si siguiera minando sumaria a la tasa sin estar contado.
    for (const BWAPI::Unit worker : BWAPI::Broodwar->self()->getUnits())
    {
        if (!worker->getType().isWorker()) { continue; }
        if (m_workerAssignment.count(worker->getID())) { continue; }
        if (worker->isGatheringMinerals()) { worker->stop(); }
    }

    // 5) Si un obrero asignado quedo quieto, lo volvemos a mandar a SU mineral
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
    //
    // En la Pregunta 2 se entrenan desde el inicio los obreros de la configuracion
    // mas grande. Los que sobran esperan quietos y no suman, y asi la fase 3 no
    // tiene que esperar a que se entrenen: se entrenan mientras corre la fase 1.
    int workersWanted = 20;
    if (m_measurementMode)
    {
        workersWanted = m_saturationActive
            ? (int)getMeasuredMinerals().size() * m_saturation.getMaxWorkersPerMineral()
            : (int)getBaseMinerals().size();
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

    // Se anota en el archivo de resultados en que frame termino la partida, para
    // saber cuanto tiempo de juego hay disponible en cada mapa.
    if (m_saturationActive)
    {
        SaturationMeter::appendLog("Fin de partida en el frame %d (%s)",
                                   BWAPI::Broodwar->getFrameCount(), isWinner ? "victoria" : "derrota");
    }
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
    else if (text == "/sat auto")
    {
        // Reinicia el recorrido automatico 1 -> 2 -> 3 desde cero.
        // Va ANTES que la rama de /sat N, porque "/sat auto" tambien empieza
        // con "/sat " y si no se comprueba primero nunca se alcanzaria.
        m_saturationActive = true;
        m_saturation.restartAuto();
        m_workerAssignment.clear();
        BWAPI::Broodwar->printf("Saturacion: recorrido automatico 1, 2, 3 sobre los %d campos mas cercanos",
                                SATURATION_PATCHES);
    }
    else if (text.rfind("/sat ", 0) == 0)
    {
        // /sat N fija N obreros por campo a mano y abandona el automatico.
        const int n = std::atoi(text.substr(5).c_str());
        if (n >= 1 && n <= 10)
        {
            m_saturationActive = true;
            m_saturation.setWorkersPerMineral(n);
            m_workerAssignment.clear();   // el reparto se rehace con el nuevo tope
            BWAPI::Broodwar->printf("Saturacion: %d obrero(s) por campo. Medicion reiniciada.", n);
        }
        else
        {
            BWAPI::Broodwar->printf("Uso: /sat N (N entre 1 y 10), o /sat auto");
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