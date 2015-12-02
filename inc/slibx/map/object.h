#ifndef CHECKHEADER_SLIB_MAP_OBJECT
#define CHECKHEADER_SLIB_MAP_OBJECT

#include "definition.h"

#include <slib/math/geograph.h>
#include <slib/render/texture.h>
#include <slib/graphics/freetype.h>

SLIB_MAP_NAMESPACE_BEGIN

class SLIB_EXPORT MapMarker : public Referable
{
public:
	String key;

	GeoLocation location;
	String text;
	Color textColor;
	Ref<FreeType> textFont;
	sl_uint32 textFontSize;

	Size iconSize;
	Ref<Texture> iconTexture;
	Rectangle iconTextureRectangle; // texture whole coordinate
	sl_bool flagVisible;

public:
	Ref<Texture> _textureText;

public:
	void invalidate()
	{
		_textureText.setNull();
	}
};


class SLIB_EXPORT MapIcon : public Referable
{
public:
	String key;
	GeoLocation location;
	Size iconSize;
	sl_real rotationAngle;
	Ref<Texture> iconTexture;
	Rectangle iconTextureRectangle; // texture whole coordinate
	
	sl_bool flagVisible;

};


class SLIB_EXPORT MapPolygon : public Referable
{
public:
	String key;
	List<GeoLocation> points;
	Color color;
	float width;
};

SLIB_MAP_NAMESPACE_END

#endif