#pragma once
#include <BWAPI.h>

#include <BWTA.h>
#include <windows.h>

extern bool analyzed;
extern bool analysis_just_finished;
extern BWTA::Region* home;
extern BWTA::Region* enemy_base;
DWORD WINAPI AnalyzeThread();

using namespace BWAPI;
using namespace BWTA;

class ExampleAIModule : public BWAPI::AIModule
{
public:
	//Methods inherited from BWAPI:AIModule
	virtual void onStart();
	virtual void onEnd(bool isWinner);
	virtual void onFrame();
	virtual void onSendText(std::string text);
	virtual void onReceiveText(BWAPI::Player player, std::string text);
	virtual void onPlayerLeft(BWAPI::Player player);
	virtual void onNukeDetect(BWAPI::Position target);
	virtual void onUnitDiscover(BWAPI::Unit unit);
	virtual void onUnitEvade(BWAPI::Unit unit);
	virtual void onUnitShow(BWAPI::Unit unit);
	virtual void onUnitHide(BWAPI::Unit unit);
	virtual void onUnitCreate(BWAPI::Unit unit);
	virtual void onUnitDestroy(BWAPI::Unit unit);
	virtual void onUnitMorph(BWAPI::Unit unit);
	virtual void onUnitRenegade(BWAPI::Unit unit);
	virtual void onSaveGame(std::string gameName);
	virtual void onUnitComplete(BWAPI::Unit unit);

	//Own methods
	void drawStats();
	void drawTerrainData();
	void showPlayers();
	void showForces();
	void gatherMaterials();
	void buildStuff(UnitType unitType, Unit unit);
	void countUnits();
	Position findGuardPoint();
private:
	int guardID = 0;
	int nrOfSupplyDepot = 0;
	int nrOfBarracks = 0;
	int nrOfRefinerys = 0;
	int nrOfAcademys = 0;
	int nrOfMarines = 0;
	int nrOfWorkers = 4;
	int nrOfMedics = 0;
	int nrOfFactorys = 0;
	int nrOfSiegeTanks = 0;
	int nrOfMachineShopAddon = 0;
	int nrOfBays = 0;
	int nrOfCommandCenter = 0;
	int firstCommandCenterID = 0;
	int nrOfBunkers = 0;
};
