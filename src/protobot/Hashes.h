#pragma once
#include <BWAPI.h>
#include <bwem.h>

/// <summary>
/// Hash for storing units as keys
/// </summary>
struct unitHash {
	std::size_t operator()(const BWAPI::Unit& unit) const {
		return std::hash<int>{}(unit->getID());
	}
};

/// <summary>
/// Hash for storing BWAPI::TilePositions 
/// </summary>
struct PositionHash {
	std::size_t operator()(const BWAPI::Position& tile) const {
		return std::hash<int>()(0.5 * (tile.x + tile.y) * (tile.x + tile.y + 1) + tile.y);
	}
};

/// <summary>
/// Hash for storing BWAPI::TilePositions 
/// </summary>
struct TilePositionHash {
	std::size_t operator()(const BWAPI::TilePosition& tile) const {
		return std::hash<int>()(0.5 * (tile.x + tile.y) * (tile.x + tile.y + 1) + tile.y);
	}
};

/// <summary>
/// Hash for storing AreaId pairs as keys
/// </summary>
struct AreaIdPairHash {
	std::size_t operator()(const std::pair<BWEM::Area::id, BWEM::Area::id>& v) const {
		return std::hash<int>{}(v.first) ^ std::hash<int>{}(v.second);
	}
};

/// <summary>
/// Hash used for storing <AreaId, Area> pairs
/// </summary>
struct AreaIdHash {
	std::size_t operator()(const BWEM::Area::id v) const {
		return std::hash<int>{}(v);
	}
};