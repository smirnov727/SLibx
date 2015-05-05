#include "map_tile_building.h"
#include "map_tile_config.h"
#include "map_data_config.h"

SLIB_MAP_NAMESPACE_START

MapBuildingTileManager::MapBuildingTileManager()
{
	setTileLifeMillseconds(SLIB_MAP_TILE_LIFE_MILLISECONDS);
	setMaxTilesCount(SLIB_MAP_MAX_BUILDING_TILES_COUNT);
	setMaxBuildingsCount(SLIB_MAP_MAX_BUILDINGS_COUNT);
	setMaxDetailedBuildingsCount(SLIB_MAP_MAX_DETAILED_BUILDINGS_COUNT);
}

Ref<MapBuildingTile> MapBuildingTileManager::getTile(const MapTileLocationi& location)
{
	Ref<MapBuildingTile> tile;
	m_tiles.get(location, &tile);
	if (tile.isNotNull()) {
		tile->timeLastAccess = Time::now();
	}
	return tile;
}

Ref<MapBuildingTile> MapBuildingTileManager::loadTile(const MapTileLocationi& location)
{
	Ref<MapBuildingTile> tile;
	m_tiles.get(location, &tile);
	if (tile.isNotNull()) {
		tile->timeLastAccess = Time::now();
		return tile;
	}
	Ref<MapDataLoader> loader = getDataLoader();
	if (loader.isNotNull()) {

		List< Ref<VW_Building_ObjectInfo> > objects;
		objects.add(VW_Building::readTile(loader, SLIB_MAP_BUILDING_TILE_TYPE, location, sl_false));
		objects.add(VW_Building::readTile(loader, SLIB_MAP_BUILDING_WORLD_TILE_TYPE, location, sl_false));
		objects.add(VW_Building::readTile(loader, SLIB_MAP_BRIDGE_TILE_TYPE, location, sl_true));

		tile = new MapBuildingTile();
		if (tile.isNotNull()) {
			tile->location = location;
			tile->objects = objects;
			tile->timeLastAccess = Time::now();
			m_tiles.put(location, tile);
			return tile;
		}
	}
	return Ref<MapBuildingTile>::null();
}

void MapBuildingTileManager::freeOldTiles()
{
	class SortTile
	{
	public:
		SLIB_INLINE static Time key(Ref<MapBuildingTile>& tile)
		{
			return tile->timeLastAccess;
		}
	};

	sl_int64 timeLimit = getTileLifeMillseconds();
	sl_uint32 tileLimit = getMaxTilesCount();
	Time now = Time::now();

	List< Ref<MapBuildingTile> > tiles;
	{
		ListLocker< Ref<MapBuildingTile> > t(m_tiles.values());
		for (sl_size i = 0; i < t.count(); i++) {
			Ref<MapBuildingTile>& tile = t[i];
			if (tile.isNotNull()) {
				if ((now - tile->timeLastAccess).getMillisecondsCount() < timeLimit) {
					tiles.add(tile);
				} else {
					m_tiles.remove(tile->location);
				}
			}
		}
	}
	tiles = tiles.sort<SortTile, Time>(sl_false);
	{
		ListLocker< Ref<MapBuildingTile> > t(tiles);
		for (sl_size i = tileLimit; i < t.count(); i++) {
			Ref<MapBuildingTile>& tile = t[i];
			m_tiles.remove(tile->location);
		}
	}
}

Ref<MapBuilding> MapBuildingTileManager::getBuilding(String key)
{
	Ref<MapBuilding> building;
	m_buildings.get(key, &building);
	if (building.isNotNull()) {
		building->timeLastAccess = Time::now();
	}
	return building;
}

Ref<MapBuilding> MapBuildingTileManager::loadBuilding(VW_Building_ObjectInfo* info)
{
	Ref<MapBuilding> building;
	m_buildings.get(info->key, &building);
	if (building.isNotNull()) {
		building->timeLastAccess = Time::now();
		return building;
	}
	Ref<MapDataLoader> loader = getDataLoader();
	if (loader.isNotNull()) {
		Ref<VW_Building_Object> vo = VW_Building::readObject(loader, info);
		if (vo.isNotNull()) {
			building = new MapBuilding;
			if (building.isNotNull()) {
				building->key = info->key;
				building->object = vo;
				building->info = info;
				building->timeLastAccess = Time::now();
				m_buildings.put(info->key, building);
				return building;
			}
		}
	}
	return Ref<MapBuilding>::null();
}

void MapBuildingTileManager::freeOldBuildings()
{
	class SortBuilding
	{
	public:
		SLIB_INLINE static Time key(Ref<MapBuilding>& tile)
		{
			return tile->timeLastAccess;
		}
	};

	sl_int64 timeLimit = getTileLifeMillseconds();
	sl_uint32 tileLimit = getMaxBuildingsCount() * 2;
	Time now = Time::now();

	List< Ref<MapBuilding> > buildings;
	{
		ListLocker< Ref<MapBuilding> > t(m_buildings.values());
		for (sl_size i = 0; i < t.count(); i++) {
			Ref<MapBuilding>& building = t[i];
			if (building.isNotNull()) {
				if ((now - building->timeLastAccess).getMillisecondsCount() < timeLimit) {
					buildings.add(building);
				} else {
					m_buildings.remove(building->key);
				}
			}
		}
	}
	buildings = buildings.sort<SortBuilding, Time>(sl_false);
	{
		ListLocker< Ref<MapBuilding> > t(buildings);
		for (sl_size i = tileLimit; i < t.count(); i++) {
			Ref<MapBuilding>& building = t[i];
			m_buildings.remove(building->key);
		}
	}
}


void MapBuildingTileManager::loadRequestedDetailedTextures()
{
	_BuildingTextureAddress addr = m_requestedDetailedTexture;
	if (addr.key.isNotEmpty()) {
		loadDetailedTexture(addr.key, addr.index);
		m_requestedDetailedTexture.key.setNull();
	}
}

Ref<Texture> MapBuildingTileManager::getDetailedTexture(String key, sl_uint32 index)
{
	String tkey = _getDetailedTextureKey(key, index);
	Ref<_BuildingTexture> bt;
	m_detailedTextures.get(tkey, &bt);
	if (bt.isNotNull()) {
		bt->timeLastAccess = Time::now();
		return bt->texture;
	}
	return Ref<Texture>::null();
}

Ref<Texture> MapBuildingTileManager::loadDetailedTexture(String key, sl_uint32 index)
{
	String tkey = _getDetailedTextureKey(key, index);
	Ref<_BuildingTexture> bt;
	m_detailedTextures.get(tkey, &bt);
	if (bt.isNotNull()) {
		bt->timeLastAccess = Time::now();
		return bt->texture;
	}
	Ref<MapDataLoader> loader = getDataLoader();
	if (loader.isNotNull()) {

		Ref<MapBuilding> rb = getBuilding(key);

		if (rb.isNotNull()) {
			VW_Building_Mesh mesh;
			if (rb->object->meshes.getItem(index, &mesh)) {

				Memory mem = loader->loadData(rb->info->dataType, rb->info->location, SLIB_MAP_BUILDING_PACKAGE_DIMENSION, "/" + mesh.textureFileName);
				if (mem.isNotEmpty()) {
					Ref<Texture> texture = Texture::loadFromMemory(mem);
					bt = new _BuildingTexture;
					if (bt.isNotNull()) {
						bt->tkey = tkey;
						bt->texture = texture;
						bt->timeLastAccess = Time::now();
						m_detailedTextures.put(tkey, bt);
						return texture;
					}
				}
			}

		}
	}
	return Ref<Texture>::null();
}

void MapBuildingTileManager::requestDetailedTexture(String key, sl_uint32 index)
{
	m_requestedDetailedTexture.key = key;
	m_requestedDetailedTexture.index = index;
}

String MapBuildingTileManager::_getDetailedTextureKey(String key, sl_uint32 index)
{
	return key + "_" + index;
}

void MapBuildingTileManager::freeOldDetailedTextures()
{
	class SortTexture
	{
	public:
		SLIB_INLINE static Time key(Ref<_BuildingTexture>& tile)
		{
			return tile->timeLastAccess;
		}
	};
	sl_int64 timeLimit = getTileLifeMillseconds();
	sl_uint32 tileLimit = getMaxDetailedBuildingsCount() * 2;
	Time now = Time::now();

	List< Ref<_BuildingTexture> > buildings;
	{
		ListLocker< Ref<_BuildingTexture> > t(m_detailedTextures.values());
		for (sl_size i = 0; i < t.count(); i++) {
			Ref<_BuildingTexture>& building = t[i];
			if (building.isNotNull()) {
				if ((now - building->timeLastAccess).getMillisecondsCount() < timeLimit) {
					buildings.add(building);
				} else {
					m_detailedTextures.remove(building->tkey);
				}
			}
		}
	}
	buildings = buildings.sort<SortTexture, Time>(sl_false);
	{
		ListLocker< Ref<_BuildingTexture> > t(buildings);
		for (sl_size i = tileLimit; i < t.count(); i++) {
			Ref<_BuildingTexture>& building = t[i];
			m_detailedTextures.remove(building->tkey);
		}
	}
}

SLIB_MAP_NAMESPACE_END

