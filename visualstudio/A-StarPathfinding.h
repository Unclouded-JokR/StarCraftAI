#include <BWAPI.h>

using namespace std;

struct Node {
	BWAPI::TilePosition tile, parent;
	double gCost, hCost, fCost;

	Node(BWAPI::TilePosition tile, BWAPI::TilePosition parent, double gCost, double hCost, double fCost) {
		this->tile = tile;
		this->parent = parent;
		this->gCost = gCost;
		this->hCost = hCost;
		this->fCost = fCost;
	};

	// Default Node constructor has an invalid BWAPI TilePosition value
	Node() {
		tile = BWAPI::TilePosition(-1, -1);
	};
};

class Path {
	BWAPI::Unit unit;
	BWAPI::TilePosition start;
	BWAPI::TilePosition	end;

	public:
		vector<BWAPI::TilePosition> tiles;
		bool reachable;

		// Initializes the Path with the units start position and the target end position
		// Also initializes whether the path is reachable (default would be false until generated);
		Path(BWAPI::Unit unit, BWAPI::Position end) {
			this->start = BWAPI::TilePosition(unit->getPosition());
			this->end = BWAPI::TilePosition(end);
			this->reachable = false;
			this->tiles = {};
		}

		void generateAStarPath();

		vector<Node> getNeighbours(Node node);
};