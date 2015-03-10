#include "dem.h"

#include "../../slib/core/io.h"
#include "../../slib/render/program.h"

SLIB_MAP_NAMESPACE_START
sl_bool DEM::initialize(sl_uint32 _N)
{
	Array<sl_geo_val> _array = Array<sl_geo_val>::create(_N * _N);
	if (_array.isNotEmpty()) {
		sl_geo_val* _dem = _array.getBuf();
		for (sl_size i = 0; i < _array.count(); i++) {
			_dem[i] = 0;
		}
		N = _N;
		array = _array;
		dem = _dem;
		return sl_true;
	}
	return sl_false;
}

sl_bool DEM::initializeFromFloatData(sl_uint32 _N, const void* _data, sl_size size)
{
	if (size != _N * _N * 4) {
		return sl_false;
	}
	if (!initialize(_N)) {
		return sl_false;
	}
	sl_geo_val* _dem = dem;
	sl_uint8* data = (sl_uint8*)_data;
	for (sl_uint32 y = 0; y < _N; y++) {
		for (sl_uint32 x = 0; x < _N; x++) {
			*_dem = MemoryIO::readFloat(data);
			data += 4;
			_dem++;
		}
	}
	return sl_true;
}

void DEM::makeMesh(Primitive& out, sl_uint32 M, const GeoRectangle& region, const Rectangle& rectDEM, const Rectangle& rectTexture)
{
	if (M <= 1) {
		return;
	}
	sl_geo_val lat0 = region.bottomLeft.latitude;
	sl_geo_val lon0 = region.bottomLeft.longitude;
	sl_geo_val lat1 = region.topRight.latitude;
	sl_geo_val lon1 = region.topRight.longitude;
	sl_geo_val dlat = lat1 - lat0;
	sl_geo_val dlon = lon1 - lon0;

	sl_real mx0 = rectDEM.left * (N - 1);
	sl_real my0 = rectDEM.top * (N - 1);
	sl_real mx1 = rectDEM.right * (N - 1);
	sl_real my1 = rectDEM.bottom * (N - 1);
	sl_real dmx = mx1 - mx0;
	sl_real dmy = my1 - my0;

	sl_real tx0 = rectTexture.left;
	sl_real ty0 = rectTexture.top;
	sl_real tx1 = rectTexture.right;
	sl_real ty1 = rectTexture.bottom;
	sl_real dtx = tx1 - tx0;
	sl_real dty = ty1 - ty0;

	GeoLocation loc;
	
	out.type = Primitive::typeTriangles;
	out.countElements = 6 * (M - 1) * (M - 1);
	SLIB_SCOPED_ARRAY(DEM_Vertex, vb, M*M);
	DEM_Vertex* pv = vb;
	for (sl_uint32 y = 0; y < M; y++) {
		for (sl_uint32 x = 0; x < M; x++) {
			loc.latitude = lat0 + dlat * (M - 1 - y) / (M - 1);
			loc.longitude = lon0 + dlon * x / (M - 1);
			if (N >= 2) {
				sl_real mx = mx0 + dmx * x / (M - 1);
				sl_real my = my0 + dmy * y / (M - 1);
				sl_int32 mxi = (sl_int32)(mx);
				sl_int32 myi = (sl_int32)(my);
				sl_real mxf;
				sl_real myf;
				if (mxi < 0) {
					mxi = 0;
					mxf = 0;
				} else if (mxi >= (sl_int32)N - 1) {
					mxi = N - 2;
					mxf = 1;
				} else {
					mxf = mx - mxi;
				}
				if (myi < 0) {
					myi = 0;
					myf = 0;
				} else if (myi >= (sl_int32)N - 1) {
					myi = N - 2;
					myf = 1;
				} else {
					myf = my - myi;
				}
				sl_int32 p = mxi + myi * N;
				loc.altitude =
					(1 - mxf) * (1 - myf) * dem[p]
					+ (1 - mxf) * myf * dem[p + N]
					+ mxf * (1 - myf) * dem[p + 1]
					+ mxf * myf * dem[p + 1 + N];
			} else {
				loc.altitude = 0;
			}
			pv->position = loc.convertToPosition();
			pv->altitude = (sl_real)(loc.altitude);
			pv->texCoord.x = tx0 + dtx * x / (M - 1);
			pv->texCoord.y = ty0 + dty * y / (M - 1);
			pv++;
		}
	}
	out.vertexBuffer = VertexBuffer::create(vb, sizeof(DEM_Vertex) * M * M);
	SLIB_SCOPED_ARRAY(sl_uint16, ib, out.countElements);
	sl_uint16* pi = ib;
	for (sl_uint32 y = 0; y < M - 1; y++) {
		for (sl_uint32 x = 0; x < M - 1; x++) {
			*(pi++) = (sl_uint16)(y * M + x);
			*(pi++) = (sl_uint16)(y * M + x + 1);
			*(pi++) = (sl_uint16)(y * M + x + M);
			*(pi++) = (sl_uint16)(y * M + x + 1);
			*(pi++) = (sl_uint16)(y * M + x + M);
			*(pi++) = (sl_uint16)(y * M + x + 1 + M);
		}
	}
	out.indexBuffer = IndexBuffer::create(ib, out.countElements * sizeof(sl_uint16));
}

SLIB_MAP_NAMESPACE_END