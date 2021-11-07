#include "MyStrategy.hpp"
#include <exception>

MyStrategy::MyStrategy() : homePlanet(-1), resetTimer(0) {}

model::Action MyStrategy::getAction(const model::Game& game) {
	if (homePlanet == -1) {
		init(game);
		prodCycle.init(game, teamPlayers, teamHomePlanets, enemyHomePlanets, logDists);
	}
	separatePlanets(game);
	++resetTimer;

	// total robots available
	population = 0;
	for (int i = 0; i < game.planets.size(); i++) {
		//int or = 0;
		for (auto wg: game.planets[i].workerGroups) {
			if (wg.playerIndex == game.myIndex) {
				population += wg.number;
			}
		}
	}
	for (auto fw: game.flyingWorkerGroups) {
		if (fw.playerIndex == game.myIndex) {
			population += fw.number;
		}
	}

	vector<model::MoveAction> moveActions;
	vector<model::BuildingAction> buildActions;

	observer.update(game, planetDists);
	fc.updateSafeAdj(game, game.maxTravelDistance);

	if (role == COMBAT)
	{
		warController.update(game, fc, observer);
	}

	if (!prodCycle.isPlanned) {
		prodCycle.planBuilding(game, logDists);
		//role initialization
		vector<pair<int, int>> baseDists; //bAsed LMAO||| first - distance, second - home planet id
		for (int id: teamHomePlanets) {
			int maxdist = 0;
			for (int i = 0; i < prodCycle.buildingPlanet.size(); i++) {
				for (int j = 0; j < prodCycle.buildingPlanet[i].size(); j++) {
					maxdist = max(maxdist, planetDists[id][prodCycle.buildingPlanet[i][j]]);
				}
			}
			baseDists.push_back(pair<int, int>(maxdist, id));
		}

		sort(baseDists.begin(), baseDists.end(), [](pair<int, int> a, pair<int, int> b) {
			if (a.first == b.first) return a.second < b.second;
			else return a.first < b.first;
		});

		for (int i = 0; i < baseDists.size(); i++) {
			if (baseDists[i].second == homePlanet) {
				role = i;
				break;
			}
		}
		cout << "\nrole:" << role;

	} else {
		static vector<pair<int, int>> buildingOrder; //planet id and building type
		if (buildingOrder.size() == 0) //if buildingOrder is not initialized
		{
			for (int i = 0; i < prodCycle.buildingPlanet.size(); i++) {
				for (int j = 0; j < prodCycle.buildingPlanet[i].size(); j++) {
					buildingOrder.push_back(
							pair<int, int>(prodCycle.buildingPlanet[i][j], i)); //planet id and building type
				}
			}
			sort(buildingOrder.begin(), buildingOrder.end(), [&](pair<int, int> b1, pair<int, int> b2) {
				return planetDists[homePlanet][b1.first] > planetDists[homePlanet][b2.first];
			});
		}

		prodCycle.isBuilt = true;
		vector<bool> isBuilt(buildingOrder.size(), false); //true if ith building is built
		for (int i = 0; i < buildingOrder.size(); i++) {
			if (game.planets[buildingOrder[i].first].building.has_value()
				&& b2t(game.planets[buildingOrder[i].first].building.value().buildingType) == buildingOrder[i].second) {
				isBuilt[i] = true;
			} else prodCycle.isBuilt = false;
		}

		if (!prodCycle.isBuilt && role == WORKER) //ща будем базу строить
		{
			static vector<bool> inProcess(buildingOrder.size(),
										  false); //true if there is number of workers already in a flight to build the ith building
			int freestone = min(game.planets[homePlanet].resources.count(t2r(STONE)) ?
								game.planets[homePlanet].resources.at(t2r(STONE)) : 0,
								!game.planets[homePlanet].workerGroups.empty() ?
								game.planets[homePlanet].workerGroups[0].number : 0);

			for (int i = 0; i < buildingOrder.size(); i++) {
				if (isBuilt[i]) {
					inProcess[i] = false;
					if (game.planets[buildingOrder[i].first].workerGroups.size() > 0
						&& game.planets[buildingOrder[i].first].workerGroups[0].playerIndex == game.myIndex) {
						fc.send(buildingOrder[i].first, homePlanet,
								game.planets[buildingOrder[i].first].workerGroups[0].number, {},
								true); //if there are workers on a planet with a building then order them back
					}
				} else if (!inProcess[i]) {
					int price = game.buildingProperties.at(t2b(buildingOrder[i].second)).buildResources.at(
							model::Resource::STONE);
					if (freestone >= price) //if we can build it
					{
						fc.send(homePlanet, buildingOrder[i].first, price, model::Resource::STONE, true);
						freestone -= price;
						inProcess[i] = true;
						buildActions.push_back(
								model::BuildingAction(buildingOrder[i].first, t2b(buildingOrder[i].second)));
					}
				} else {
					buildActions.push_back(model::BuildingAction(buildingOrder[i].first, t2b(buildingOrder[i].second)));
				}
			}
		}
	}

	if (prodCycle.isBuilt) {
#if 0 //TODO: отредачить под новый формат buildPlanets
		if (resetTimer > 100) {
			for (int building = 3; building < prodCycle.stackedPlanet.size(); ++building) {
				if (prodCycle.stackedPlanet[building]) {
					if (game.planets[prodCycle.buildingPlanet[building]].workerGroups.empty() ||
						game.planets[prodCycle.buildingPlanet[building]].workerGroups[0].playerIndex != game.myIndex) {
						continue;
					}
					int fr = prodCycle.buildingPlanet[building];
					int freeRobots = game.planets[fr].workerGroups[0].number - fc.onFlightAt(fr);

					fc.send(fr, prodCycle.buildingPlanet[CAREER],
											  freeRobots / 3,
											  optional<model::Resource>());

					fc.send(fr, prodCycle.buildingPlanet[FARM],
											  freeRobots / 3,
											  optional<model::Resource>());

					fc.send(fr, prodCycle.buildingPlanet[MINES],
											  freeRobots -
											  2 * freeRobots / 3,
											  optional<model::Resource>());
				}
			}

			resetTimer = 0;
			prodCycle.stackedPlanet = vector<bool>(prodCycle.stackedPlanet.size(), true);
		}
		for (int id = 0; id < game.planets.size(); ++id) {
			if (!game.planets[id].workerGroups.empty() &&
				game.planets[id].workerGroups[0].playerIndex == game.myIndex) {

				if (id == prodCycle.buildingPlanet[MINES]) {
					if (prodCycle.sendRobots(game, fc, id, ORE, 32,
											 {
													 {prodCycle.buildingPlanet[FOUNDRY],      1.0 / 2},
													 {prodCycle.buildingPlanet[EXTRAFOUNDRY], 1.0 / 2}},
											 {},
											 12 * 3)) {

						prodCycle.stackedPlanet[MINES] = false;
					}

				} else if (id == prodCycle.buildingPlanet[CAREER]) {
					if (prodCycle.sendRobots(game, fc, id, SAND, 16,
											 {
													 {prodCycle.buildingPlanet[FURNACE], 1.0 / 1}},
											 {},
											 12 * 3)) {

						prodCycle.stackedPlanet[CAREER] = false;
					}

				} else if (id == prodCycle.buildingPlanet[FARM]) {
					if (prodCycle.sendRobots(game, fc, id, ORGANICS, 8,
											 {
													 {prodCycle.buildingPlanet[BIOREACTOR], 1.0 / 1}},
											 {},
											 6 * 3)) {

						prodCycle.stackedPlanet[FARM] = false;
					}

				} else if (id == prodCycle.buildingPlanet[FOUNDRY]) {
					if (prodCycle.sendRobots(game, fc, id, METAL, 16 / 2,
											 {
													 {prodCycle.buildingPlanet[CHIP_FACTORY],        1.0 / 4},
													 {prodCycle.buildingPlanet[ACCUMULATOR_FACTORY], 1.0 / 8},
													 {prodCycle.buildingPlanet[REPLICATOR],          1.0 / 8}},
											 {
													 {prodCycle.buildingPlanet[MINES], 1.0 / 2}},
											 16 * 3)) {

						prodCycle.stackedPlanet[FOUNDRY] = false;
					}

				} else if (id == prodCycle.buildingPlanet[EXTRAFOUNDRY]) {
					if (prodCycle.sendRobots(game, fc, id, METAL, 16 / 2,
											 {
													 {prodCycle.buildingPlanet[CHIP_FACTORY],        1.0 / 4},
													 {prodCycle.buildingPlanet[ACCUMULATOR_FACTORY], 1.0 / 8},
													 {prodCycle.buildingPlanet[REPLICATOR],          1.0 / 8}},
											 {
													 {prodCycle.buildingPlanet[MINES], 1.0 / 2}},
											 16 * 3)) {

						prodCycle.stackedPlanet[EXTRAFOUNDRY] = false;
					}

				} else if (id == prodCycle.buildingPlanet[FURNACE]) {
					if (prodCycle.sendRobots(game, fc, id, SILICON, 8,
											 {
													 {prodCycle.buildingPlanet[CHIP_FACTORY], 1.0 / 2}},
											 {
													 {prodCycle.buildingPlanet[CAREER], 1.0 / 2}},
											 12 * 3)) {

						prodCycle.stackedPlanet[FURNACE] = false;
					}

				} else if (id == prodCycle.buildingPlanet[BIOREACTOR]) {
					if (prodCycle.sendRobots(game, fc, id, PLASTIC, 4,
											 {
													 {prodCycle.buildingPlanet[ACCUMULATOR_FACTORY], 1.0 / 2}},
											 {
													 {prodCycle.buildingPlanet[FARM], 1.0 / 2}},
											 12 * 3)) {

						prodCycle.stackedPlanet[BIOREACTOR] = false;
					}

				} else if (id == prodCycle.buildingPlanet[CHIP_FACTORY]) {
					if (prodCycle.sendRobots(game, fc, id, CHIP, 4,
											 {
													 {prodCycle.buildingPlanet[REPLICATOR], 1.0 / 3}},
											 {
													 {prodCycle.buildingPlanet[MINES],  1.0 / 3},
													 {prodCycle.buildingPlanet[CAREER], 1.0 / 3}},
											 12 * 3)) {

						prodCycle.stackedPlanet[CHIP_FACTORY] = false;
					}

				} else if (id == prodCycle.buildingPlanet[ACCUMULATOR_FACTORY]) {
					if (prodCycle.sendRobots(game, fc, id, ACCUMULATOR, 2,
											 {
													 {prodCycle.buildingPlanet[REPLICATOR], 1.0 / 3}},
											 {
													 {prodCycle.buildingPlanet[MINES], 1.0 / 3},
													 {prodCycle.buildingPlanet[FARM],  1.0 / 3}},
											 12 * 3)) {

						prodCycle.stackedPlanet[ACCUMULATOR_FACTORY] = false;
					}

				} else if (id == prodCycle.buildingPlanet[REPLICATOR]) {
					if (prodCycle.sendRobots(game, fc, id, -1, 5,
											 {},
											 {
													 {prodCycle.buildingPlanet[FARM],   1.0 / 7},
													 {prodCycle.buildingPlanet[CAREER], 1.0 * 2 / 7},
													 {prodCycle.buildingPlanet[MINES],  1.0 * 4 / 7}},
											 14 * 3)) {

						prodCycle.stackedPlanet[REPLICATOR] = false;
					}

				} else {
					prodCycle.sendRobots(game, fc, id, -1, 0,
										 {},
										 {
												 {prodCycle.buildingPlanet[FARM],   1.0 / 7},
												 {prodCycle.buildingPlanet[CAREER], 1.0 * 2 / 7},
												 {prodCycle.buildingPlanet[MINES],  1.0 * 4 / 7}},
										 0, true);
				}
			}
		}
#endif
	}

	if (game.currentTick == 999) {
		int robC = 0;
		fstream file("stat.txt", ios::in);
		file >> robC;
		file.close();
		for (int id = 0; id < game.planets.size(); ++id) {
			if (!game.planets[id].workerGroups.empty() &&
				game.planets[id].workerGroups[0].playerIndex == game.myIndex) {
				robC += game.planets[id].workerGroups[0].number;
			}
		}
		for (int i = 0; i < game.flyingWorkerGroups.size(); ++i) {
			if (game.flyingWorkerGroups[i].playerIndex == game.myIndex) {
				robC += game.flyingWorkerGroups[i].number;
			}
		}
		file.open("stat.txt", ios::out);
		file << robC;
		file.close();
		int c;
		file.open("statC.txt", ios::in);
		file >> c;
		file.close();
		file.open("statC.txt", ios::out);
		file << c + 1;
		file.close();
		file.open("statRes.txt", ios::out);
		file << robC / (c + 1);
		file.close();
	}

	vector<model::MoveAction> addMoves = fc.update();
	for (model::MoveAction move: addMoves) {
		moveActions.push_back(move);
	}

	return model::Action(moveActions, buildActions, optional<model::Specialty>());
}

void MyStrategy::init(const model::Game& game) {
	for (int i = 0; i < game.players.size(); i++) {
		if (game.players[i].teamIndex == 0) {
			teamPlayers.insert(i);
			cout << i;
		}
	}

	for (int i = 0; i < game.planets.size(); ++i) {
		if (!game.planets[i].workerGroups.empty() &&
			game.planets[i].workerGroups[0].playerIndex == game.myIndex) {
			homePlanet = i;
		}

		if (!game.planets[i].workerGroups.empty() &&
			teamPlayers.find(game.planets[i].workerGroups[0].playerIndex) != teamPlayers.end()) {
			teamHomePlanets.push_back(i);
			//cout << i << '\n';
		}

		if (!game.planets[i].workerGroups.empty() &&
			teamPlayers.find(game.planets[i].workerGroups[0].playerIndex) == teamPlayers.end()) {
			enemyHomePlanets.push_back(i);
		}
	}

	//cout << game.myIndex << "\n";
	/*cout << enemyHomePlanets.size() << teamHomePlanets.size();

	for(int i = 0; i < enemyHomePlanets.size(); i++)
	{
		cout << teamHomePlanets[i] << " ";
		cout << enemyHomePlanets[i] << "| ";
	}*/

	const int INF = 1000000;
	planetDists = vector<vector<int>>(game.planets.size(), vector<int>(game.planets.size(), INF));
	logDists = vector<vector<int>>(game.planets.size(), vector<int>(game.planets.size(), INF));

	for (int i = 0; i < game.planets.size(); ++i) {
		for (int j = 0; j < game.planets.size(); ++j) {			
			int d = abs(game.planets[i].x - game.planets[j].x) +
					abs(game.planets[i].y - game.planets[j].y);
			
			if (d <= game.maxTravelDistance)
				planetDists[i][j] = d;
			
			if (d <= game.maxTravelDistance + game.logisticsUpgrade)
				logDists[i][j] = d;
		}
	}

	for (int i = 0; i < game.planets.size(); ++i) {
		for (int j = 0; j < i; ++j) {
			for (int k = 0; k < game.planets.size(); ++k)
			{
				planetDists[i][j] = min(planetDists[i][j], planetDists[i][k] + planetDists[k][j]);
				planetDists[j][i] = planetDists[i][j];

				logDists[i][j] = min(logDists[i][j], logDists[i][k] + logDists[k][j]);
				logDists[j][i] = logDists[i][j];
			}
		}
	}


	fc.setup(planetDists, &observer);
	fc.updateAdj(game, game.maxTravelDistance);

	observer.setup(game);

}

void MyStrategy::separatePlanets(const model::Game& game) { // generating list of planets
	//TODO change to true zone separating
	if (planetInf.empty()) {
		planetInf = vector<Planet>(game.planets.size());
	}

	for (int id = 0; id < game.planets.size(); ++id) {
		if (game.planets[id].workerGroups.empty() || game.planets[id].workerGroups[0].playerIndex == game.myIndex) {
			planetInf[id].planetOwner = 1;
		} else {
			planetInf[id].planetOwner = 0;
		}
	}
}
