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
    static PixelGeolocationCalculator load(const std::string& path);

  public:
    PixelGeolocationCalculator(const TleReader::TLE& tle, const DateTime& passStart, const TimeSpan& passLength, double scanAngle, double roll, double pitch, double yaw, int earthRadius = 6378, int satelliteAltitude = 825);

    void calcPixelCoordinates();

    void save(const std::string& path);

  public:
    int getGeorefMaxImageHeight() const {
        return (mCoordinates.size() / 158) * 10;
    }

    const CoordGeodetic& getCenterCoordinate() const {
        return mCoordinates[mCoordinates.size() / 2 + 79];
    }

    inline const CoordGeodetic& getCoordinateAt(unsigned int x, unsigned int y) const {
        return mCoordinates[((x / 10)) + ((y / 10) * 158)];
    }

    inline const CoordGeodetic& getCoordinateTopLeft() const {
        return mCoordinates[0];
    }

    inline const CoordGeodetic& getCoordinateTopRight() const {
        return mCoordinates[157];
    }

    inline const CoordGeodetic& getCoordinateBottomLeft() const {
        return mCoordinates[mCoordinates.size() - 158];
    }

    inline const CoordGeodetic& getCoordinateBottomRight() const {
        return mCoordinates[mCoordinates.size() - 1];
    }

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
    static CartesianCoordinate<T> coordinateToMercatorProjection(const CoordGeodetic& coordinate, double radius, float scale) {
        CartesianCoordinate<T> cartesianCoordinate;
        CoordGeodetic correctedCoordinate = coordinate;

        if(coordinate.latitude > degreeToRadian(85.05113)) {
            correctedCoordinate.latitude = degreeToRadian(85.05113);
        } else if(coordinate.latitude < degreeToRadian(-85.05113)) {
            correctedCoordinate.latitude = degreeToRadian(-85.05113);
        }

        cartesianCoordinate.x = radius * (M_PI + correctedCoordinate.longitude);
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
    Vector los_to_earth(const Vector& position, double roll, double pitch, double yaw);
    double calculateBearingAngle(const CoordGeodetic& start, const CoordGeodetic& end);
    Matrix4x4 lookAt(const Vector3& position, const Vector3& target, const Vector3& up);

    static inline double degreeToRadian(double degree) {
        return (M_PI * degree / 180.0);
    }

    static inline double radioanToDegree(double radian) {
        return radian * (180.0 / M_PI);
    }

  private:
    Tle mTle;
    SGP4 mSgp4;
    DateTime mPassStart;
    TimeSpan mPassLength;
    double mScanAngle, mRoll, mPitch, mYaw;
    int mEarthradius;
    int mSatelliteAltitude;
    std::vector<CoordGeodetic> mCoordinates;


    static constexpr double PIXELTIME_MINUTES = 0.02564876089324618736383442265795; // Just a rough calculation for every 10 pixel in minutes
    static constexpr double PIXELTIME_MS = 154.0;
};

#endif // PIXELGEOLOCATIONCALCULATOR_H
