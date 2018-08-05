#include "decomposition.hpp"
#include "Box.hpp"
#include <vector>
#include <algorithm>
#include <set>
#include <iostream>
#include <unordered_map>
#include <cassert>

using namespace std;

struct Event {
	int pos = -1;
	int idx = -1;
	bool add = false;

	bool operator<(const Event& e) const {
		if (pos==e.pos) return add>e.add;
		return pos<e.pos;
	}
};

constexpr int X_AXIS = 0;
constexpr int Y_AXIS = 1;

constexpr int UP = 0;
constexpr int DOWN = 1;

struct DecomposeNode {
	Range xRange;
	int yStart = 0;

	bool operator<(const DecomposeNode& n) const {
		return xRange.from < n.xRange.from;
	}
};
bool operator<(const DecomposeNode& n, int i) {
	return n.xRange.from < i;
}
bool operator<(int i, const DecomposeNode& n) {
	return i < n.xRange.from;
}

template<>
Decomposition<2> decomposeFreeSpace<2>(const ObstacleSet<2>& obstacles) {
	vector<Event> events;
	for(int i=0; i<(int)obstacles.size(); ++i) {
		const auto& obs = obstacles[i];
		if (obs.box[X_AXIS].size() == 0) continue;
		events.push_back({obs.box[Y_AXIS].from, i, obs.direction == UP});
	}
	sort(events.begin(), events.end());

	set<DecomposeNode, less<>> nodeSet;
	Decomposition<2> decomposition;
	for(Event event : events) {
		const Range range = obstacles[event.idx].box[X_AXIS];
		if (event.add) {
			auto it = nodeSet.upper_bound(range.from);
			assert(it != nodeSet.begin());
			--it;
			Box<2> box{it->xRange, {it->yStart, event.pos}};
			decomposition.emplace_back(box);
		} else {
			Range totalRange = range;
			DecomposeNode node{totalRange, event.pos};
			nodeSet.insert(std::move(node));
		}
	}
	return decomposition;
}

template<int D>
Decomposition<D> decomposeFreeSpace(const ObstacleSet<D>& obstacles) {
	return {};
}

template
Decomposition<3> decomposeFreeSpace<3>(const ObstacleSet<3>& obstacles);
