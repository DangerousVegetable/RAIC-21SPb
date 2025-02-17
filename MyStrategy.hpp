#ifndef _MY_STRATEGY_HPP_
#define _MY_STRATEGY_HPP_

#include "model/Game.hpp"
#include "model/Action.hpp"

#include "Cycle.h"
#include "Planet.h"
#include "Terms.h"
#include <vector>
#include <iostream>
#include <fstream>

using namespace std;

class MyStrategy {
public:
	MyStrategy();
	model::Action getAction(const model::Game& game);

	int population;
	int homePlanet;
	vector<int> enemyHomePlanets;
	vector<Planet> planetInf;
	vector<vector<int>> planetDists;
	void init(const model::Game& game);
	void separatePlanets(const model::Game& game); // generating list of planets

	int resetTimer;
	Cycle prodCycle;
};

#endif
