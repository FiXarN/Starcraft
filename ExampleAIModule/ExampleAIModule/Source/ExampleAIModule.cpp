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
	for (auto firstCommandCenter : Broodwar->self()->getUnits()) {
		if (firstCommandCenter->getType() == UnitTypes::Terran_Command_Center) {
			firstCommandCenterID = firstCommandCenter->getID();
			break;
		}
	}
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
		gatherMinerals();
		//Commands units
		for (auto u : Broodwar->self()->getUnits()) {
			if (u->getType() == UnitTypes::Terran_Barracks ||
				u->getType() == UnitTypes::Terran_Factory) {
				//Find guardPoint
				Position guardPos = findGuardPoint();
				guardPos.y -= 10.0;
				//Move units to guardPoint
				u->setRallyPoint(guardPos);
			}
			//Set tanks into siege mode when eneimes is visble OR normal mode when enemy is not visble
			int nrOfVisibleEnemies = 0;
			for (auto enemy : Broodwar->enemy()->getUnits()) {
				if (!enemy->getType().isBuilding()) {
					nrOfVisibleEnemies++;
				}
			}
			if (nrOfVisibleEnemies > 0) {
				if (u->getType() == UnitTypes::Terran_Siege_Tank_Tank_Mode) {
					u->siege();
				}
				else {
					u->unsiege();
				}
			}
		}
		//Make workers build
		for (auto u : Broodwar->self()->getUnits())
		{
			countBuildings();
			//Check if unit is a worker.
			if (u->getType().isWorker() && !u->isConstructing()) {
				//Step 2, build barrack
				if (u->canBuild(UnitTypes::Terran_Barracks) && nrOfWorkers >= 9 && nrOfBarracks < 1) {
					buildStuff(UnitTypes::Terran_Barracks, u);
				}
				//Step 3, build two supply
				else if (u->canBuild(UnitTypes::Terran_Supply_Depot) && nrOfBarracks >= 1 && nrOfSupplyDepot < 4) {
					buildStuff(UnitTypes::Terran_Supply_Depot, u);
				}
				//Build bunker at chokepoint
				else if (u->canBuild(UnitTypes::Terran_Bunker) && nrOfSupplyDepot == 4 && nrOfBunkers < 2) {
					//Find guardPoint
					Position guardPos = findGuardPoint();
					u->rightClick(guardPos);
					if (u->getPosition() == guardPos) {
						TilePosition posAtChoke = Broodwar->getBuildLocation(UnitTypes::Terran_Bunker, u->getTilePosition(), 250);
						if (Broodwar->canBuildHere(posAtChoke, UnitTypes::Terran_Bunker)) {
							Broodwar->printf("Build bunker");
							u->build(UnitTypes::Terran_Bunker, posAtChoke);
						}
					}
				}
				//Extra add on, build engineering bay
				else if (u->canBuild(UnitTypes::Terran_Engineering_Bay) && nrOfRefinerys >= 1 && nrOfBays < 1) {
					buildStuff(UnitTypes::Terran_Engineering_Bay, u);
				}
				//Step 5, build refinery
				/*else if (u->canBuild(UnitTypes::Terran_Refinery) && nrOfSupplyDepot >= 4 && nrOfRefinerys < 1) {
					buildStuff(UnitTypes::Terran_Refinery, u);
				}
				//Step 6, build academy
				else if (u->canBuild(UnitTypes::Terran_Academy) && nrOfFactorys >= 1 && nrOfAcademys < 1) {
					buildStuff(UnitTypes::Terran_Academy, u);
				}
				//Step 8, build factory
				else if (u->canBuild(UnitTypes::Terran_Factory) && nrOfBays >= 1 && nrOfFactorys < 1) {
					buildStuff(UnitTypes::Terran_Factory, u);
				}*/
				break;
			}
		}

		//Train units
		for (auto units : Broodwar->self()->getUnits()) {
			//Step 1, build enough of workers
			if (units->getType() == UnitTypes::Terran_Command_Center && nrOfWorkers < 9) {
				if (units->canTrain(UnitTypes::Terran_SCV)) {
					units->train(UnitTypes::Terran_SCV);
					nrOfWorkers++;
				}
			}
			//Step 4, build enough with marine
			if (units->getType() == UnitTypes::Terran_Barracks && nrOfBays >= 1 && nrOfMarines < 10) {
				if (units->canTrain(UnitTypes::Terran_Marine)) {
					units->train(UnitTypes::Terran_Marine);
					nrOfMarines++;
				}
			}
			//Step 7, build enough of medics
			if (units->getType() == UnitTypes::Terran_Barracks && nrOfAcademys >= 1 && nrOfMedics < 7) {
				if (units->canTrain(UnitTypes::Terran_Medic)) {
					units->train(UnitTypes::Terran_Medic);
					nrOfMedics++;
				}
			}
			//Step 9, build add-on on factory
			if (units->getType() == UnitTypes::Terran_Factory) {
				if (units->canBuildAddon(UnitTypes::Terran_Machine_Shop) && nrOfMachineShopAddon < 1) {
					units->buildAddon(UnitTypes::Terran_Machine_Shop);
					nrOfMachineShopAddon++;
				}
			}
			//Step 10, build siege tanks - Tank mode
			if (units->getType() == UnitTypes::Terran_Factory && nrOfMachineShopAddon >= 1 && nrOfSiegeTanks < 5) {
				if (units->canTrain(UnitTypes::Terran_Siege_Tank_Tank_Mode)) {
					units->train(UnitTypes::Terran_Siege_Tank_Tank_Mode);
					nrOfSiegeTanks++;
				}
			}
			//Step 11, research siege mode
			if (units->getType() == UnitTypes::Terran_Machine_Shop) {
				if (units->canResearch(TechTypes::Tank_Siege_Mode)) {
					units->research(TechTypes::Tank_Siege_Mode);
				}
			}
			//Make armor and better weapons
			if (units->getType() == UnitTypes::Terran_Engineering_Bay) {
				if (units->canUpgrade(UpgradeTypes::Terran_Infantry_Armor)) {
					units->upgrade(UpgradeTypes::Terran_Infantry_Armor);
				}
				else if (units->canUpgrade(UpgradeTypes::Terran_Infantry_Weapons)) {
					units->upgrade(UpgradeTypes::Terran_Infantry_Weapons);
				}
			}
		}

		//Find a new base location

		/*if (nrOfSiegeTanks >= 1) {
			//Start base
			BaseLocation* myBase = BWTA::getNearestBaseLocation(Broodwar->self()->getStartLocation());
			double distance = 999999999.9;
			BaseLocation* nearestBase;

			//Find the nearest base
			for (auto baseLocation : BWTA::getBaseLocations()) {
				if (baseLocation != myBase) {
					if (baseLocation->getGroundDistance(myBase) < distance) {
						distance = baseLocation->getGroundDistance(myBase);
						nearestBase = baseLocation;
					}
				}
			}
			//Move one worker to the new base location
			for (auto u : Broodwar->self()->getUnits()) {
				if (u->getType().isWorker()) {
					u->rightClick(nearestBase->getPosition());

					if (u->getPosition() == nearestBase->getPosition() && u->canBuild(UnitTypes::Terran_Command_Center)
						&& nrOfCommandCenter < 2 && nrOfSiegeTanks == 5) {
						buildStuff(UnitTypes::Terran_Command_Center, u);
					}
				}
				break;
			}
			//Train workers in new command center
			for (auto u : Broodwar->self()->getUnits()) {
				if (u->getType() == UnitTypes::Terran_Command_Center && u->getID() != firstCommandCenterID) {
					if (u->canTrain(UnitTypes::Terran_SCV) && nrOfWorkers < 13) {
						u->train(UnitTypes::Terran_SCV);
						nrOfWorkers++;
					}
				}
			}
		}*/
	}

	//Draw lines around regions, chokepoints etc.
	if (analyzed)
	{
		drawTerrainData();
	}
}

void ExampleAIModule::gatherMinerals() {
	// Send workers on either mining- or gas gathering duty
	for (auto u : Broodwar->self()->getUnits()) {
		// Counting how many workers are mining and how many are gathering gas
		int nrOfMiners = 0;
		int nrOfGasGatherers = 0;
		for (auto m : Broodwar->self()->getUnits()) {
			if (m->getType().isWorker() && m->isGatheringMinerals()) {
				nrOfMiners++;
			}
			else if (m->getType().isWorker() && m->isGatheringGas()) {
				nrOfGasGatherers++;
			}
		}

		//If we dont have a refinery, send all workers to gather minerals
		if (u->getType().isWorker() && !u->isConstructing() && nrOfRefinerys < 1)
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
		//If we do have a refinery, send three workers to gather gas
		else if (u->getType().isWorker() && !u->isConstructing() && nrOfRefinerys > 0 && (nrOfGasGatherers != 3 || nrOfMiners != 6)) {
			if (nrOfGasGatherers < 3) {
				//Broodwar->printf("POOP POOOP POOOOOP");
				Unit closestRefinery = NULL;
				for (auto refinery : Broodwar->self()->getUnits()) {
					if (refinery->getType().isRefinery()) {
						closestRefinery = refinery;
					}
				}
				if (closestRefinery != NULL)
				{
					u->rightClick(closestRefinery);
					//Broodwar->printf("Send worker %d to geyser %d", u->getID(), closestRefinery->getID());
				}
			}
			else {
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
}

void ExampleAIModule::buildStuff(UnitType unitType, Unit unit) {
	TilePosition buildingPos = Broodwar->getBuildLocation(unitType, unit->getTilePosition(), 100);
	if (Broodwar->canBuildHere(buildingPos, unitType) && unit->isConstructing() == false) {
		unit->build(unitType, buildingPos);
	}
}

void ExampleAIModule::countBuildings() {
	nrOfBarracks = 0;
	nrOfSupplyDepot = 0;
	nrOfRefinerys = 0;
	nrOfAcademys = 0;
	nrOfFactorys = 0;
	nrOfBays = 0;
	nrOfCommandCenter = 0;
	nrOfBunkers = 0;

	for (auto u : Broodwar->self()->getUnits()) {
		if (u->getType() == UnitTypes::Terran_Barracks) {
			nrOfBarracks++;
		}
		else if (u->getType() == UnitTypes::Terran_Supply_Depot) {
			nrOfSupplyDepot++;
		}
		else if (u->getType() == UnitTypes::Terran_Refinery) {
			nrOfRefinerys++;
		}
		else if (u->getType() == UnitTypes::Terran_Academy) {
			nrOfAcademys++;
		}
		else if (u->getType() == UnitTypes::Terran_Factory) {
			nrOfFactorys++;
		}
		else if (u->getType() == UnitTypes::Terran_Engineering_Bay) {
			nrOfBays++;
		}
		else if (u->getType() == UnitTypes::Terran_Command_Center) {
			nrOfCommandCenter++;
		}
		else if (u->getType() == UnitTypes::Terran_Bunker) {
			nrOfBunkers++;
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
		for (int j = 0; j < (int)p.size(); j++)
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
