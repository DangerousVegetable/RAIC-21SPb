#include "FlyingController.h"

FlyingController::FlyingController() {
}

void FlyingController::setup(vector<vector<int>> distances, Observer* observer) {
	d = distances;
	this->observer = observer;
}

vector<int> FlyingController::findPathDijkstra(FlyingGroup group) {
	int INF = 1e7;
	vector<int> distances(observer->size(), INF);
	vector<int> prev(observer->size(), -1);
	distances[group.fr] = 0;
	prev[group.fr] = -1;

	priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> q;
	q.push({0, group.fr});
	while (!q.empty()) {
		pair<int, int> p = q.top();
		q.pop();

		int u = p.second, dist = p.first;

		if (dist > distances[u])
			continue;

		for (int v: adj[u]) {
			int next_dist = dist + d[v][u];

			if (!observer->isOurs(v))
				next_dist += 1000;

			if (next_dist < distances[v]) {
				distances[v] = next_dist;
				q.push({next_dist, v});
				prev[v] = u;
			}
		}
	}

	vector<int> path;
	int v = group.to;
	while (v != -1) {
		path.push_back(v);
		v = prev[v];
	}
	reverse(path.begin(), path.end());

	return path;
}

vector<int> FlyingController::findPath(FlyingGroup group) {
	int to = group.to;
	int fr = group.fr;

	vector<int> path = {to};

	int prev = to;
	int oldPrev = prev;
	while (prev != fr) {
		oldPrev = prev;
		vector<int> vertices = (group.safety == IGNORANCE ? adj[prev] : safeAdj[prev]);
		for (int v: vertices) {
			if (d[fr][v] + d[v][prev] == d[fr][prev] && v != prev) {
				path.push_back(v);
				prev = v;
			}
		}

		if (prev == oldPrev)
			return findPathDijkstra(group);
	}

	reverse(path.begin(), path.end());

	return path;
}

int FlyingController::onFlightAt(int planet) {
	int res = 0;
	for (FlyingGroup group: groups)
		if (!group.isFinished && group.path.size() != 1 && (group.path[0] == planet && group.timeToNext == 1))
			res += group.num;

	return res;
}

int FlyingController::onFlightTo(int planet) {
	int res = 0;
	for (FlyingGroup group: groups)
		if (!group.isFinished && group.path.size() != 1 && group.to == planet)
			res += group.num;

	return res;
}

void FlyingController::send(int fr, int to, int num, optional<model::Resource> res, int safetyMode) {
	int availableToSend = observer->ours[fr] - onFlightAt(fr);
	if (availableToSend <= 0)
		return;
	
	num = min(availableToSend, num);
	FlyingGroup group(fr, to, num, res, safetyMode);
	group.setPath(findPath(group), d);

	groups.push_back(group);
}

// TODO: Remove finished groups
vector<model::MoveAction> FlyingController::update() {
	vector<model::MoveAction> moves;

	for (FlyingGroup& group: groups) {
		if (group.isFinished)
			continue;

		if (group.safety == AVOIDANCE && !observer->isOurs(group.path[1])) {
			group.fr = group.path[0];
			group.setPath(findPath(group), d);

			if (!observer->isOurs(group.path[1]))
				continue;
		}

		optional<model::MoveAction> move = group.step(d);
		if (move.has_value()) {
			if (group.safety == IGNORANCE)
				moves.push_back(move.value());
			else {
				group.fr = group.path[0];
				group.setPath(findPath(group), d);
				group.step(d);

				if (move.has_value())
					moves.push_back(move.value());
			}
		}
	}

	for (int i = 0; i < groups.size(); i++)
	{
		while (groups[i].isFinished)
		{
			groups.erase(groups.begin() + i);
			if (i == groups.size())
				break;
		}
	}

	return moves;
}

void FlyingController::updateAdj(const model::Game& game, int maxTravelDistance) {
	adj.clear();
	for (int i = 0; i < game.planets.size(); ++i) {
		adj.push_back({});
		for (int j = 0; j < game.planets.size(); ++j) {
			if (i == j)
				continue;

			if ((abs(game.planets[i].x - game.planets[j].x) + abs(game.planets[i].y - game.planets[j].y) <=
				 maxTravelDistance))
				adj[i].push_back(j);
		}
	}

	safeAdj = adj;
}

void FlyingController::updateSafeAdj(const model::Game& game, int maxTravelDistance) {
	safeAdj.clear();
	for (int i = 0; i < game.planets.size(); ++i) {
		safeAdj.push_back({});

		if (!observer->isOurs(i))
			continue;

		for (int j = 0; j < game.planets.size(); ++j) {
			if (i == j)
				continue;

			if (!observer->isOurs(j))
				continue;

			if ((abs(game.planets[i].x - game.planets[j].x) + abs(game.planets[i].y - game.planets[j].y) <=
				 maxTravelDistance))
				safeAdj[i].push_back(j);
		}
	}
}
