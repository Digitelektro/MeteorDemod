#ifndef VECTOR_H
#define VECTOR_H

#include <SGP4.h>

#include "CoordGeodetic.h"

struct Vector3 : Vector {

  private:
    static constexpr double cEarthRadius = 6378.137;                 // Earth radius
    static constexpr double cFlatteningFactor = 1.0 / 298.257223563; // Flattening factor WGS84 Model

  public:
    Vector3()
        : Vector() {}

    Vector3(const CoordGeodetic& coordinate)
        : Vector() {
        double cosLat = cos(coordinate.latitude);
        double sinLat = sin(coordinate.latitude);
        double cosLon = cos(coordinate.longitude);
        double sinLon = sin(coordinate.longitude);

        double radB = cEarthRadius * (1 - cFlatteningFactor);

        double N = pow(cEarthRadius, 2) / sqrt(pow(cEarthRadius, 2) * pow(cosLat, 2) + pow(radB, 2) * pow(sinLat, 2));

        x = (N + coordinate.altitude) * cosLat * cosLon;
        y = (N + coordinate.altitude) * cosLat * sinLon;
        z = ((pow(radB, 2) / pow(cEarthRadius, 2)) * N + coordinate.altitude) * sinLat;
    }

    Vector3(const Vector& v)
        : Vector(v) {}

    Vector& operator+=(const Vector& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    Vector& operator-=(const Vector& v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }

    Vector3 operator*(double factor) const {
        Vector3 r(*this);
        r.x *= factor;
        r.y *= factor;
        r.z *= factor;
        return r;
    }

    Vector3& operator*=(double factor) {
        x *= factor;
        y *= factor;
        z *= factor;
        return *this;
    }

    Vector3 operator/(double factor) const {
        Vector3 r(*this);
        r.x /= factor;
        r.y /= factor;
        r.z /= factor;
        return r;
    }

    Vector3& operator/=(double factor) {
        x /= factor;
        y /= factor;
        z /= factor;
        return *this;
    }

    Vector3 Cross(const Vector& v) const {
        Vector3 r;
        r.x = y * v.z - z * v.y;
        r.y = z * v.x - x * v.z;
        r.z = x * v.y - y * v.x;
        return r;
    }

    Vector3& Normalize() {
        double m = Magnitude();
        if(m > 0) {
            return (*this) /= m;
        }
        return *this;
    }

    double DistanceSquared() const {
        return (x * x + y * y + z * z);
    }

    CoordGeodetic toCoordinate() {
        double b = cEarthRadius * (1 - cFlatteningFactor);
        double r = Magnitude();

        double lat = asin(z / r);
        double lon = atan2(y, x);

        double e = sqrt((pow(cEarthRadius, 2) - pow(b, 2)) / pow(cEarthRadius, 2));
        double e2 = sqrt((pow(cEarthRadius, 2) - pow(b, 2)) / pow(b, 2));
        double p = sqrt(pow(x, 2) + pow(y, 2));
        double phi = atan2(z * cEarthRadius, p * b);
        lat = atan2(z + pow(e2, 2) * b * pow(sin(phi), 3), p - pow(e, 2) * cEarthRadius * pow(cos(phi), 3));

        return CoordGeodetic(lat, lon, 0, true);
    }
};

#endif // VECTOR_H
