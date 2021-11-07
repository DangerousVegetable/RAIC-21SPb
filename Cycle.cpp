//
// Created by Maximilian on 09.10.2021.
//

#include "Cycle.h"


const vector<double> Cycle::workCoeff = {6.4, 3.2, 1.6, 3.2, 1.6, 0.8, 0.8, 0.4, 1};
const double Cycle::baseConsumption = 19;

int planetType(const model::Game& game, int id) {
	if (game.planets[id].harvestableResource.has_value() &&
		game.planets[id].harvestableResource.value() != t2r(STONE)) { //has a harvestable resource differing from stone
		return r2t(game.planets[id].harvestableResource.value());
	} else return 3;
}

Cycle::Cycle() : buildingPlanet(9), resourceTraffic(9, vector<double>(9)), isBuilt(false),
				 orderedPlanet(CYCLE_BUILD_NUM, false), isPlanned(false),
				 /*stackedPlanet(CYCLE_BUILD_NUM, false), prodFactor(1),*/
				 trafficCoeff() {

	/*trafficCoeff[MINES].insert(pair<int, double>(FOUNDRY, 3.2));
	trafficCoeff[FOUNDRY].insert(pair<int, double>(MINES, 1.6));

	trafficCoeff[FOUNDRY].insert(pair<int, double>(CHIP_FACTORY, 0.8));
	trafficCoeff[CHIP_FACTORY].insert(pair<int, double>(MINES, 0.6));
	trafficCoeff[CHIP_FACTORY].insert(pair<int, double>(CAREER, 0.6));

	trafficCoeff[FOUNDRY].insert(pair<int, double>(ACCUMULATOR_FACTORY, 0.4));
	trafficCoeff[ACCUMULATOR_FACTORY].insert(pair<int, double>(MINES, 0.4));
	trafficCoeff[ACCUMULATOR_FACTORY].insert(pair<int, double>(FARM, 0.2));

	trafficCoeff[FOUNDRY].insert(pair<int, double>(REPLICATOR, 0.4));
	trafficCoeff[CHIP_FACTORY].insert(pair<int, double>(REPLICATOR, 0.4));
	trafficCoeff[ACCUMULATOR_FACTORY].insert(pair<int, double>(REPLICATOR, 0.2));
	trafficCoeff[REPLICATOR].insert(pair<int, double>(MINES, 0.6));
	trafficCoeff[REPLICATOR].insert(pair<int, double>(CAREER, 0.2));
	trafficCoeff[REPLICATOR].insert(pair<int, double>(FARM, 0.2));

	trafficCoeff[CAREER].insert(pair<int, double>(FURNACE, 1.6));
	trafficCoeff[FURNACE].insert(pair<int, double>(CAREER, 0.8));

	trafficCoeff[FURNACE].insert(pair<int, double>(CHIP_FACTORY, 0.8));

	trafficCoeff[FARM].insert(pair<int, double>(BIOREACTOR, 0.8));
	trafficCoeff[BIOREACTOR].insert(pair<int, double>(FARM, 0.4));

	trafficCoeff[BIOREACTOR].insert(pair<int, double>(ACCUMULATOR_FACTORY, 0.4));*/
	trafficCoeff[MINES].insert(pair<int, double>(FOUNDRY, 3.2));
	trafficCoeff[FOUNDRY].insert(pair<int, double>(MINES, 1.6));

	trafficCoeff[FOUNDRY].insert(pair<int, double>(CHIP_FACTORY, 0.8));
	trafficCoeff[CHIP_FACTORY].insert(pair<int, double>(MINES, 0.6));
	trafficCoeff[CHIP_FACTORY].insert(pair<int, double>(CAREER, 0.6));

	trafficCoeff[FOUNDRY].insert(pair<int, double>(ACCUMULATOR_FACTORY, 0.4));
	trafficCoeff[ACCUMULATOR_FACTORY].insert(pair<int, double>(MINES, 0.4));
	trafficCoeff[ACCUMULATOR_FACTORY].insert(pair<int, double>(FARM, 0.2));

	trafficCoeff[FOUNDRY].insert(pair<int, double>(REPLICATOR, 0.4));
	trafficCoeff[CHIP_FACTORY].insert(pair<int, double>(REPLICATOR, 0.4));
	trafficCoeff[ACCUMULATOR_FACTORY].insert(pair<int, double>(REPLICATOR, 0.2));
	trafficCoeff[REPLICATOR].insert(pair<int, double>(MINES, 0.6));
	trafficCoeff[REPLICATOR].insert(pair<int, double>(CAREER, 0.2));
	trafficCoeff[REPLICATOR].insert(pair<int, double>(FARM, 0.2));

	trafficCoeff[CAREER].insert(pair<int, double>(FURNACE, 1.6));
	trafficCoeff[FURNACE].insert(pair<int, double>(CAREER, 0.8));

	trafficCoeff[FURNACE].insert(pair<int, double>(CHIP_FACTORY, 0.8));

	trafficCoeff[FARM].insert(pair<int, double>(BIOREACTOR, 0.8));
	trafficCoeff[BIOREACTOR].insert(pair<int, double>(FARM, 0.4));

	trafficCoeff[BIOREACTOR].insert(pair<int, double>(ACCUMULATOR_FACTORY, 0.4));
}

bool isTeamPlanet(const model::Game& game, const set<int>& teammates, int id) {
	return (game.planets[id].workerGroups.empty() ||
			teammates.find(game.planets[id].workerGroups[0].playerIndex) != teammates.end());
}

void Cycle::init(const model::Game& game, const set<int>& teammates, const vector<int>& teamHomePlanets,
				 const vector<int>& enemyHomePlanets, const vector<vector<int>>& planetDists) {
	planetTypes.resize(5); // 0-2 - RES, 3 - NO RES, 4 - ALL
	for (int id = 0; id < game.planets.size(); ++id) {
		//is mine, doesn't have a building, and on 'my' side
		if (isTeamPlanet(game, teammates, id)
			&& !game.planets[id].building.has_value() && onMySide(planetDists, teamHomePlanets, enemyHomePlanets, id)) {
			planetTypes[planetType(game, id)].push_back(id);
			planetTypes[4].push_back(id);
		}
	}

	vector<int> avdist(5, 0); //average distances between planets of one type
	for (int t = 0; t < 5; t++) {
		int num = 0;
		int distsum = 0;
		for (int i = 0; i < planetTypes[t].size(); i++) {
			for (int j = 0; j < planetTypes[t].size(); j++) {
				num++;
				distsum += planetDists[planetTypes[t][i]][planetTypes[t][j]];
			}
		}

		if (t < 3) avdist[t] = distsum / num;
		else avdist[t] = game.maxTravelDistance * 2.5;
	}

	//graph where objects are planets connected with their 'neighbour' planets with the same type
	buildGraph.resize(game.planets.size());
	for (int i = 0; i < game.planets.size(); i++) {
		int type = planetType(game, i);
		if (type == 3)
			type = 4; //if a planet doesn't have resources then its `neigbours` are planets with any type nearby
		for (int j = 0; j < planetTypes[type].size(); j++) {
			if (planetDists[i][planetTypes[type][j]] <= avdist[type]) {
				buildGraph[i].push_back(planetTypes[type][j]);
			}
		}
	}
}

bool Cycle::sendRobots(const model::Game& game, FlyingController& fc, Observer& observer, int planet) {
	int planetType = REPLICATOR;
	for (int i = 0; i < buildingPlanet.size(); ++i) {
		for (int pl: buildingPlanet[i]) {
			if (pl == planet) {
				planetType = i;
			}
		}
	}

	double sumKRes = 0, sumKEmpty = 0;
	for (int targetPl = planetType + 1; targetPl < resourceTraffic[planetType].size(); ++targetPl) {
		sumKRes += resourceTraffic[planetType][targetPl];
	}
	for (int targetPl = 0; targetPl < planetType; ++targetPl) {
		sumKEmpty += resourceTraffic[planetType][targetPl];
	}

	int freeRobots = observer.ours[planet] - fc.onFlightAt(planet);
	// Cannot be negative, so
	freeRobots = max(0, freeRobots);

	int resource = planetType == REPLICATOR ? -1 : planetType;
	int freeReses = 0;
	if (resource != -1) {
		freeReses = game.planets[planet].resources.count(t2r(resource)) ?
					game.planets[planet].resources.at(t2r(resource)) : 0;
	}

	int leftRobots = freeRobots;
	int leftReses = freeReses;

	static vector<vector<double>> shortageRobots(game.planets.size(), vector<double>(game.planets.size(), 0));

	freeRobots = leftRobots; // чтобы рассчитывать пропорции из оставшихся
	freeReses = leftReses; // чтобы рассчитывать пропорции из оставшихся
	int totalFreeRobots = leftRobots;

	int lastAction = -1;
	//if (freeRobots < 12*3) return false;
	for (int targetPl = planetType + 1; targetPl < resourceTraffic[planetType].size(); ++targetPl) {
		int batch = (int)resourceTraffic[planetType][targetPl]/buildingPlanet[planetType].size()/buildingPlanet[targetPl].size();

		for (int targetID: buildingPlanet[targetPl]) {
			if(min(leftRobots, leftReses) >= batch)
			{
				fc.send(planet, targetID, batch, optional<model::Resource>(t2r(resource)));

				leftRobots -= batch;
				leftReses -= batch;
			}
//			shortageRobots[planet][targetID] += plK.second * totalFreeRobots - batch;
		}
	}

	freeRobots = leftRobots; // чтобы рассчитывать пропорции из оставшихся

	for (int targetPl = 0; targetPl < planetType; ++targetPl) {
		int batch = (int) resourceTraffic[planetType][targetPl]/buildingPlanet[planetType].size()/buildingPlanet[targetPl].size();

		for (int targetID: buildingPlanet[targetPl]) {
			if(leftRobots >= batch)
			{
				fc.send(planet, targetID, batch, optional<model::Resource>());
				lastAction = targetID;

				leftRobots -= batch;
			}
//			shortageRobots[planet][targetID] += plK.second * totalFreeRobots - batch;
		}
	}
	#if 0 //пока убрал, потому что не очень понял для чего он нужен
	if (leftRobots > 0 && lastAction != -1) {
		fc.send(planet, lastAction, leftRobots, optional<model::Resource>());
		//moveActions.rbegin()->workerNumber += leftRobots;
		//#shortage
		Robots[planet][moveActions.rbegin()->targetPlanet] += leftRobots;
	}
	#endif
	return true;
}

double Cycle::logistsRequired(const vector<vector<int>>& planetDists, const vector<vector<int>>& candidates,
							  const vector<double>& cons) {
	double factor = cons[0] / Cycle::workCoeff[0];
	double ans = 0;
	for (int i = 0; i < 9; i++) {
		for (auto p: trafficCoeff[i]) {
			int j = p.first;
			double cost = p.second;
			double realcost = (cost / candidates[i].size()) /
							  candidates[j].size(); //if 2 MINES send 4 res to 2 FOUNDRIES, each MINE sends 1 res to each FOUNDRY

			for (int v1 = 0; v1 < candidates[i].size(); v1++) {
				for (int v2 = 0; v2 < candidates[j].size(); v2++) {
					ans += realcost * planetDists[candidates[i][v1]][candidates[j][v2]] * factor;
				}
			}
		}
	}
	return ans;
}

bool Cycle::onMySide(const vector<vector<int>>& planetDists, const vector<int>& teamHomePlanets,
					 const vector<int>& enemyHomePlanets, int id) {
	int odist = -1;
	for (int en: enemyHomePlanets) {
		if (odist == -1) odist = planetDists[id][en];
		else odist = min(odist, planetDists[id][en]);
	}

	int mdist = -1;
	for (int te: teamHomePlanets) {
		if (mdist == -1) mdist = planetDists[id][te];
		else mdist = min(mdist, planetDists[id][te]);
	}

	return ((double) odist > 0.85 * (double) mdist);
}

vector<double> getWorkPower(double workpower) { //ordinary worker - 1, pro worker - 1.2
	double factor = workpower / Cycle::baseConsumption;
	vector<double> wpperbuilding;
	for (int i = 0; i < Cycle::workCoeff.size(); i++) {
		wpperbuilding.push_back(Cycle::workCoeff[i] * factor);
	}
	return wpperbuilding;
}

vector<double> Cycle::getMaxWorkPower(const model::Game& game) { //unused functinon btw
	vector<double> maxwp = getWorkPower(1200); //1200 = 1000 workers * 120%
	double coeff = -1;
	for (int i = 0; i < 3; i++) {
		//coeff = theoretically possible 'amount' of work on planets (if they're all 100% used) devided by calculated max
		double curcoeff = (planetTypes[i].size() * game.buildingProperties.at(t2b(i)).maxWorkers) / maxwp[i];
		//cout << curcoeff << '\n';
		if (coeff == -1) coeff = curcoeff;
		else coeff = min(1., min(coeff, curcoeff));
	}
	for (int i = 0; i < maxwp.size(); i++) {
		maxwp[i] = maxwp[i] * coeff;
	}
	return maxwp;
}

vector<vector<int>> Cycle::positionBuilding(const model::Game& game, const vector<vector<int>>& planetDists,
											vector<double> buildPower, int attempts, int opts) {
	static vector<vector<int>> ptypes = planetTypes; //shuffle of planetTypes

	vector<int> buildNum(buildPower.size(), 0);
	for (int i = 0; i < buildNum.size(); i++) {
		buildNum[i] = ceil(buildPower[i] / (game.buildingProperties.at(t2b(i)).maxWorkers * 1.2));
	}

	/*int attempts = 15;
	int opts = 50;*/

	double besteff = -1;
	vector<vector<int>> bestbuilding;
	//std::default_random_engine generator;
	for (int att = 0; att < attempts; att++) {//making attempts trying to find the best solution
		vector<vector<int>> current(buildNum.size()); //randomly generated base
		for (int i = 0; i < current.size(); i++) {
			current[i] = vector<int>(buildNum[i], -1);
		}

		for (int i = 0; i < 4; i++) {//shuffling ptypes
			std::random_shuffle(ptypes[i].begin(), ptypes[i].end());
		}

		vector<int> typeind(4, 0);
		for (int i = 0; i < current.size(); i++) {
			int btype = 3;
			if (i < 3) btype = i; //building type

			for (int j = 0; j < current[i].size(); j++) {
				if (typeind[btype] < ptypes[btype].size()) {
					current[i][j] = ptypes[btype][typeind[btype]];
					typeind[btype]++;
				} else {
					bool success = false;
					for (int htype = 0; htype < 3; htype++) {
						if (typeind[htype] < ptypes[htype].size()) {
							current[i][j] = ptypes[htype][typeind[htype]];
							typeind[htype]++;
							success = true;
							break;
						}
					}
					if (!success) {
						//cout << "fail :(\n";
						//fail (not enough planets to build the base lmao)
						return vector<vector<int>>(1, vector<int>(1, -1));
					}
				}
			}
		}

		int cureff = logistsRequired(planetDists, current, buildPower);
		if (att == 0) besteff = cureff;

		set<int> usedids;
		map<int, pair<int, int>> indicies;
		for (int i = 0; i < current.size(); i++) {
			for (int j = 0; j < current[i].size(); j++) {
				usedids.insert(current[i][j]);
				indicies[current[i][j]] = pair<int, int>(i, j);
			}
			/*usedids.emplace(current[i]);
			indicies[current[i]] = i;*/
		}

		for (int o = 0; o < opts; o++) {
			bool change = false;
			for (int i = 0; i < current.size(); i++) {
				for (int j = 0; j < current[i].size(); j++) { //running through all buildings
					//running through all 'neighbour' planets
					for (int k = 0; k < buildGraph[current[i][j]].size(); k++) {
						int pl = buildGraph[current[i][j]][k]; //neigbour planet

						vector<vector<int>> newcand = current;
						bool swap = false;

						if (usedids.find(pl) != usedids.end()) { //if planet is used then try to swap it
							//check if we can swap
							if (indicies[pl].first >= 3 || planetType(game, current[i][j]) == indicies[pl].first) {
								newcand[i][j] = pl;
								newcand[indicies[pl].first][indicies[pl].second] = current[i][j];
								swap = true;
							} else continue; //if we can't swap just skip this neighbour and carry on trying
						} else {
							newcand[i][j] = pl;
						}
						int neweff = logistsRequired(planetDists, newcand, buildPower);
						if (neweff < cureff) {
							if (swap) {
								indicies[current[i][j]] = indicies[pl];
								indicies[pl] = pair<int, int>(i, j);
							} else {
								usedids.erase(current[i][j]);
								usedids.insert(pl);
								indicies.erase(current[i][j]);
								indicies[pl] = pair<int, int>(i, j);
							}
							current = newcand;
							cureff = neweff;
							change = true;

							break; //if we managed to improve neweff for this planet then move on to the next one
						}
					}
				}
			}
			if (!change) {
				break; //if no imrovement to be seen then we're in the local minimum and continuing is pointless
				//cout << o << "\n";
			}
		}
		//cout << opts << "\n";
		//cout << cureff << "\n";

		if (besteff > cureff) {
			bestbuilding = current;
			besteff = cureff;
		}
	}
	return bestbuilding;
}

void Cycle::planBuilding(const model::Game& game, const vector<vector<int>>& planetDists) {
	cout << "Calculating optimal base plan....\n";
	double left = 0, right = 500; //we don't want our base to use more than 400 of wp (too much logists)
	int count = 5;
	int attempts = 10;
	int opts = 10; //this values have to be small at the begining
	const double controlLogNum = 1000; //num of logists required to pass

	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	vector<double> passedPowers;
	while (count--) {
		double mid = (left + right) / 2;
		vector<double> curwp = getWorkPower(mid);

		vector<vector<int>> bPlan = positionBuilding(game, planetDists, curwp, attempts, opts);
		if (bPlan.size() != 1) {
			double l = logistsRequired(planetDists, bPlan, curwp);
			if (l <= controlLogNum) {
				passedPowers.push_back(mid);
			}

			if (l < controlLogNum) left = mid;
			if (l > controlLogNum) right = mid;
		} else right = mid;
	}
	double bestpower = *passedPowers.rbegin(); //choosing the most optimal workpower

	vector<double> bestwp = getWorkPower(bestpower);
	vector<vector<int>> buildingPlan = positionBuilding(game, planetDists, bestwp, 50, 100);
	logistsNum = logistsRequired(planetDists, buildingPlan, bestwp);
	cout << logistsNum << " " << bestpower << " "
		 << bestpower / Cycle::baseConsumption / 5 << " per tick\n";
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::cout << "Time difference = " <<
			  std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "ms" << std::endl;

	//filling main info
	buildingPlanet = buildingPlan;
	buildingWorkpower = bestwp;
	double factor = bestpower / Cycle::baseConsumption;
	for (auto tr: trafficCoeff) {
		int i = tr.first;
		for (auto edge: tr.second) {
			int j = edge.first;
			double num = edge.second;
			resourceTraffic[i][j] = num*factor; //from ith buildings to jth buildings goes num res
		}
	}
/*
	for(int i = 0; i < 9; i++)
	{
		for(int j = 0; j < 9; j++)
		{
			cout << resourceTraffic[i][j] << " ";
		}
		cout << "\n";
	}
*/
	cout << "BASE:\n";
	for (int i = 0; i < buildingPlanet.size(); i++) {
		cout << i << ":\n\t";
		for (int j = 0; j < buildingPlanet[i].size(); j++) {
			cout << buildingPlanet[i][j] << " ";
		}
		cout << "\n";
	}
	isPlanned = true;
}