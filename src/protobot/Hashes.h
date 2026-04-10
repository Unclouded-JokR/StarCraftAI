#pragma once
#include <BWAPI.h>
#include <bwem.h>

// Hash for storing units as keys
struct unitHash {
	std::size_t operator()(const BWAPI::Unit& unit) const {
		return std::hash<int>{}(unit->getID());
	}
};

// Hash for storing BWAPI::TilePositions 
struct PositionHash {
	std::size_t operator()(const BWAPI::Position& tile) const {
		return std::hash<int>()(0.5 * (tile.x + tile.y) * (tile.x + tile.y + 1) + tile.y);
	}
};

// Hash for storing BWAPI::TilePositions 
struct TilePositionHash {
	std::size_t operator()(const BWAPI::TilePosition& tile) const {
		return std::hash<int>()(0.5 * (tile.x + tile.y) * (tile.x + tile.y + 1) + tile.y);
	}
};

// Hash for storing AreaId pairs as keys
struct AreaIdPairHash {
	std::size_t operator()(const std::pair<BWEM::Area::id, BWEM::Area::id>& v) const {
		return std::hash<int>{}(v.first) ^ std::hash<int>{}(v.second);
	}
};

// Hash used for storing <AreaId, Area> pairs
struct AreaIdHash {
	std::size_t operator()(const BWEM::Area::id v) const {
		return std::hash<int>{}(v);
	}
};