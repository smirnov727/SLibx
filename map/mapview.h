#ifndef CHECKHEADER_SLIB_MAP_MAPVIEW
#define CHECKHEADER_SLIB_MAP_MAPVIEW

#include "definition.h"

#include "earth_renderer.h"
#include "data.h"
#include "tile_dem.h"

#include "../../slib/ui/view.h"
#include "../../slib/image/freetype.h"

SLIB_MAP_NAMESPACE_START

class MapView : public RenderView
{
public:
	MapView();
	~MapView();

public:
	void initialize();
	virtual void release();

	Ref<MapCamera> getCamera()
	{
		return m_earthRenderer.getCamera();
	}
	MapEarthRenderer& getEarthRenderer()
	{
		return m_earthRenderer;
	}

	Ref<FreeType> getFontForPOI();
	void setFontForPOI(Ref<FreeType> font);
	void setPoiInformation(Map<sl_int64, Variant> poiInformation);
	void setWayNames(Map<sl_int64, String> wayNames);

	Ref<MapMarker> getMarker(String key);
	void putMarker(String key, Ref<MapMarker> marker);
	void removeMarker(String key);
	void putAdditionalMarker(String key, Ref<MapMarker> marker);
	void clearAdditionalMarkers();

	Ref<MapPolygon> getPolygon(String key);
	void putPolygon(String key, Ref<MapPolygon> polygon);
	void putAdditionalPolygon(String key, Ref<MapPolygon> polygon);
	void removePolygon(String key);
	void clearAdditionalPolygons();

protected:
	virtual void onFrame(RenderEngine* engine);
	virtual sl_bool onMouseEvent(MouseEvent& event);
	virtual sl_bool onMouseWheelEvent(MouseWheelEvent& event);

	virtual String formatLatitude(double f);
	virtual String formatLongitude(double f);
	virtual String formatAltitude(double f);
	virtual String getStatusText();

protected:
	Ref<MapDEMTileManager> getDEMTiles();

	void _zoom(double ratio);

private:
	sl_bool m_flagInit;

	MapEarthRenderer m_earthRenderer;

	Ref<Texture> m_textureStatus;

	sl_real m_viewportWidth;
	sl_real m_viewportHeight;

	Point m_pointMouseBefore;
	Point m_pointMouseBefore2;
	sl_bool m_flagTouchBefore2;

	LatLon m_locationMouseDown;
	Point m_pointMouseDown;
	Time m_timeMouseDown;
	Matrix4 m_transformMouseDown;
	sl_bool m_flagMouseExitMoving;
	sl_bool m_flagMouseDown;

	sl_bool m_flagCompassHighlight;

public:
	SLIB_PROPERTY_INLINE(Ref<FreeType>, StatusFont);
	SLIB_PROPERTY_INLINE(Ref<MapDataLoader>, DataLoader);

	SLIB_PROPERTY_INLINE(Ref<Texture>, CompassTexture);
	SLIB_PROPERTY_INLINE(Rectangle, CompassTextureRectangle);
	SLIB_PROPERTY_INLINE(Ref<Texture>, CompassHighlightTexture);
	SLIB_PROPERTY_INLINE(Rectangle, CompassHighlightTextureRectangle);
	SLIB_PROPERTY_INLINE(Point, CompassPosition);
	SLIB_PROPERTY_INLINE(sl_real, CompassSize);

};
SLIB_MAP_NAMESPACE_END

#endif

