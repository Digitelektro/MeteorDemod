#ifndef PIXELGEOLOCATIONCALCULATOR_H
#define PIXELGEOLOCATIONCALCULATOR_H
#include <string>
#include <math.h>
#include <list>
#include <vector>
#include <SGP4.h>
#include <CoordGeodetic.h>
#include "matrix.h"
#include "vector.h"
#include "tlereader.h"

class PixelGeolocationCalculator
{
public:
    template<typename T>
    struct CartesianCoordinateT {
        T x;
        T y;
    };

    template<typename T>
    friend std::ostream& operator << (std::ostream &o, const CartesianCoordinateT<T> &coord) {
        return o << "x: " << coord.x << "\ty: " << coord.y;
    }

    typedef CartesianCoordinateT<int> CartesianCoordinate;
    typedef CartesianCoordinateT<float> CartesianCoordinateF;
    typedef CartesianCoordinateT<double> CartesianCoordinateD;

private:
    PixelGeolocationCalculator();

public:
    static PixelGeolocationCalculator load(const std::string &path);

public:
    PixelGeolocationCalculator(const TleReader::TLE &tle, const DateTime &passStart, const TimeSpan &passLength, double scanAngle, double roll, double pitch, double yaw, int earthRadius = 6378, int satelliteAltitude = 825);

    void calcPixelCoordinates();

    void save(const std::string &path);

public:

    int getGeorefMaxImageHeight() const {
        return (mEquidistantCartesianCoordinates.size() / 158) * 10;
    }

    const CartesianCoordinateF getTopLeftEquidistant(float scale) const {
        return {mEquidistantCartesianCoordinates[0].x * scale, mEquidistantCartesianCoordinates[0].y * scale};
    }

    const CartesianCoordinateF getTopRightEquidistant(float scale) const {
        return {mEquidistantCartesianCoordinates[157].x * scale, mEquidistantCartesianCoordinates[157].y * scale};
    }

    const CartesianCoordinateF getBottomLeftEquidistant(float scale) const {
        return {mEquidistantCartesianCoordinates[mEquidistantCartesianCoordinates.size() - 158].x * scale, mEquidistantCartesianCoordinates[mEquidistantCartesianCoordinates.size() - 158].y * scale};
    }

    const CartesianCoordinateF getBottomRightEquidistant(float scale) const {
        return {mEquidistantCartesianCoordinates[mEquidistantCartesianCoordinates.size() - 1].x * scale, mEquidistantCartesianCoordinates[mEquidistantCartesianCoordinates.size() - 1].y * scale};
    }

    const CartesianCoordinateF getEquidistantAt(unsigned int x, unsigned int y, float scale) const {
        return {mEquidistantCartesianCoordinates[((x / 10)) + ((y / 10) * 158)].x * scale, mEquidistantCartesianCoordinates[((x / 10)) + ((y / 10) * 158)].y * scale};
    }

    const CartesianCoordinateF getTopLeftMercator(float scale) const {
        return {mMercatorCartesianCoordinates[0].x * scale, mMercatorCartesianCoordinates[0].y * scale};
    }

    const CartesianCoordinateF getTopRightMercator(float scale) const {
        return {mMercatorCartesianCoordinates[157].x * scale, mMercatorCartesianCoordinates[157].y * scale};
    }

    const CartesianCoordinateF getBottomLeftMercator(float scale) const {
        return {mMercatorCartesianCoordinates[mMercatorCartesianCoordinates.size() - 158].x * scale, mMercatorCartesianCoordinates[mMercatorCartesianCoordinates.size() - 158].y * scale};
    }

    const CartesianCoordinateF getBottomRightMercator(float scale) const {
        return {mMercatorCartesianCoordinates[mMercatorCartesianCoordinates.size() - 1].x * scale, mMercatorCartesianCoordinates[mMercatorCartesianCoordinates.size() - 1].y * scale};
    }

    const CoordGeodetic getCenterCoordinate() const {
        return mCenterCoordinate;
    }

    const CartesianCoordinateF getMercatorAt(unsigned int x, unsigned int y, float scale) const {
        return {mMercatorCartesianCoordinates[((x / 10)) + ((y / 10) * 158)].x * scale, mMercatorCartesianCoordinates[((x / 10)) + ((y / 10) * 158)].y * scale};
    }

public:
    static CartesianCoordinateF coordinateToMercatorProjection(double latitude, double longitude, double radius, float scale) {
        return coordinateToMercatorProjection(CoordGeodetic(latitude, longitude, 0), radius, scale);
    }

    static CartesianCoordinateF coordinateToAzimuthalEquidistantProjection(double latitude, double longitude, double centerLatitude, double centerLongitude, double radius, float scale) {
        return coordinateToAzimuthalEquidistantProjection(CoordGeodetic(latitude, longitude, 0), CoordGeodetic(centerLatitude, centerLongitude, 0), radius, scale);
    }

    static CartesianCoordinateF coordinateToMercatorProjection(const CoordGeodetic &coordinate, double radius, float scale) {
        CartesianCoordinateF cartesianCoordinate;
        CoordGeodetic correctedCoordinate = coordinate;

        if (coordinate.latitude > degreeToRadian(85.05113))
        {
            correctedCoordinate.latitude = degreeToRadian(85.05113);
        }
        else if (coordinate.latitude < degreeToRadian(-85.05113))
        {
            correctedCoordinate.latitude = degreeToRadian(-85.05113);
        }

        cartesianCoordinate.x = radius * (M_PI + correctedCoordinate.longitude);
        cartesianCoordinate.y = radius * (M_PI - log(tan(M_PI / 4.0 + (correctedCoordinate.latitude) / 2.0)));
        return {cartesianCoordinate.x * scale, cartesianCoordinate.y * scale};
    }

    static CartesianCoordinateF coordinateToAzimuthalEquidistantProjection(const CoordGeodetic &coordinate, const CoordGeodetic &centerCoordinate, double radius, float scale) {
        CartesianCoordinateF cartesianCoordinate;
        cartesianCoordinate.x = radius * (cos(coordinate.latitude) * sin(coordinate.longitude - centerCoordinate.longitude));
        cartesianCoordinate.y = -radius * (cos(centerCoordinate.latitude) * sin(coordinate.latitude) - sin(centerCoordinate.latitude) * cos(coordinate.latitude) * cos(coordinate.longitude - centerCoordinate.longitude));
        return {cartesianCoordinate.x * scale, cartesianCoordinate.y * scale};
    }

private:
    void calculateCartesionCoordinates();
    Vector locationToVector(const CoordGeodetic &location);
    CoordGeodetic vectorToLocation(const Vector &vector);
    CoordGeodetic los_to_earth(const CoordGeodetic &position, double roll, double pitch, double yaw);
    CoordGeodetic los_to_earth(const Vector &position, double roll, double pitch, double yaw);
    double calculateBearingAngle(const CoordGeodetic &start, const CoordGeodetic &end);
    Matrix4x4 lookAt(const Vector3 &position, const Vector3 &target, const Vector3 &up);

    static inline double degreeToRadian(double degree)
    {
        return (M_PI * degree / 180.0);
    }

    static inline double radioanToDegree(double radian)
    {
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
    std::vector<CartesianCoordinateF> mMercatorCartesianCoordinates;
    std::vector<CartesianCoordinateF> mEquidistantCartesianCoordinates;
    CoordGeodetic mCenterCoordinate;


    static constexpr double PIXELTIME_MINUTES = 0.02564876089324618736383442265795;  //Just a rough calculation for every 10 pixel in minutes
    static constexpr double PIXELTIME_MS = 154.0;
};

#endif // PIXELGEOLOCATIONCALCULATOR_H
