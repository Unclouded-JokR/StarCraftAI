<<<<<<< HEAD
#pragma once
#include <BWAPI.h>
#include <unordered_map>
#include <algorithm>
#include <vector>

class ProtoBotCommander;

class EconomyManager
{
public:
	ProtoBotCommander* commanderReference;
	std::unordered_map<BWAPI::Unit, int> assigned;
	//std::unordered_map<BWAPI::Unit, BWAPI::Unit> assignedWorkers;
	//std::unordered_map<std::string, std::vector<double>> map_of_vectors;
	std::unordered_map<BWAPI::Unit, std::vector<BWAPI::Unit>> assignedWorkers;


	EconomyManager(ProtoBotCommander* commanderReference);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Unit unit, const BWAPI::Unitset& units);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Position p, const BWAPI::Unitset& units);
	void OnFrame();
	void assignUnit(BWAPI::Unit unit);
	BWAPI::Unit getAvalibleWorker();
};
=======
#pragma once
#include <BWAPI.h>
#include <unordered_map>
#include <algorithm>
#include <vector>

class ProtoBotCommander;

class EconomyManager
{
public:
	ProtoBotCommander* commanderReference;
	std::unordered_map<BWAPI::Unit, int> assigned;
	//std::unordered_map<BWAPI::Unit, BWAPI::Unit> assignedWorkers;
	//std::unordered_map<std::string, std::vector<double>> map_of_vectors;
	std::unordered_map<BWAPI::Unit, std::vector<BWAPI::Unit>> assignedWorkers;


	EconomyManager(ProtoBotCommander* commanderReference);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Unit unit, const BWAPI::Unitset& units);
	BWAPI::Unit GetClosestUnitToWOWorker(BWAPI::Position p, const BWAPI::Unitset& units);
	void OnFrame();
	void assignUnit(BWAPI::Unit unit);
	BWAPI::Unit getAvalibleWorker();
};

>>>>>>> 3ce017aaacb86dcdfaa08f7d917aba3aea17c5a4
