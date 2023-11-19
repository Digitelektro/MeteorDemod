#include "projectimage.h"

#include <cmath>
#include <memory>
#include <numeric>
#include <opencv2/imgcodecs.hpp>

#include "GIS/shaperenderer.h"
#include "settings.h"
#include "threadpool.h"

std::map<std::string, cv::MarkerTypes> ProjectImage::MarkerLookup{{"STAR", cv::MARKER_STAR},
                                                                  {"CROSS", cv::MARKER_CROSS},
                                                                  {"SQUARE", cv::MARKER_SQUARE},
                                                                  {"DIAMOND", cv::MARKER_DIAMOND},
                                                                  {"TRIANGLE_UP", cv::MARKER_TRIANGLE_UP},
                                                                  {"TRIANGLE_DOWN", cv::MARKER_TRIANGLE_DOWN},
                                                                  {"TILTED_CROSS", cv::MARKER_TILTED_CROSS}};

std::list<ProjectImage> ProjectImage::createCompositeProjector(Projection projection, const std::list<PixelGeolocationCalculator>& gcpCalclulators, float scale, int earthRadius, int altitude) {
    float MinX = std::numeric_limits<float>::max();
    float MinY = std::numeric_limits<float>::max();
    float MaxX = std::numeric_limits<float>::min();
    float MaxY = std::numeric_limits<float>::min();
    float corner;

    Vector3 centerVector;
    for(const auto& c : gcpCalclulators) {
        centerVector += Vector3(c.getCenterCoordinate());
    }
    CoordGeodetic center = centerVector.toCoordinate();

    if(projection == Projection::Equidistant) {
        for(const auto& gcpCalc : gcpCalclulators) {
            auto topLeft = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(gcpCalc.getCoordinateTopLeft(), center, gcpCalc.getSatelliteHeight(), scale);
            auto topRight = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(gcpCalc.getCoordinateTopRight(), center, gcpCalc.getSatelliteHeight(), scale);
            auto bottomLeft = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(gcpCalc.getCoordinateBottomLeft(), center, gcpCalc.getSatelliteHeight(), scale);
            auto bottomRight = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(gcpCalc.getCoordinateBottomRight(), center, gcpCalc.getSatelliteHeight(), scale);

            corner = std::min({topLeft.x, topRight.x, bottomLeft.x, bottomRight.x});
            MinX = corner < MinX ? corner : MinX;

            corner = std::min({topLeft.y, topRight.y, bottomLeft.y, bottomRight.y});
            MinY = corner < MinY ? corner : MinY;

            corner = std::max({topLeft.x, topRight.x, bottomLeft.x, bottomRight.x});
            MaxX = corner > MaxX ? corner : MaxX;

            corner = std::max({topLeft.y, topRight.y, bottomLeft.y, bottomRight.y});
            MaxY = corner > MaxY ? corner : MaxY;
        }
    } else if(projection == Projection::Mercator) {
        for(const auto& gcpCalc : gcpCalclulators) {
            PixelGeolocationCalculator::CartesianCoordinateF topLeft = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(gcpCalc.getCoordinateTopLeft(), gcpCalc.getSatelliteHeight(), scale);
            PixelGeolocationCalculator::CartesianCoordinateF topRight = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(gcpCalc.getCoordinateTopRight(), gcpCalc.getSatelliteHeight(), scale);
            PixelGeolocationCalculator::CartesianCoordinateF bottomLeft = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(gcpCalc.getCoordinateBottomLeft(), gcpCalc.getSatelliteHeight(), scale);
            PixelGeolocationCalculator::CartesianCoordinateF bottomRight = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(gcpCalc.getCoordinateBottomRight(), gcpCalc.getSatelliteHeight(), scale);

            corner = std::min({topLeft.x, topRight.x, bottomLeft.x, bottomRight.x});
            MinX = corner < MinX ? corner : MinX;

            corner = std::min({topLeft.y, topRight.y, bottomLeft.y, bottomRight.y});
            MinY = corner < MinY ? corner : MinY;

            corner = std::max({topLeft.x, topRight.x, bottomLeft.x, bottomRight.x});
            MaxX = corner > MaxX ? corner : MaxX;

            corner = std::max({topLeft.y, topRight.y, bottomLeft.y, bottomRight.y});
            MaxY = corner > MaxY ? corner : MaxY;
        }
    }

    int width = static_cast<int>(std::abs(MaxX - MinX));
    int height = static_cast<int>(std::abs(MaxY - MinY));
    float xStart = std::min(MinX, MaxX);
    float yStart = std::min(MinY, MaxY);

    std::list<ProjectImage> projectors;
    for(const auto& gcpCalc : gcpCalclulators) {
        ProjectImage projector(projection, gcpCalc, scale, earthRadius, altitude);
        projector.mWidth = width;
        projector.mHeight = height;
        projector.mXStart = xStart;
        projector.mYStart = yStart;
        projector.mCenterCoordinate = center;
        projector.mBoundariesCalcNeeded = false;
        projectors.emplace_back(projector);
    }
    return projectors;
}

ProjectImage::ProjectImage(Projection projection, const PixelGeolocationCalculator& geolocationCalculator, float scale, int earthRadius, int altitude)
    : mProjection(projection)
    , mGeolocationCalculator(geolocationCalculator)
    , mScale(scale)
    , mEarthRadius(earthRadius)
    , mAltitude(altitude)
    , mTransformer(cv::createThinPlateSplineShapeTransformer2(Settings::getInstance().getResourcesPath() + "kernels/tps.cl")) {

    mTheta = 0.5 * SWATH / earthRadius;                                        // Maximum half-Angle subtended by Swath from Earth's centre
    mHalfChord = static_cast<int>(earthRadius * std::sin(mTheta));             // Maximum Length of chord subtended at Centre of Earth
    int h = static_cast<int>(earthRadius * std::cos(mTheta));                  // Minimum Altitude from Centre of Earth to chord from std::costd::sine
    mInc = earthRadius - h;                                                    // Maximum Distance from Arc to Chord
    mScanAngle = std::atan(mHalfChord / static_cast<double>(altitude + mInc)); // Maximum Angle subtended by half-chord from satellite
}

void ProjectImage::calculateTransformation(const cv::Size& imageSize) {
    if(mBoundariesCalcNeeded) {
        calculateImageBoundaries();
    }

    std::vector<cv::Point2f> sourcePoints, targetPoints;
    for(int y = 0; y < imageSize.height; y += 100) {
        for(int x = 0; x < imageSize.width; x += 100) {
            if(mProjection == Projection::Equidistant || mProjection == Projection::Rectify) {
                const auto p1 = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(mGeolocationCalculator.getCoordinateAt(x, y), mCenterCoordinate, mGeolocationCalculator.getSatelliteHeight(), mScale);
                sourcePoints.push_back(cv::Point2f(x, y));
                targetPoints.push_back(cv::Point2f(p1.x + (-mXStart), p1.y + (-mYStart)));
            } else {
                // Mercator
                const auto p1 = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(mGeolocationCalculator.getCoordinateAt(x, y), mGeolocationCalculator.getSatelliteHeight(), mScale);
                sourcePoints.push_back(cv::Point2f(x, y));
                targetPoints.push_back(cv::Point2f(p1.x + (-mXStart), p1.y + (-mYStart)));
            }
        }

        int x = imageSize.width;
        if(mProjection == Projection::Equidistant || mProjection == Projection::Rectify) {
            const auto p1 = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(mGeolocationCalculator.getCoordinateAt(x, y), mCenterCoordinate, mGeolocationCalculator.getSatelliteHeight(), mScale);
            sourcePoints.push_back(cv::Point2f(x, y));
            targetPoints.push_back(cv::Point2f(p1.x + (-mXStart), p1.y + (-mYStart)));
        } else {
            // Mercator
            const auto p1 = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(mGeolocationCalculator.getCoordinateAt(x, y), mGeolocationCalculator.getSatelliteHeight(), mScale);
            sourcePoints.push_back(cv::Point2f(x, y));
            targetPoints.push_back(cv::Point2f(p1.x + (-mXStart), p1.y + (-mYStart)));
        }
    }

    std::vector<cv::DMatch> matches;
    for(std::size_t i = 0; i < sourcePoints.size(); i++) {
        matches.push_back(cv::DMatch(i, i, 0));
    }
    mTransformer->estimateTransformation(targetPoints, sourcePoints, matches);

    if(mProjection == Projection::Rectify) {
        rectify(imageSize);
    } else {
        mMapX = cv::Mat(mHeight, mWidth, CV_32FC1);
        mMapY = cv::Mat(mHeight, mWidth, CV_32FC1);

        std::vector<cv::Point2f> dst;
        std::vector<cv::Point2f> src(mHeight * mWidth);

        for(int row = 0; row < mHeight; row++) {
            for(int col = 0; col < mWidth; col++) {
                src[col + (row * mWidth)] = cv::Point2f(col, row);
            }
        }
        mTransformer->applyTransformation(src, dst);
        for(int row = 0; row < mHeight; row++) {
            for(int col = 0; col < mWidth; col++) {
                mMapX.at<float>(row, col) = dst[col + (row * mWidth)].x;
                mMapY.at<float>(row, col) = dst[col + (row * mWidth)].y;
            }
        }
    }
}

cv::Mat ProjectImage::project(const cv::Mat& image) {
    cv::Mat newImage;
    cv::remap(image, newImage, mMapX, mMapY, cv::INTER_LINEAR);

    drawMapOverlay(newImage);

    return newImage;
}

void ProjectImage::calculateImageBoundaries() {
    float minX;
    float minY;
    float maxX;
    float maxY;

    mCenterCoordinate = mGeolocationCalculator.getCenterCoordinate();

    if(mProjection == Projection::Equidistant || mProjection == Projection::Rectify) {
        auto topLeft = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(mGeolocationCalculator.getCoordinateTopLeft(), mCenterCoordinate, mGeolocationCalculator.getSatelliteHeight(), mScale);
        auto topRight = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(mGeolocationCalculator.getCoordinateTopRight(), mCenterCoordinate, mGeolocationCalculator.getSatelliteHeight(), mScale);
        auto bottomLeft = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(mGeolocationCalculator.getCoordinateBottomLeft(), mCenterCoordinate, mGeolocationCalculator.getSatelliteHeight(), mScale);
        auto bottomRight = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(mGeolocationCalculator.getCoordinateBottomRight(), mCenterCoordinate, mGeolocationCalculator.getSatelliteHeight(), mScale);

        minX = std::min({topLeft.x, topRight.x, bottomLeft.x, bottomRight.x});
        minY = std::min({topLeft.y, topRight.y, bottomLeft.y, bottomRight.y});
        maxX = std::max({topLeft.x, topRight.x, bottomLeft.x, bottomRight.x});
        maxY = std::max({topLeft.y, topRight.y, bottomLeft.y, bottomRight.y});
    } else if(mProjection == Projection::Mercator) {
        auto topLeft = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(mGeolocationCalculator.getCoordinateTopLeft(), mGeolocationCalculator.getSatelliteHeight(), mScale);
        auto topRight = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(mGeolocationCalculator.getCoordinateTopRight(), mGeolocationCalculator.getSatelliteHeight(), mScale);
        auto bottomLeft = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(mGeolocationCalculator.getCoordinateBottomLeft(), mGeolocationCalculator.getSatelliteHeight(), mScale);
        auto bottomRight = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(mGeolocationCalculator.getCoordinateBottomRight(), mGeolocationCalculator.getSatelliteHeight(), mScale);

        minX = std::min({topLeft.x, topRight.x, bottomLeft.x, bottomRight.x});
        minY = std::min({topLeft.y, topRight.y, bottomLeft.y, bottomRight.y});
        maxX = std::max({topLeft.x, topRight.x, bottomLeft.x, bottomRight.x});
        maxY = std::max({topLeft.y, topRight.y, bottomLeft.y, bottomRight.y});
    } else {
    }

    mWidth = static_cast<int>(std::abs(maxX - minX));
    mHeight = static_cast<int>(std::abs(maxY - minY));
    mXStart = static_cast<int>(std::min(minX, maxX));
    mYStart = static_cast<int>(std::min(minY, maxY));
}

void ProjectImage::rectify(const cv::Size& imageSize) {
    int i, k, ni;
    int DupPix;
    float phi;

    mWidth = static_cast<int>(imageSize.width * (0.902 + std::sin(mScanAngle)));
    mHeight = imageSize.height;
    mOriginalImageHalfWidth = (imageSize.width + 1) / 2; // Half-width of original image ('A')
    mNewImageHalfWidth = (mWidth + 1) / 2;               // Half-Width of stretched image ('Z')

    // Create lookup table of std::sines
    std::unique_ptr<int[]> lut = std::make_unique<int[]>(mNewImageHalfWidth);

    for(i = 0; i < mNewImageHalfWidth - 1; i++) {
        phi = std::atan((i / static_cast<double>(mNewImageHalfWidth)) * mHalfChord / (mAltitude + mInc));
        lut[i] = static_cast<int>(mOriginalImageHalfWidth * (std::sin(phi) / std::sin(mScanAngle)));
    }

    mMapX = cv::Mat(mHeight, mWidth, CV_32FC1);
    mMapY = cv::Mat(mHeight, mWidth, CV_32FC1);

    mFlip = mGeolocationCalculator.isNorthBoundPass();

    i = 0;
    ni = 0;
    while(i < mNewImageHalfWidth) {
        k = lut[i];
        ni = i;
        while(true) {
            if(lut[ni] == 0 || lut[ni] > k)
                break;
            ni += 1;
        }

        DupPix = ni - i; // DupPix = number of repeated (spread) pixels

        if(lut[ni] == 0) {
            DupPix = 1;
        }

        for(int y = 0; y < mHeight; y++) {
            for(int x = 0; x < DupPix; x++) {
                mMapX.at<float>(y, mFlip ? mWidth - (mNewImageHalfWidth + i - 1 + x) : mNewImageHalfWidth + i - 1 + x) = mOriginalImageHalfWidth + k - 1;
                mMapX.at<float>(y, mFlip ? mWidth - (mNewImageHalfWidth - i - x) : mNewImageHalfWidth - i - x) = mOriginalImageHalfWidth - k;
                mMapY.at<float>(y, mNewImageHalfWidth + i - 1 + x) = mFlip ? (mHeight - y) : y;
                mMapY.at<float>(y, mNewImageHalfWidth - i - x) = mFlip ? (mHeight - y) : y;
            }
        }
        i += DupPix;
    }
}

void ProjectImage::drawMapOverlay(cv::Mat& image) {
    Settings& settings = Settings::getInstance();

    GIS::ShapeRenderer graticules(settings.getResourcesPath() + settings.getShapeGraticulesFile(), cv::Scalar(settings.getShapeGraticulesColor().B, settings.getShapeGraticulesColor().G, settings.getShapeGraticulesColor().R));
    graticules.setThickness(settings.getShapeGraticulesThickness());
    graticules.drawShape(image, std::bind(&ProjectImage::transform, this, std::placeholders::_1, std::placeholders::_2));

    GIS::ShapeRenderer countryBorders(settings.getResourcesPath() + settings.getShapeBoundaryLinesFile(),
                                      cv::Scalar(settings.getShapeBoundaryLinesColor().B, settings.getShapeBoundaryLinesColor().G, settings.getShapeBoundaryLinesColor().R));
    countryBorders.setThickness(settings.getShapeBoundaryLinesThickness());
    countryBorders.drawShape(image, std::bind(&ProjectImage::transform, this, std::placeholders::_1, std::placeholders::_2));

    GIS::ShapeRenderer coastLines(settings.getResourcesPath() + settings.getShapeCoastLinesFile(), cv::Scalar(settings.getShapeCoastLinesColor().B, settings.getShapeCoastLinesColor().G, settings.getShapeCoastLinesColor().R));
    coastLines.setThickness(settings.getShapeCoastLinesThickness());
    coastLines.drawShape(image, std::bind(&ProjectImage::transform, this, std::placeholders::_1, std::placeholders::_2));

    GIS::ShapeRenderer cities(settings.getResourcesPath() + settings.getShapePopulatedPlacesFile(),
                              cv::Scalar(settings.getShapePopulatedPlacesColor().B, settings.getShapePopulatedPlacesColor().G, settings.getShapePopulatedPlacesColor().R));
    cities.setFontHeight(settings.getShapePopulatedPlacesFontSize() * mScale);
    cities.setFontLineWidth(settings.getShapePopulatedPlacesFontWidth());
    cities.setPointRadius(settings.getShapePopulatedPlacesPointradius() * mScale);
    cities.addNumericFilter(settings.getShapePopulatedPlacesFilterColumnName(), settings.getShapePopulatedPlacesNumbericFilter());
    cities.setTextFieldName(settings.getShapePopulatedPlacesTextColumnName());
    cities.drawShape(image, std::bind(&ProjectImage::transform, this, std::placeholders::_1, std::placeholders::_2));

    if(settings.drawReceiver()) {
        double x = settings.getReceiverLatitude();
        double y = settings.getReceiverLongitude();
        bool draw = transform(x, y);

        if(draw) {
            cv::drawMarker(image, cv::Point2d(x, y), cv::Scalar(0, 0, 0), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness() + 1);
            cv::drawMarker(image,
                           cv::Point2d(x, y),
                           cv::Scalar(settings.getReceiverColor().B, settings.getReceiverColor().G, settings.getReceiverColor().R),
                           stringToMarkerType(settings.getReceiverMarkType()),
                           settings.getReceiverSize(),
                           settings.getReceiverThickness());
        }
    }
}

cv::MarkerTypes ProjectImage::stringToMarkerType(const std::string& markerType) {
    auto itr = MarkerLookup.find(markerType);
    if(itr != MarkerLookup.end()) {
        return itr->second;
    }
    return cv::MARKER_TRIANGLE_UP;
}

bool ProjectImage::transform(double& x, double& y) {
    bool valid = true;
    float centerLatitude = static_cast<float>(mGeolocationCalculator.getCenterCoordinate().latitude * (180.0 / M_PI));
    float centerLongitude = static_cast<float>(mGeolocationCalculator.getCenterCoordinate().longitude * (180.0 / M_PI));

    if(mProjection == Projection::Equidistant) {
        auto location = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>({x, y, 0}, mCenterCoordinate, mGeolocationCalculator.getSatelliteHeight(), mScale);
        valid = equidistantCheck(x, y, centerLatitude, centerLongitude);
        x = location.x + (-mXStart);
        y = location.y + (-mYStart);
    } else if(mProjection == Projection::Mercator) {
        auto location = PixelGeolocationCalculator::coordinateToMercatorProjection<float>({x, y, 0}, mGeolocationCalculator.getSatelliteHeight(), mScale);
        x = location.x + (-mXStart);
        y = location.y + (-mYStart);
    } else if(mProjection == Projection::Rectify) {
        auto location = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>({x, y, 0}, mCenterCoordinate, mGeolocationCalculator.getSatelliteHeight(), mScale);
        valid = equidistantCheck(x, y, centerLatitude, centerLongitude);
        x = location.x + (-mXStart);
        y = location.y + (-mYStart);

        std::vector<cv::Point2f> dst;
        std::vector<cv::Point2f> src;
        src.emplace_back(cv::Point2f(x, y));
        mTransformer->applyTransformation(src, dst);

        if(dst[0].x < 0 || dst[0].x > mOriginalImageHalfWidth * 2) {
            valid = false;
        }

        if(dst[0].x >= mOriginalImageHalfWidth) {
            int k = std::floor(dst[0].x) - mOriginalImageHalfWidth;
            double phi = std::asin(static_cast<double>(k) / mOriginalImageHalfWidth * std::sin(mScanAngle));
            double original = std::tan(phi) * (mAltitude + mInc) / mHalfChord * mNewImageHalfWidth;
            x = mFlip ? mWidth - (mNewImageHalfWidth + original) : mNewImageHalfWidth + original;
        } else {
            int k = mOriginalImageHalfWidth - std::floor(dst[0].x);
            double phi = std::asin(static_cast<double>(k) / mOriginalImageHalfWidth * std::sin(mScanAngle));
            double original = std::tan(phi) * (mAltitude + mInc) / mHalfChord * mNewImageHalfWidth;
            x = mFlip ? mWidth - (mNewImageHalfWidth - original) : mNewImageHalfWidth - original;
        }

        y = mFlip ? mHeight - dst[0].y : dst[0].y;
    }
    return valid;
}

bool ProjectImage::equidistantCheck(float latitude, float longitude, float centerLatitude, float centerLongitude) {
    // Degree To radian
    latitude = M_PI * latitude / 180.0f;
    longitude = M_PI * longitude / 180.0f;
    centerLatitude = M_PI * centerLatitude / 180.0f;
    centerLongitude = M_PI * centerLongitude / 180.0f;

    float deltaSigma = std::sin(centerLatitude) * std::sin(latitude) + std::cos(latitude) * std::cos(longitude - centerLongitude);
    if(deltaSigma < 0.0) {
        return false;
    }

    return true;
}
