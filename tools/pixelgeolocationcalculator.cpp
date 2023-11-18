#include "pixelgeolocationcalculator.h"

#include "settings.h"


PixelGeolocationCalculator::PixelGeolocationCalculator(const TleReader::TLE& tle,
                                                       const DateTime& passStart,
                                                       const TimeSpan& passLength,
                                                       double scanAngle,
                                                       double roll,
                                                       double pitch,
                                                       double yaw,
                                                       int imageWidth,
                                                       int imageHeight,
                                                       int earthRadius,
                                                       int satelliteAltitude)
    : mTle(tle.satellite, tle.line1, tle.line2)
    , mSgp4(mTle)
    , mPassStart(passStart)
    , mPassLength(passLength)
    , mScanAngle(scanAngle)
    , mRollOffset(roll)
    , mPitchOffset(pitch)
    , mYawOffset(yaw)
    , mImageWidth(imageWidth)
    , mImageHeight(imageHeight)
    , mEarthradius(earthRadius)
    , mSatelliteAltitude(satelliteAltitude) {
    mCenterCoordinate = getCoordinateAt(mImageWidth / 2, mImageHeight / 2);
}


CoordGeodetic PixelGeolocationCalculator::getCoordinateAt(unsigned int x, unsigned int y) const {
    if(x > mImageWidth) {
        x = mImageWidth;
    }
    if(y > mImageHeight) {
        y = mImageHeight;
    }

    DateTime currentTime = mPassStart;
    currentTime = currentTime.AddMicroseconds(PIXELTIME_MS * 1000 * y);
    Eci position = mSgp4.FindPosition(currentTime.AddMicroseconds(PIXELTIME_MS * 1000 * 10));
    Eci prevPosition = mSgp4.FindPosition(currentTime);
    CoordGeodetic satOnGround = Eci(currentTime, position.Position(), position.Velocity()).ToGeodetic();
    CoordGeodetic satOnGroundPrev = Eci(currentTime, prevPosition.Position(), prevPosition.Velocity()).ToGeodetic();

    double angle = calculateBearingAngle(satOnGround, satOnGroundPrev);
    angle = degreeToRadian(90) - angle;

    double currentScanAngle = (mScanAngle / mImageWidth) * ((mImageWidth / 2.0) - x);
    // std::cout << "CurrentScan Angle2 " << currentScanAngle << std::endl;

    Vector3 posVector(raytraceToEarth(Vector3(satOnGround), degreeToRadian(currentScanAngle), 0, angle));
    return posVector.toCoordinate();
}

const CoordGeodetic& PixelGeolocationCalculator::getCenterCoordinate() const {
    return mCenterCoordinate;
}

CoordGeodetic PixelGeolocationCalculator::getCoordinateTopLeft() const {
    return getCoordinateAt(0, 0);
}

CoordGeodetic PixelGeolocationCalculator::getCoordinateTopRight() const {
    return getCoordinateAt(mImageWidth, 0);
}

CoordGeodetic PixelGeolocationCalculator::getCoordinateBottomLeft() const {
    return getCoordinateAt(0, mImageHeight);
}

CoordGeodetic PixelGeolocationCalculator::getCoordinateBottomRight() const {
    return getCoordinateAt(mImageWidth, mImageHeight);
}

bool PixelGeolocationCalculator::isNorthBoundPass() const {
    auto coord1 = getCoordinateAt(0, 0);
    auto coord2 = getCoordinateAt(0, 100);
    return (coord1.latitude < coord2.latitude);
}

Vector PixelGeolocationCalculator::raytraceToEarth(const Vector& position, double roll, double pitch, double yaw) const {
    double a = 6371.0087714;
    double b = 6371.0087714;
    double c = 6356.752314245;

    double x = position.x;
    double y = position.y;
    double z = position.z;

    Matrix4x4 matrix(1, 0, 0, position.x, 0, 1, 0, position.y, 0, 0, 1, position.z, 0, 0, 0, 1);

    Vector lookVector(0, 0, 0);
    Matrix4x4 lookMatrix = lookAt(position, lookVector, Vector(0, 0, 1));
    Matrix4x4 rotateX = Matrix4x4::CreateRotationX(roll + degreeToRadian(mRollOffset));
    Matrix4x4 rotateY = Matrix4x4::CreateRotationY(pitch + degreeToRadian(mPitchOffset));
    Matrix4x4 rotateZ = Matrix4x4::CreateRotationZ(yaw + degreeToRadian(mYawOffset));
    matrix = matrix * lookMatrix * rotateZ * rotateY * rotateX;

    Vector vector3(matrix.mElements[2], matrix.mElements[6], matrix.mElements[10]);

    double u = vector3.x;
    double v = vector3.y;
    double w = vector3.z;

    double value = -pow(a, 2) * pow(b, 2) * w * z - pow(a, 2) * pow(c, 2) * v * y - pow(b, 2) * pow(c, 2) * u * x;
    double radical = pow(a, 2) * pow(b, 2) * pow(w, 2) + pow(a, 2) * pow(c, 2) * pow(v, 2) - pow(a, 2) * pow(v, 2) * pow(z, 2) + 2 * pow(a, 2) * v * w * y * z - pow(a, 2) * pow(w, 2) * pow(y, 2) + pow(b, 2) * pow(c, 2) * pow(u, 2)
                     - pow(b, 2) * pow(u, 2) * pow(z, 2) + 2 * pow(b, 2) * u * w * x * z - pow(b, 2) * pow(w, 2) * pow(x, 2) - pow(c, 2) * pow(u, 2) * pow(y, 2) + 2 * pow(c, 2) * u * v * x * y - pow(c, 2) * pow(v, 2) * pow(x, 2);
    double magnitude = pow(a, 2) * pow(b, 2) * pow(w, 2) + pow(a, 2) * pow(c, 2) * pow(v, 2) + pow(b, 2) * pow(c, 2) * pow(u, 2);

    /*double t = -(1 / (pow(c, 2) * (pow(u, 2) + pow(v, 2)) + pow(a, 2) * pow(w, 2))) *
       (pow(c, 2) * (u * x + v * y) + pow(a, 2) * w * z +
       0.5 * sqrt(4 * pow((pow(a, 2) * (u * x + v * y) + pow(a, 2) * w * z), 2) - 4 * (pow(a, 2) * (pow(u, 2) + pow(v, 2)) + pow(a, 2) * pow(w, 2)) * (pow(c, 2) * (-pow(a, 2) + pow(x, 2) + pow(y, 2)) + pow(a, 2) * pow(z, 2))));
    */

    if(radical < 0) {
        return {0, 0, 0};
    }

    double d = (value - a * b * c * sqrt(radical)) / magnitude;

    if(d < 0) {
        return {0, 0, 0};
    }

    x += d * u;
    y += d * v;
    z += d * w;

    return {x, y, z};
}

// Todo: More precise calculation maybe required, example: https://github.com/airbreather/Gavaghan.Geodesy/blob/master/Source/Gavaghan.Geodesy/GeodeticCalculator.cs
double PixelGeolocationCalculator::calculateBearingAngle(const CoordGeodetic& start, const CoordGeodetic& end) {
    double alpha = end.longitude - start.longitude;
    double y = sin(alpha) * cos(end.latitude);
    double x = cos(start.latitude) * sin(end.latitude) - sin(start.latitude) * cos(end.latitude) * cos(alpha);
    double theta = atan2(y, x);

    return theta;
}

Matrix4x4 PixelGeolocationCalculator::lookAt(const Vector3& position, const Vector3& target, const Vector3& up) {
    Vector3 k = Vector3(target) - position;
    double m = k.DistanceSquared();
    if(m < std::numeric_limits<double>::epsilon()) {
        return Matrix4x4();
    }
    k = k * (1.0 / sqrt(m));

    Vector3 i = up.Cross(k);
    i.Normalize();

    Vector3 j = k.Cross(i);
    j.Normalize();

    return Matrix4x4(i.x, j.x, k.x, 0.0, i.y, j.y, k.y, 0.0, i.z, j.z, k.z, 0.0, 0.0, 0.0, 0.0, 1.0);
}
