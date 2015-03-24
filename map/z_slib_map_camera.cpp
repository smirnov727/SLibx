#include "camera.h"
#include "earth.h"

SLIB_MAP_NAMESPACE_START
MapCamera::MapCamera() : m_location(0, 0, 0)
{
	m_tilt = 0;
	m_rotationZ = 0;

	m_flagValidMatrixView = sl_false;
}

void MapCamera::setEyeLocation(const GeoLocation& eye)
{
	m_location = eye;
	m_flagValidMatrixView = sl_false;
}

void MapCamera::setTilt(sl_real angle)
{
	m_tilt = angle;
	m_flagValidMatrixView = sl_false;
}

void MapCamera::setRotationZ(sl_real angle)
{
	m_rotationZ = angle;
	m_flagValidMatrixView = sl_false;
}

void MapCamera::move(double latitude, double longitude, double altitude)
{
	moveTo(m_location.latitude + latitude, m_location.longitude + longitude, m_location.altitude + altitude);
}

void MapCamera::moveTo(double latitude, double longitude, double altitude)
{
	m_location.altitude = altitude;
	m_location.latitude = latitude;
	if (m_location.latitude < -89) {
		m_location.latitude = -89;
	}
	if (m_location.latitude > 89) {
		m_location.latitude = 89;
	}
	m_location.longitude = longitude;
	while (m_location.longitude > 180) {
		m_location.longitude -= 360;
	}
	while (m_location.longitude < -180) {
		m_location.longitude += 360;
	}
	m_flagValidMatrixView = sl_false;
}

void MapCamera::zoom(double ratio, double _min, double _max)
{
	if (ratio > 0) {
		double h = m_location.altitude * ratio;
		if (h < _min) {
			h = _min;
		}
		if (h > _max) {
			h = _max;
		}
		m_location.altitude = h;
		m_flagValidMatrixView = sl_false;
	}
}

void MapCamera::rotateZ(sl_real angle)
{
	setRotationZ(m_rotationZ + angle);
}

void MapCamera::updateViewMatrix()
{
	if (!m_flagValidMatrixView) {
		GeoLocation locAt;
		locAt.altitude = 0;
		locAt.latitude = m_location.latitude;
		locAt.longitude = m_location.longitude;
		Vector3 at = MapEarth::getCartesianPosition(locAt);
		Vector3 eye = MapEarth::getCartesianPosition(m_location);
		Matrix4 t = Transform3::getLookAtMatrix(eye, at, Vector3::axisY());
		if (m_tilt > 0) {
			Vector3 raxis = (eye - at).cross(Vector3::axisY());
			Matrix4 m = Transform3::getTranslationMatrix(-at)
				* Transform3::getRotationMatrix(raxis, Math::getRadianFromDegree(m_tilt))
				* Transform3::getTranslationMatrix(at);
			eye = (Vector4(eye, 1.0f) * m).xyz();
			t = Transform3::getLookAtMatrix(eye, at, Vector3::axisY());			
		}
		m_matrixView = t;
		m_flagValidMatrixView = sl_true;
	}
}
SLIB_MAP_NAMESPACE_END
