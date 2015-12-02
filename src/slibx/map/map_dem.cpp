#include "../../../inc/slibx/map/dem.h"

#include <slib/core/io.h>
#include <slib/core/scoped_pointer.h>
#include <slib/render/program.h>

SLIB_MAP_NAMESPACE_BEGIN
sl_bool DEM::initialize(sl_uint32 _N)
{
	Array<float> _array = Array<float>::create(_N * _N);
	if (_array.isNotNull()) {
		float* _dem = _array.getBuf();
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
	float* _dem = dem;
	sl_uint8* data = (sl_uint8*)_data;
	for (sl_uint32 y = 0; y < _N; y++) {
		for (sl_uint32 x = 0; x < _N; x++) {
			*_dem = MIO::readFloat(data);
			data += 4;
			_dem++;
		}
	}
	return sl_true;
}

void DEM::scaleDEM(float* o, sl_uint32 M, const Rectangle& rectDEM) const
{
	if (M <= 1) {
		return;
	}
	sl_real mx0 = rectDEM.left * (N - 1);
	sl_real my0 = rectDEM.top * (N - 1);
	sl_real mx1 = rectDEM.right * (N - 1);
	sl_real my1 = rectDEM.bottom * (N - 1);
	sl_real dmx = mx1 - mx0;
	sl_real dmy = my1 - my0;
	
	float* po = o;
	for (sl_uint32 y = 0; y < M; y++) {
		for (sl_uint32 x = 0; x < M; x++) {
			float altitude;
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
				altitude =
					(1 - mxf) * (1 - myf) * dem[p]
					+ (1 - mxf) * myf * dem[p + N]
					+ mxf * (1 - myf) * dem[p + 1]
					+ mxf * myf * dem[p + 1 + N];
			} else {
				altitude = 0;
			}
			*po = altitude;
			po++;
		}
	}
}

template <class GLOBE>
void _DEM_makeMesh(const DEM* _dem, const GLOBE& globe, const Vector3lf& positionCenter, Primitive& out, sl_uint32 M
	, const GeoRectangle& region, const Rectangle& rectDEM
	, const Rectangle& rectTexture, const Rectangle rectTextureLayers[SLIB_SMAP_MAX_LAYERS_COUNT])
{
	if (M <= 1) {
		return;
	}

	float* dem = _dem->dem;
	sl_uint32 N = _dem->N;

	double lat0 = region.bottomLeft.latitude;
	double lon0 = region.bottomLeft.longitude;
	double lat1 = region.topRight.latitude;
	double lon1 = region.topRight.longitude;
	double dlat = lat1 - lat0;
	double dlon = lon1 - lon0;

	sl_real mx0 = rectDEM.left * (N - 1);
	sl_real my0 = rectDEM.top * (N - 1);
	sl_real mx1 = rectDEM.right * (N - 1);
	sl_real my1 = rectDEM.bottom * (N - 1);
	sl_real dmx = mx1 - mx0;
	sl_real dmy = my1 - my0;

	sl_real tx0 = rectTexture.left;
	sl_real ty0 = rectTexture.top;
	sl_real dtx = rectTexture.getWidth();
	sl_real dty = rectTexture.getHeight();

	sl_real layer_tx0[SLIB_SMAP_MAX_LAYERS_COUNT];
	sl_real layer_ty0[SLIB_SMAP_MAX_LAYERS_COUNT];
	sl_real layer_dtx[SLIB_SMAP_MAX_LAYERS_COUNT];
	sl_real layer_dty[SLIB_SMAP_MAX_LAYERS_COUNT];
	for (sl_uint32 layer = 0; layer < SLIB_SMAP_MAX_LAYERS_COUNT; layer++) {
		layer_tx0[layer] = rectTextureLayers[layer].left;
		layer_ty0[layer] = rectTextureLayers[layer].top;
		layer_dtx[layer] = rectTextureLayers[layer].getWidth();
		layer_dty[layer] = rectTextureLayers[layer].getHeight();
	}
	
	GeoLocation loc;
	
	out.type = Primitive::typeTriangles;
	out.countElements = 6 * (M - 1) * (M - 1);
	SLIB_SCOPED_BUFFER(DEM_Vertex, 4096, vb, M*M);
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
			pv->position = globe.getCartesianPosition(loc) - positionCenter;
			pv->altitude = (sl_real)(loc.altitude);
			pv->texCoord.x = tx0 + dtx * x / (M - 1);
			pv->texCoord.y = ty0 + dty * y / (M - 1);
			for (sl_uint32 layer = 0; layer < SLIB_SMAP_MAX_LAYERS_COUNT; layer++) {
				pv->texCoordLayers[layer].x = layer_tx0[layer] + layer_dtx[layer] * x / (M - 1);
				pv->texCoordLayers[layer].y = layer_ty0[layer] + layer_dty[layer] * y / (M - 1);
			}
			pv++;
		}
	}

	out.vertexBuffer = VertexBuffer::create(vb, sizeof(DEM_Vertex) * M * M);
	SLIB_SCOPED_BUFFER(sl_uint16, 4096, ib, out.countElements);
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

void DEM::makeMeshFromGlobe(const Globe& globe, const Vector3lf& positionCenter, Primitive& out, sl_uint32 M
	, const GeoRectangle& region, const Rectangle& rectDEM
	, const Rectangle& rectTexture
	, const Rectangle rectTextureLayers[SLIB_SMAP_MAX_LAYERS_COUNT]) const
{
	_DEM_makeMesh(this, globe, positionCenter, out, M, region, rectDEM, rectTexture, rectTextureLayers);
}

void DEM::makeMeshFromSphericalGlobe(const SphericalGlobe& globe, const Vector3lf& positionCenter, Primitive& out, sl_uint32 M
	, const GeoRectangle& region, const Rectangle& rectDEM
	, const Rectangle& rectTexture
	, const Rectangle rectTextureLayers[SLIB_SMAP_MAX_LAYERS_COUNT]) const
{
	_DEM_makeMesh(this, globe, positionCenter, out, M, region, rectDEM, rectTexture, rectTextureLayers);
}

float DEM::getAltitudeAt(float x, float y)
{
	if (N == 0) {
		return 0;
	} else if (N == 1) {
		return dem[0];
	} else {
		sl_real mx = x * (N - 1);
		sl_real my = y * (N - 1);
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
		float altitude =
			(1 - mxf) * (1 - myf) * dem[p]
			+ (1 - mxf) * myf * dem[p + N]
			+ mxf * (1 - myf) * dem[p + 1]
			+ mxf * myf * dem[p + 1 + N];
		return altitude;
	}
}
SLIB_MAP_NAMESPACE_END