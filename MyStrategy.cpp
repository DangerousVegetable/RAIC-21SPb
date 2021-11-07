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
		//warController.update(game, fc, observer);
	}

	if (!prodCycle.isPlanned) {
		prodCycle.planBuilding(game, logDists);
		//role initialization
		vector<pair<int, int>> baseDists; //first - distance, second - home planet id
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

		vector<double> needWorkers(game.planets.size(), 0); //workers needed on a planet;
		vector<double> upcomingWorkers(game.planets.size(), 0); //upcoming workers
		vector<int> stoneUpcoming(game.planets.size(), 0);

		for(int i = 0; i < game.planets.size(); i++)
		{
			upcomingWorkers[i] = max(observer.ours[i] + fc.onFlightTo(i),0);
			if(game.planets[i].resources.count(model::Resource::STONE))
			{
				stoneUpcoming[i] = game.planets[i].resources.at(t2r(STONE));
			}
		}

		for(auto fg: fc.groups)
		{
			if (!fg.isFinished && fg.path.size() != 1 && fg.res.has_value() && fg.res.value() == model::Resource::STONE)
			{
				stoneUpcoming[fg.to] += fg.num; //stone coming to the planet
			}
		}

		if (!prodCycle.isBuilt && role == WORKER) //ща будем базу строить
		{
			int freestone = min(game.planets[homePlanet].resources.count(t2r(STONE)) ?
								game.planets[homePlanet].resources.at(t2r(STONE)) : 0,
								!game.planets[homePlanet].workerGroups.empty() ?
								game.planets[homePlanet].workerGroups[0].number : 0);

			for (int i = 0; i < buildingOrder.size(); i++) {
				int price = game.buildingProperties.at(t2b(buildingOrder[i].second)).buildResources.at(
						model::Resource::STONE);
				if (isBuilt[i]) {
					if (game.planets[buildingOrder[i].first].workerGroups.size() > 0
						&& game.planets[buildingOrder[i].first].workerGroups[0].playerIndex == game.myIndex) {
						/*fc.send(buildingOrder[i].first, homePlanet,
								game.planets[buildingOrder[i].first].workerGroups[0].number, {},
								true); //if there are workers on a planet with a building then order them back*/
					}
				} else if (stoneUpcoming[buildingOrder[i].first] < price) {
					if (freestone >= price) //if we can build it
					{
						fc.send(homePlanet, buildingOrder[i].first, price, model::Resource::STONE, IGNORANCE);
						freestone -= price;
						buildActions.push_back(
								model::BuildingAction(buildingOrder[i].first, t2b(buildingOrder[i].second)));

					}
					else{
						needWorkers[homePlanet] += price; //if there is not enough stone then we need some workers back to the homePlanet
						freestone = 0;
					}
				} else {
					needWorkers[buildingOrder[i].first] += 20;
					buildActions.push_back(model::BuildingAction(buildingOrder[i].first, t2b(buildingOrder[i].second)));
				}
			}
		}

		if(role == WORKER)
		{
			if(prodCycle.isBuilt)
			{
				for(int i = 0; i < prodCycle.buildingPlanet.size(); i++) //ordering workers to go to the buildings
				{
					for(int j = 0; j < prodCycle.buildingPlanet[i].size(); j++)
					{
						needWorkers[prodCycle.buildingPlanet[i][j]] = prodCycle.buildingWorkpower[i]/prodCycle.buildingPlanet[i].size()/1.2;
						//cout << prodCycle.buildingPlanet[i][j] << ":" << needWorkers[prodCycle.buildingPlanet[i][j]] << "\n";
					}
				}
			}
			//cout << upcomingWorkers[0] << " " << needWorkers[0] << "\n";


			for(int i = 0; i < game.planets.size(); i++)
			{
				if(needWorkers[i] > 0)
				{
					needWorkers[i] += fc.onFlightAt(i);
				}
				needWorkers[i] -= upcomingWorkers[i];
			}
 /*
			for(int i = 0; i < buildingOrder.size(); i++)
			{
				if(isBuilt[i]) continue;
				cout << buildingOrder[i].first << ": " << stoneUpcoming[buildingOrder[i].first] << " "<<needWorkers[buildingOrder[i].first] << "| ";
			}
			cout << "\n";*/
			for(int i = 0; i < needWorkers.size(); i++)
			{
				if(needWorkers[i] > 0)
				{
					for(int id = 0; id < game.planets.size(); id++)
					{
						if(needWorkers[i] <= 0) break;
						if(needWorkers[id] < 0)
						{
							int cantake = min((int)ceil(min(-needWorkers[id], needWorkers[i])), observer.ours[id]);
							if(cantake != 0)
							{
								fc.send(id,i,cantake, {}, IGNORANCE);
								needWorkers[i] -= cantake;
								needWorkers[id] += cantake;
							}
						}
					}
				}
			}
		}
	}

	if (prodCycle.isBuilt) {
		/*if (resetTimer > 100) {
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
		}*/

		if (role == LOGIST) {
			for (int i = 0; i < prodCycle.buildingPlanet.size(); ++i) {
				for(int j = 0; j < prodCycle.buildingPlanet[i].size(); j++)
				{
					prodCycle.sendRobots(game, fc, observer, prodCycle.buildingPlanet[i][j]);
				}
			}
			//cout << "sendRobots is done\n";

			double available = observer.ours[homePlanet]; 
			while(available > 1000-prodCycle.logistsNum)
			{
				for(int i = 0; i < 3; i++)
				{
					
					double controlsum = 0; //number of logists that has to be flying to ith buildings
					for(int j = 0; j < prodCycle.resourceTraffic.size(); j++)
					{
						controlsum += prodCycle.resourceTraffic[j][i];
					}
					/*double num = 0; //actually flying
					for(int j = 0; j < prodCycle.buildingPlanet[i].size(); j++)
					{
						num += fc.onFlightTo(prodCycle.buildingPlanet[i][j]); //currently flying to the ith jth building
					}
					
					double needed = controlsum-num;
					if(needed > 0)
					{*/
					double bneeded = controlsum/prodCycle.buildingPlanet[i].size(); //needed for one ith building

					for(int j = 0; j < prodCycle.buildingPlanet[i].size(); j++)
					{
						if(available <= 1000-prodCycle.logistsNum) break;
						fc.send(homePlanet, prodCycle.buildingPlanet[i][j], min(available,bneeded), {}, AVOIDANCE);
						available -= bneeded;
					}
					//}
				}
			}
		}
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
		if (game.players[i].teamIndex == game.players[game.myIndex].teamIndex) {
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

	if(role != LOGIST)
	{
		fc.setup(planetDists, &observer);
		fc.updateAdj(game, game.maxTravelDistance);
	}
	else
	{
		fc.setup(logDists, &observer);
		fc.updateAdj(game, game.maxTravelDistance+game.logisticsUpgrade);
	}

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
