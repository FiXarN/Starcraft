#include "ExampleAIModule.h" 
using namespace BWAPI;

bool analyzed;
bool analysis_just_finished;
BWTA::Region* home;
BWTA::Region* enemy_base;

//This is the startup method. It is called once
//when a new game has been started with the bot.
void ExampleAIModule::onStart()
{
	Broodwar->sendText("Hello world!");
	//Enable flags
	Broodwar->enableFlag(Flag::UserInput);
	//Uncomment to enable complete map information
	//Broodwar->enableFlag(Flag::CompleteMapInformation);

	//Start analyzing map data
	BWTA::readMap();
	analyzed = false;
	analysis_just_finished = false;
	//CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AnalyzeThread, NULL, 0, NULL); //Threaded version
	AnalyzeThread();

	//Send each worker to the mineral field that is closest to it
	gatherMinerals();
}

//Called when a game is ended.
//No need to change this.
void ExampleAIModule::onEnd(bool isWinner)
{
	if (isWinner)
	{
		Broodwar->sendText("I won!");
	}
}

//Finds a guard point around the home base.
//A guard point is the center of a chokepoint surrounding
//the region containing the home base.
Position ExampleAIModule::findGuardPoint()
{
	//Get the chokepoints linked to our home region
	std::set<BWTA::Chokepoint*> chokepoints = home->getChokepoints();
	double min_length = 10000;
	BWTA::Chokepoint* choke = NULL;

	//Iterate through all chokepoints and look for the one with the smallest gap (least width)
	for (std::set<BWTA::Chokepoint*>::iterator c = chokepoints.begin(); c != chokepoints.end(); c++)
	{
		double length = (*c)->getWidth();
		if (length < min_length || choke == NULL)
		{
			min_length = length;
			choke = *c;
		}
	}

	return choke->getCenter();
}

//This is the method called each frame. This is where the bot's logic
//shall be called.
void ExampleAIModule::onFrame()
{
	//https://bwapi.github.io/class_b_w_a_p_i_1_1_game.html#aeb27c5ae787797bd151fbb6998ee0d9d
	//https://bwapi.github.io/class_b_w_a_p_i_1_1_player_interface.html#acd1991823cf6aa521bd59c6c06766017
	//https://bwapi.github.io/class_b_w_a_p_i_1_1_unit_interface.html#a014c3fe2ee378af72919005534db3729
	//https://bwapi.github.io/class_b_w_a_p_i_1_1_tech_type.html
	//Call every 100:th frame
	if (Broodwar->getFrameCount() % 200 == 0)
	{
		//Make workers build
		for (auto u : Broodwar->self()->getUnits())
		{
			//Check if unit is a worker.
			if (u->getType().isWorker()) {
				//If 100 minerals reached, build supply depot
				if (u->canBuild(UnitTypes::Terran_Supply_Depot) && nrOfSupplyDepot < 2) {
					buildStuff(UnitTypes::Terran_Supply_Depot, u, 0);
				}
				//Build barrack
				else if (u->canBuild(UnitTypes::Terran_Barracks) && nrOfSupplyDepot == 2 && nrOfBarracks < 1) {
					buildStuff(UnitTypes::Terran_Barracks, u, 1);
				}
				//Build refinery
				else if (u->canBuild(UnitTypes::Terran_Refinery) && nrOfWorkers == 9 && nrOfRefinerys < 1) {
					buildStuff(UnitTypes::Terran_Refinery, u, 2);
				}
				//Build academy
				else if (u->canBuild(UnitTypes::Terran_Academy) && nrOfRefinerys == 1 && nrOfAcademys < 1) {
					buildStuff(UnitTypes::Terran_Academy, u, 3);
				}
				//Build factory
				else if (u->canBuild(UnitTypes::Terran_Factory) && nrOfMedics == 3 && nrOfFactorys < 1) {
					buildStuff(UnitTypes::Terran_Factory, u, 4);
				}
				//If not, gather
				else {
					//Two workers are gathering gas
					if (nrOfRefinerys > 0) {
						Unit refinery = NULL;
						for (auto findRefinery : Broodwar->self()->getUnits()) {
							if (findRefinery->getType().isRefinery()) {
								refinery = findRefinery;
							}
						}
						for (auto gatherGasUnit : Broodwar->self()->getUnits()) {
							if (gatherGasUnit->getType().isWorker() && gatherGasUnit->getID() != u->getID()) {
								gatherGasUnit->rightClick(refinery);
								break;
							}
						}
						if (u->getType().isWorker() && !u->isConstructing()) {
							u->rightClick(refinery);
						}
					}
					else {
						gatherMinerals();
					}
				}
				break;
			}
		}

		//Train units and send to choke point
		for (auto units : Broodwar->self()->getUnits()) {
			//Train marines
			if (units->getType() == UnitTypes::Terran_Barracks && nrOfMarines < 10) {
				if (units->canTrain(UnitTypes::Terran_Marine)) {
					units->train(UnitTypes::Terran_Marine);
					nrOfMarines++;
				}
			}
			//Move marines to choke point
			if (units->getType() == UnitTypes::Terran_Marine) {
				//Find guardPoint
				Position guardPos = findGuardPoint();
				//Move marines to guardPoint
				units->rightClick(guardPos);
			}
			//Train medics
			if (nrOfAcademys == 1 && (units->getType() == UnitTypes::Terran_Barracks && nrOfMedics < 3)) {
				if (units->canTrain(UnitTypes::Terran_Medic)) {
					units->train(UnitTypes::Terran_Medic);
					nrOfMedics++;
				}
			}
			//Move medics to choke point
			if (units->getType() == UnitTypes::Terran_Medic) {
				//Find guardPoint
				Position guardPos = findGuardPoint();
				//Move marines to guardPoint
				units->rightClick(guardPos);
			}
			//Train SCV and make them gather
			if (nrOfMarines == 10 && (units->getType() == UnitTypes::Terran_Command_Center && nrOfWorkers < 9)) {
				if (units->canTrain(UnitTypes::Terran_SCV)) {
					units->train(UnitTypes::Terran_SCV);
					nrOfWorkers++;
				}
				//Make the new workers gather gas ONLY
				if (units->getType().isWorker() && units->isBeingGathered() == false) {
					gatherMinerals();
				}
			}
			//Factory build add on
			if (units->getType() == UnitTypes::Terran_Factory) {
				if (units->canBuildAddon(UnitTypes::Terran_Machine_Shop)) {
					units->buildAddon(UnitTypes::Terran_Machine_Shop);
				}
				if (units->canTrain(UnitTypes::Terran_Siege_Tank_Tank_Mode) && nrOfSiegeTanks < 3) {
					units->train(UnitTypes::Terran_Siege_Tank_Tank_Mode);
				}
			}
			//Move tanks to choke point
			if (units->getType() == UnitTypes::Terran_Siege_Tank_Tank_Mode) {
				//Find guardPoint
				Position guardPos = findGuardPoint();
				//Move marines to guardPoint
				units->rightClick(guardPos);
			}
		}

		Broodwar->printf("NrOfMarines: %d", nrOfMarines);
		Broodwar->printf("NrOfWorkers: %d", nrOfWorkers);
		Broodwar->printf("NrOfMedics: %d", nrOfMedics);
		Broodwar->printf("-----------------------------");
	}

	//Draw lines around regions, chokepoints etc.
	if (analyzed)
	{
		drawTerrainData();
	}
}

void ExampleAIModule::gatherMinerals() {
	for (auto u : Broodwar->self()->getUnits())
	{
		if (u->getType().isWorker())
		{
			Unit closestMineral = NULL;
			for (auto m : Broodwar->getMinerals())
			{
				if (closestMineral == NULL || u->getDistance(m) < u->getDistance(closestMineral))
				{
					closestMineral = m;
				}
			}
			if (closestMineral != NULL)
			{
				u->rightClick(closestMineral);
				//Broodwar->printf("Send worker %d to mineral %d", u->getID(), closestMineral->getID());
			}
		}
	}
}

void ExampleAIModule::buildStuff(UnitType unitType, Unit unit, int incrementNrOf) {
	TilePosition buildingPos = Broodwar->getBuildLocation(unitType, unit->getTilePosition(), 300);
	if (Broodwar->canBuildHere(buildingPos, unitType) && unit->isConstructing() == false) {
		unit->build(unitType, buildingPos);
		if (unit->isConstructing()) {
			switch (incrementNrOf)
			{
			case 0: //Increase nrOf Supply
				nrOfSupplyDepot++;
				break;
			case 1: //Increase nrOf Barrack
				nrOfBarracks++;
				break;
			case 2: //Increase nrOf Refinery
				nrOfRefinerys++;
				break;
			case 3: //Increase nrOf Academy
				nrOfAcademys++;
				break;
			case 4:
				nrOfFactorys++;
				break;
			default:
				break;
			}
		}
	}
	else {
		Broodwar->printf("Can't build at this location");
		buildingPos = Broodwar->getBuildLocation(unitType, unit->getTilePosition(), 600);
		if (Broodwar->canBuildHere(buildingPos, unitType) && unit->isConstructing() == false) {
			unit->build(unitType, buildingPos);
			if (unit->isConstructing()) {
				switch (incrementNrOf)
				{
				case 0: //Increase nrOf Supply
					nrOfSupplyDepot++;
					break;
				case 1: //Increase nrOf Barrack
					nrOfBarracks++;
					break;
				case 2: //Increase nrOf Refinery
					nrOfRefinerys++;
					break;
				case 3: //Increase nrOf Academy
					nrOfAcademys++;
					break;
				case 4:
					nrOfFactorys++;
					break;
				default:
					break;
				}
			}
		}
	}
}

void ExampleAIModule::countUnits() {
	nrOfSupplyDepot = 0;
	nrOfBarracks = 0;
	nrOfMarines = 0;
	nrOfRefinerys = 0;
	nrOfAcademys = 0;
	nrOfWorkers = 0;
	nrOfMedics = 0;

	for (auto units : Broodwar->self()->getUnits()) {
		if (units->getType() == UnitTypes::Terran_Supply_Depot) {
			nrOfSupplyDepot++;
		}
		else if (units->getType() == UnitTypes::Terran_Barracks) {
			nrOfBarracks++;
		}
		else if (units->getType() == UnitTypes::Terran_Marine) {
			nrOfMarines++;
		}
		else if (units->getType() == UnitTypes::Terran_Refinery) {
			nrOfRefinerys++;
		}
		else if (units->getType() == UnitTypes::Terran_Academy) {
			nrOfAcademys++;
		}
		else if (units->getType() == UnitTypes::Terran_SCV) {
			nrOfWorkers++;
		}
		else if (units->getType() == UnitTypes::Terran_Medic) {
			nrOfMedics++;
		}
	}
}

//Is called when text is written in the console window.
//Can be used to toggle stuff on and off.
void ExampleAIModule::onSendText(std::string text)
{
	if (text == "/show players")
	{
		showPlayers();
	}
	else if (text == "/show forces")
	{
		showForces();
	}
	else
	{
		Broodwar->printf("You typed '%s'!", text.c_str());
		Broodwar->sendText("%s", text.c_str());
	}
}

//Called when the opponent sends text messages.
//No need to change this.
void ExampleAIModule::onReceiveText(BWAPI::Player player, std::string text)
{
	Broodwar->printf("%s said '%s'", player->getName().c_str(), text.c_str());
}

//Called when a player leaves the game.
//No need to change this.
void ExampleAIModule::onPlayerLeft(BWAPI::Player player)
{
	Broodwar->sendText("%s left the game.", player->getName().c_str());
}

//Called when a nuclear launch is detected.
//No need to change this.
void ExampleAIModule::onNukeDetect(BWAPI::Position target)
{
	if (target != Positions::Unknown)
	{
		Broodwar->printf("Nuclear Launch Detected at (%d,%d)", target.x, target.y);
	}
	else
	{
		Broodwar->printf("Nuclear Launch Detected");
	}
}

//No need to change this.
void ExampleAIModule::onUnitDiscover(BWAPI::Unit unit)
{
	//Broodwar->sendText("A %s [%x] has been discovered at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x,unit->getPosition().y);
}

//No need to change this.
void ExampleAIModule::onUnitEvade(BWAPI::Unit unit)
{
	//Broodwar->sendText("A %s [%x] was last accessible at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x,unit->getPosition().y);
}

//No need to change this.
void ExampleAIModule::onUnitShow(BWAPI::Unit unit)
{
	//Broodwar->sendText("A %s [%x] has been spotted at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x,unit->getPosition().y);
}

//No need to change this.
void ExampleAIModule::onUnitHide(BWAPI::Unit unit)
{
	//Broodwar->sendText("A %s [%x] was last seen at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x,unit->getPosition().y);
}

//Called when a new unit has been created.
//Note: The event is called when the new unit is built, not when it
//has been finished.
void ExampleAIModule::onUnitCreate(BWAPI::Unit unit)
{
	if (unit->getPlayer() == Broodwar->self())
	{
		Broodwar->sendText("A %s [%x] has been created at (%d,%d)", unit->getType().getName().c_str(), unit, unit->getPosition().x, unit->getPosition().y);
	}
}

//Called when a unit has been destroyed.
void ExampleAIModule::onUnitDestroy(BWAPI::Unit unit)
{
	if (unit->getPlayer() == Broodwar->self())
	{
		Broodwar->sendText("My unit %s [%x] has been destroyed at (%d,%d)", unit->getType().getName().c_str(), unit, unit->getPosition().x, unit->getPosition().y);
	}
	else
	{
		Broodwar->sendText("Enemy unit %s [%x] has been destroyed at (%d,%d)", unit->getType().getName().c_str(), unit, unit->getPosition().x, unit->getPosition().y);
	}
}

//Only needed for Zerg units.
//No need to change this.
void ExampleAIModule::onUnitMorph(BWAPI::Unit unit)
{
	//Broodwar->sendText("A %s [%x] has been morphed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x,unit->getPosition().y);
}

//No need to change this.
void ExampleAIModule::onUnitRenegade(BWAPI::Unit unit)
{
	//Broodwar->sendText("A %s [%x] is now owned by %s",unit->getType().getName().c_str(),unit,unit->getPlayer()->getName().c_str());
}

//No need to change this.
void ExampleAIModule::onSaveGame(std::string gameName)
{
	Broodwar->printf("The game was saved to \"%s\".", gameName.c_str());
}

//Analyzes the map.
//No need to change this.
DWORD WINAPI AnalyzeThread()
{
	BWTA::analyze();

	//Self start location only available if the map has base locations
	if (BWTA::getStartLocation(BWAPI::Broodwar->self()) != NULL)
	{
		home = BWTA::getStartLocation(BWAPI::Broodwar->self())->getRegion();
	}
	//Enemy start location only available if Complete Map Information is enabled.
	if (BWTA::getStartLocation(BWAPI::Broodwar->enemy()) != NULL)
	{
		enemy_base = BWTA::getStartLocation(BWAPI::Broodwar->enemy())->getRegion();
	}
	analyzed = true;
	analysis_just_finished = true;
	return 0;
}

//Prints some stats about the units the player has.
//No need to change this.
void ExampleAIModule::drawStats()
{
	BWAPI::Unitset myUnits = Broodwar->self()->getUnits();
	Broodwar->drawTextScreen(5, 0, "I have %d units:", myUnits.size());
	std::map<UnitType, int> unitTypeCounts;
	for (auto u : myUnits)
	{
		if (unitTypeCounts.find(u->getType()) == unitTypeCounts.end())
		{
			unitTypeCounts.insert(std::make_pair(u->getType(), 0));
		}
		unitTypeCounts.find(u->getType())->second++;
	}
	int line = 1;
	for (std::map<UnitType, int>::iterator i = unitTypeCounts.begin(); i != unitTypeCounts.end(); i++)
	{
		Broodwar->drawTextScreen(5, 16 * line, "- %d %ss", i->second, i->first.getName().c_str());
		line++;
	}
}

//Draws terrain data aroung regions and chokepoints.
//No need to change this.
void ExampleAIModule::drawTerrainData()
{
	//Iterate through all the base locations, and draw their outlines.
	for (auto bl : BWTA::getBaseLocations())
	{
		TilePosition p = bl->getTilePosition();
		Position c = bl->getPosition();
		//Draw outline of center location
		Broodwar->drawBox(CoordinateType::Map, p.x * 32, p.y * 32, p.x * 32 + 4 * 32, p.y * 32 + 3 * 32, Colors::Blue, false);
		//Draw a circle at each mineral patch
		for (auto m : bl->getStaticMinerals())
		{
			Position q = m->getInitialPosition();
			Broodwar->drawCircle(CoordinateType::Map, q.x, q.y, 30, Colors::Cyan, false);
		}
		//Draw the outlines of vespene geysers
		for (auto v : bl->getGeysers())
		{
			TilePosition q = v->getInitialTilePosition();
			Broodwar->drawBox(CoordinateType::Map, q.x * 32, q.y * 32, q.x * 32 + 4 * 32, q.y * 32 + 2 * 32, Colors::Orange, false);
		}
		//If this is an island expansion, draw a yellow circle around the base location
		if (bl->isIsland())
		{
			Broodwar->drawCircle(CoordinateType::Map, c.x, c.y, 80, Colors::Yellow, false);
		}
	}
	//Iterate through all the regions and draw the polygon outline of it in green.
	for (auto r : BWTA::getRegions())
	{
		BWTA::Polygon p = r->getPolygon();
		for (int j = 0; j<(int)p.size(); j++)
		{
			Position point1 = p[j];
			Position point2 = p[(j + 1) % p.size()];
			Broodwar->drawLine(CoordinateType::Map, point1.x, point1.y, point2.x, point2.y, Colors::Green);
		}
	}
	//Visualize the chokepoints with red lines
	for (auto r : BWTA::getRegions())
	{
		for (auto c : r->getChokepoints())
		{
			Position point1 = c->getSides().first;
			Position point2 = c->getSides().second;
			Broodwar->drawLine(CoordinateType::Map, point1.x, point1.y, point2.x, point2.y, Colors::Red);
		}
	}
}

//Show player information.
//No need to change this.
void ExampleAIModule::showPlayers()
{
	for (auto p : Broodwar->getPlayers())
	{
		Broodwar->printf("Player [%d]: %s is in force: %s", p->getID(), p->getName().c_str(), p->getForce()->getName().c_str());
	}
}

//Show forces information.
//No need to change this.
void ExampleAIModule::showForces()
{
	for (auto f : Broodwar->getForces())
	{
		BWAPI::Playerset players = f->getPlayers();
		Broodwar->printf("Force %s has the following players:", f->getName().c_str());
		for (auto p : players)
		{
			Broodwar->printf("  - Player [%d]: %s", p->getID(), p->getName().c_str());
		}
	}
}

//Called when a unit has been completed, i.e. finished built.
void ExampleAIModule::onUnitComplete(BWAPI::Unit unit)
{
	//Broodwar->sendText("A %s [%x] has been completed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x,unit->getPosition().y);
}
