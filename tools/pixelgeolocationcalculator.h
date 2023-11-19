#ifndef PIXELGEOLOCATIONCALCULATOR_H
#define PIXELGEOLOCATIONCALCULATOR_H
#include <CoordGeodetic.h>
#include <SGP4.h>
#include <math.h>

#include <list>
#include <string>
#include <vector>

#include "matrix.h"
#include "tlereader.h"
#include "vector.h"

inline static CoordGeodetic operator+(const CoordGeodetic& coord1, const CoordGeodetic& coord2) {
    CoordGeodetic result(0, 0, 0);
    result.latitude = coord1.latitude + coord2.latitude;
    result.longitude = coord1.longitude + coord2.longitude;
    return result;
}

class PixelGeolocationCalculator {
  public:
    template <typename T>
    struct CartesianCoordinate {
        T x;
        T y;
    };

    template <typename T>
    friend std::ostream& operator<<(std::ostream& o, const CartesianCoordinate<T>& coord) {
        return o << "x: " << coord.x << "\ty: " << coord.y;
    }

    typedef CartesianCoordinate<float> CartesianCoordinateF;
    typedef CartesianCoordinate<double> CartesianCoordinateD;

  private:
    PixelGeolocationCalculator();

  public:
    PixelGeolocationCalculator(const TleReader::TLE& tle,
                               const DateTime& passStart,
                               const TimeSpan& passLength,
                               double scanAngle,
                               double roll,
                               double pitch,
                               double yaw,
                               int imageWidth,
                               int imageHeight,
                               int earthRadius = 6378,
                               int satelliteAltitude = 825);


    const CoordGeodetic& getCenterCoordinate() const;
    CoordGeodetic getCoordinateAt(unsigned int x, unsigned int y) const;
    CoordGeodetic getCoordinateTopLeft() const;
    CoordGeodetic getCoordinateTopRight() const;
    CoordGeodetic getCoordinateBottomLeft() const;
    CoordGeodetic getCoordinateBottomRight() const;

    bool isNorthBoundPass() const;

    inline int getEarthRadius() const {
        return mEarthradius;
    }

    inline int getSatelliteAltitude() const {
        return mSatelliteAltitude;
    }

    inline int getSatelliteHeight() const {
        return mEarthradius + mSatelliteAltitude;
    }

  public:
    template <typename T>
    static CartesianCoordinate<T> coordinateToMercatorProjection(const CoordGeodetic& coordinate, double radius, float scale, float offset = 0.0f) {
        CartesianCoordinate<T> cartesianCoordinate;
        CoordGeodetic correctedCoordinate = coordinate;

        if(coordinate.latitude > degreeToRadian(85.05113)) {
            correctedCoordinate.latitude = degreeToRadian(85.05113);
        } else if(coordinate.latitude < degreeToRadian(-85.05113)) {
            correctedCoordinate.latitude = degreeToRadian(-85.05113);
        }

        cartesianCoordinate.x = radius * (M_PI + normalizeLongitude(correctedCoordinate.longitude + degreeToRadian(offset)));
        cartesianCoordinate.y = radius * (M_PI - log(tan(M_PI / 4.0 + (correctedCoordinate.latitude) / 2.0)));
        return {cartesianCoordinate.x * scale, cartesianCoordinate.y * scale};
    }

    template <typename T>
    static CartesianCoordinate<T> coordinateToAzimuthalEquidistantProjection(const CoordGeodetic& coordinate, const CoordGeodetic& centerCoordinate, double radius, float scale) {
        T x = radius * (cos(coordinate.latitude) * sin(coordinate.longitude - centerCoordinate.longitude));
        T y = -radius * (cos(centerCoordinate.latitude) * sin(coordinate.latitude) - sin(centerCoordinate.latitude) * cos(coordinate.latitude) * cos(coordinate.longitude - centerCoordinate.longitude));
        return {x * scale, y * scale};
    }

    template <typename T>
    static bool equidistantCheck(T latitude, T longitude, T centerLatitude, T centerLongitude) {
        // Degree To radian
        latitude = M_PI * latitude / 180.0f;
        longitude = M_PI * longitude / 180.0f;
        centerLatitude = M_PI * centerLatitude / 180.0f;
        centerLongitude = M_PI * centerLongitude / 180.0f;

        T deltaSigma = std::sin(centerLatitude) * std::sin(latitude) + std::cos(latitude) * std::cos(longitude - centerLongitude);
        if(deltaSigma < 0.0) {
            return false;
        }

        return true;
    }
    template <typename T>
    static bool equidistantCheck(T latitude, T longitude, const CoordGeodetic& centerCoordinate) {
        // Degree To radian
        latitude = M_PI * latitude / 180.0f;
        longitude = M_PI * longitude / 180.0f;

        T deltaSigma = std::sin(centerCoordinate.latitude) * std::sin(latitude) + std::cos(latitude) * std::cos(longitude - centerCoordinate.longitude);
        if(deltaSigma < 0.0) {
            return false;
        }

        return true;
    }

  private:
    void calculateCartesionCoordinates();
    Vector raytraceToEarth(const Vector& position, double roll, double pitch, double yaw) const;
    static double calculateBearingAngle(const CoordGeodetic& start, const CoordGeodetic& end);
    static Matrix4x4 lookAt(const Vector3& position, const Vector3& target, const Vector3& up);

    static inline double degreeToRadian(double degree) {
        return (M_PI * degree / 180.0);
    }

    static inline double radioanToDegree(double radian) {
        return radian * (180.0 / M_PI);
    }
    template <typename T>
    static inline T normalizeLongitude(T angleRadians) {
        return std::remainder(angleRadians, static_cast<T>(2.0 * M_PI));
    }

  private:
    Tle mTle;
    SGP4 mSgp4;
    DateTime mPassStart;
    TimeSpan mPassLength;
    double mScanAngle;
    double mRollOffset;
    double mPitchOffset;
    double mYawOffset;
    int mImageWidth;
    int mImageHeight;
    int mEarthradius;
    int mSatelliteAltitude;
    CoordGeodetic mCenterCoordinate;

    static constexpr double PIXELTIME_MINUTES = 0.02564876089324618736383442265795; // Just a rough calculation for every 10 pixel in minutes
    static constexpr double PIXELTIME_MS = 154.0;
};

#endif // PIXELGEOLOCATIONCALCULATOR_H
