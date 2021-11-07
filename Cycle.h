//
// Created by Maximilian on 09.10.2021.
//

#ifndef MYSTRATEGY_CPP_CYCLE_H
#define MYSTRATEGY_CPP_CYCLE_H

#define CYCLE_BUILD_NUM 10

#include "model/Game.hpp"
#include "model/Action.hpp"
#include <vector>
#include <utility>
#include <set>
#include <random>
#include <algorithm>
#include <chrono>
#include <map>

#include "Terms.h"
#include "FlyingController.h"

using namespace std;

class Cycle {
public:
	vector<vector<int>> buildingPlanet; // НОВЫЙ ФОРМАТ!!!
	vector<vector<double>> resourceTraffic; // сколько ресурса поступает от iх заводов к jм
	// (например если надо 4 реса от MINES к FOUNDRY и #MINES = 2, #FOUNDRY = 3, то надо отсылать 4/2/3 = 2/3 реса с каждой MINES к каждой FOUNDRY)
	vector<double> buildingWorkpower; //количество рабочей силы на iх заводах
	// (например значение 240 на MINES, при условии, что #MINES = 2 - это по 100 рабочих на каждой MINE).
	// Общая формула количества рабочих: ceil(buildingWorkpower[i]/buildingPlanet[i].size()/1.2)

	vector<bool> orderedPlanet; // MINES -> REPLICATOR + EXTRAFOUNDRY не юзаю
	bool isBuilt;
	bool isPlanned;

	//vector<vector<int>> trafficCoeff;
	map<int, set<pair<int, double>>> trafficCoeff; //traffic graph (diagram) trafficCoeff[i] = {{build1, logistsrequired}, {build2, logistsrequired}}
	const static vector<double> workCoeff;//amount of workpower needed to power 1 wp on a replicator
	const static double baseConsumption; //sum of values above

	vector<vector<int>> planetTypes; // 0-2 - RES, 3 - NO RES, 4 - ALL
	vector<vector<int>> buildGraph; //graph where objects are planets connected with their 'neighbour' planets with the same type

	double logistsNum;

	bool sendRobots(const model::Game& game, FlyingController& fc, Observer& observer, int planet);

	vector<vector<int>> positionBuilding(const model::Game& game, const vector<vector<int>>& planetDists,
										 vector<double> buildPower, int attempts, int opts);

	void planBuilding(const model::Game& game, const vector<vector<int>>& planetDists);

	vector<double> getMaxWorkPower(const model::Game& game); //returns max workpower used in buildings

	void init(const model::Game& game, const set<int>& teammates, const vector<int>& teamHomePlanets,
			  const vector<int>& enemyHomePlanets, const vector<vector<int>>& planetDists);

	Cycle();

private:
	double logistsRequired(const vector<vector<int>>& planetDists, const vector<vector<int>>& candidates,
						   const vector<double>& cons); //returns amount of logists to maintain the base

	bool onMySide(const vector<vector<int>>& planetDists, const vector<int>& teamHomePlanets,
				  const vector<int>& enemyHomePlanets, int id);
};


#endif //MYSTRATEGY_CPP_CYCLE_H
