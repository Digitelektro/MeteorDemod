#include "spreadimage.h"
#include "GIS/shaperenderer.h"
#include "settings.h"
#include <opencv2/imgcodecs.hpp>
#include <cmath>
#include <numeric>
//#include <opencv2/shape/shape_transformer.hpp>

std::map<std::string, cv::MarkerTypes> SpreadImage::MarkerLookup {
    { "STAR", cv::MARKER_STAR },
    { "CROSS", cv::MARKER_CROSS },
    { "SQUARE", cv::MARKER_SQUARE },
    { "DIAMOND", cv::MARKER_DIAMOND },
    { "TRIANGLE_UP", cv::MARKER_TRIANGLE_UP },
    { "TRIANGLE_DOWN", cv::MARKER_TRIANGLE_DOWN },
    { "TILTED_CROSS", cv::MARKER_TILTED_CROSS }
};

SpreadImage::SpreadImage(int earthRadius, int altitude)
{
    mEarthRadius = earthRadius;
    mAltitude = altitude;

    mTheta = 0.5 * SWATH / earthRadius;                                          // Maximum half-Angle subtended by Swath from Earth's centre
    mHalfChord = static_cast<int>(earthRadius * std::sin(mTheta));               // Maximum Length of chord subtended at Centre of Earth
    int h = static_cast<int>(earthRadius * std::cos(mTheta));                    // Minimum Altitude from Centre of Earth to chord from std::costd::sine
    mInc = earthRadius - h;                                                      // Maximum Distance from Arc to Chord
    mScanAngle = std::atan(mHalfChord / static_cast<double>(altitude + mInc));   // Maximum Angle subtended by half-chord from satellite
}

cv::Mat SpreadImage::stretch(const cv::Mat &image)
{
    int i, k, ni, imageHeight, imageWidth, newImageWidth;
    int imageHalfWidth, newImageHalfWidth;              // centrepoints of scanlines
    int DupPix;

    imageHeight = image.size().height;
    imageWidth = image.size().width;
    imageHalfWidth = (imageWidth + 1) / 2;              // Half-width of original image ('A')

    cv::Mat strechedImage(imageHeight, static_cast<int>(imageWidth * (0.902 + std::sin(mScanAngle))), image.type());
    newImageWidth = strechedImage.size().width;
    newImageHalfWidth = (newImageWidth + 1) / 2;       // Half-Width of stretched image ('Z')

    cv::Mat mapX(strechedImage.size(), CV_32FC1);
    cv::Mat mapY(strechedImage.size(), CV_32FC1);

    // Create lookup table of std::sines
    memset(mLookUp, 0, sizeof(mLookUp));
    for (i = 0; i < newImageHalfWidth -1; i++)
    {
        mPhi = std::atan((i / static_cast<double>(newImageHalfWidth)) * mHalfChord / (mAltitude + mInc));
        mLookUp[i] = static_cast<int>(imageHalfWidth * (std::sin(mPhi) / std::sin(mScanAngle)));
    }

    i = 0;
    ni = 0;
    while (i < newImageHalfWidth)
    {
        mPhi = std::atan((i / static_cast<double>(newImageHalfWidth)) * mHalfChord / (mAltitude + mInc));
        k = mLookUp[i];
        ni = i;
        while(true)
        {
            if(mLookUp[ni] == 0 || mLookUp[ni] > k)
                break;
            ni += 1;
        }

        DupPix = ni - i;    // DupPix = number of repeated (spread) pixels

        if (mLookUp[ni] == 0)
        {
            DupPix = 1;
        }

        for(int y = 0; y < image.rows; y++) {
            for(int x = 0; x < DupPix; x++) {
                mapX.at<float>(y, newImageHalfWidth + i - 1 + x) = imageHalfWidth + k - 1;
                mapX.at<float>(y, newImageHalfWidth - i - x) = imageHalfWidth - k;
                mapY.at<float>(y, newImageHalfWidth + i - 1 + x) = y;
                mapY.at<float>(y, newImageHalfWidth - i - x) = y;
            }
        }

        i += DupPix;
    }

    cv::remap(image, strechedImage, mapX, mapY, cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

    return strechedImage;
}

cv::Mat SpreadImage::mercatorProjection(const cv::Mat &image, const PixelGeolocationCalculator &geolocationCalculator, float scale, ProgressCallback progressCallback)
{
    cv::Point2f srcTri[3];
    cv::Point2f dstTri[3];

    PixelGeolocationCalculator::CartesianCoordinateF topLeft = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator.getCoordinateTopLeft(), geolocationCalculator.getSatelliteHeight(), scale);
    PixelGeolocationCalculator::CartesianCoordinateF topRight = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator.getCoordinateTopRight(), geolocationCalculator.getSatelliteHeight(), scale);
    PixelGeolocationCalculator::CartesianCoordinateF bottomLeft = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator.getCoordinateBottomLeft(), geolocationCalculator.getSatelliteHeight(), scale);
    PixelGeolocationCalculator::CartesianCoordinateF bottomRight = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator.getCoordinateBottomRight(), geolocationCalculator.getSatelliteHeight(), scale);

    double MinX = std::min(topLeft.x, std::min(topRight.x, std::min(bottomLeft.x, bottomRight.x)));
    double MinY = std::min(topLeft.y, std::min(topRight.y, std::min(bottomLeft.y, bottomRight.y)));
    double MaxX = std::max(topLeft.x, std::max(topRight.x, std::max(bottomLeft.x, bottomRight.x)));
    double MaxY = std::max(topLeft.y, std::max(topRight.y, std::max(bottomLeft.y, bottomRight.y)));

    int width = static_cast<int>(std::abs(MaxX - MinX));
    int height = static_cast<int>(std::abs(MaxY - MinY));

    int xStart = static_cast<int>(std::min(MinX, MaxX));
    int yStart = static_cast<int>(std::min(MinY, MaxY));

    int imageWithGeorefHeight = geolocationCalculator.getGeorefMaxImageHeight() < image.size().height ? geolocationCalculator.getGeorefMaxImageHeight() : image.size().height;

    //cv::Mat newImage(height, width, image.type());
    cv::Mat newImage = cv::Mat::zeros(height, width, image.type());

    for (int y = 0; y < imageWithGeorefHeight - 10; y += 10)
    {
        if(progressCallback) {
            progressCallback(static_cast<float>(y) / imageWithGeorefHeight * 100.0f);
        }
        for (int x = 0; x < image.size().width - 10; x += 10)
        {
            const PixelGeolocationCalculator::CartesianCoordinateF p1 = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator.getCoordinateAt(x, y), geolocationCalculator.getSatelliteHeight(), scale);
            const PixelGeolocationCalculator::CartesianCoordinateF p2 = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator.getCoordinateAt(x + 10, y), geolocationCalculator.getSatelliteHeight(), scale);
            const PixelGeolocationCalculator::CartesianCoordinateF p3 = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator.getCoordinateAt(x, y + 10), geolocationCalculator.getSatelliteHeight(), scale);

            srcTri[0] = cv::Point2f( x, y );
            srcTri[1] = cv::Point2f( x + 10, y );
            srcTri[2] = cv::Point2f( x, y + 10 );

            dstTri[0] = cv::Point2f(p1.x, p1.y);
            dstTri[1] = cv::Point2f(p2.x, p2.y);
            dstTri[2] = cv::Point2f(p3.x, p3.y);
            affineTransform(image, newImage, srcTri, dstTri, -xStart, -yStart);
        }
    }

    Settings &settings = Settings::getInstance();
    GIS::ShapeRenderer graticules(settings.getResourcesPath() + settings.getShapeGraticulesFile(), cv::Scalar(settings.getShapeGraticulesColor().B, settings.getShapeGraticulesColor().G, settings.getShapeGraticulesColor().R));
    graticules.setThickness(settings.getShapeGraticulesThickness());
    graticules.drawShapeMercator(newImage, xStart, yStart, scale);

    GIS::ShapeRenderer coastLines(settings.getResourcesPath() + settings.getShapeCoastLinesFile(), cv::Scalar(settings.getShapeCoastLinesColor().B, settings.getShapeCoastLinesColor().G, settings.getShapeCoastLinesColor().R));
    coastLines.setThickness(settings.getShapeCoastLinesThickness());
    coastLines.drawShapeMercator(newImage, xStart, yStart, scale);

    GIS::ShapeRenderer countryBorders(settings.getResourcesPath() + settings.getShapeBoundaryLinesFile(), cv::Scalar(settings.getShapeBoundaryLinesColor().B, settings.getShapeBoundaryLinesColor().G, settings.getShapeBoundaryLinesColor().R));
    countryBorders.setThickness(settings.getShapeBoundaryLinesThickness());
    countryBorders.drawShapeMercator(newImage, xStart, yStart, scale);

    GIS::ShapeRenderer cities(settings.getResourcesPath() + settings.getShapePopulatedPlacesFile(), cv::Scalar(settings.getShapePopulatedPlacesColor().B, settings.getShapePopulatedPlacesColor().G, settings.getShapePopulatedPlacesColor().R));
    cities.addNumericFilter(settings.getShapePopulatedPlacesFilterColumnName(), settings.getShapePopulatedPlacesNumbericFilter());
    cities.setTextFieldName(settings.getShapePopulatedPlacesTextColumnName());
    cities.setFontScale(settings.getShapePopulatedPlacesFontScale());
    cities.setThickness(settings.getShapePopulatedPlacesThickness());
    cities.setPointRadius(settings.getShapePopulatedPlacesPointradius());
    cities.drawShapeMercator(newImage, xStart, yStart, scale);

    if(settings.drawReceiver()) {
        PixelGeolocationCalculator::CartesianCoordinateF coordinate = PixelGeolocationCalculator::coordinateToMercatorProjection<float>({settings.getReceiverLatitude(), settings.getReceiverLongitude(), 0}, mEarthRadius + mAltitude, scale);
        coordinate.x -= xStart;
        coordinate.y -= yStart;
        cv::drawMarker(newImage, cv::Point2d(coordinate.x, coordinate.y), cv::Scalar(0, 0, 0), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness() + 1);
        cv::drawMarker(newImage, cv::Point2d(coordinate.x, coordinate.y), cv::Scalar(settings.getReceiverColor().B, settings.getReceiverColor().G, settings.getReceiverColor().R), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness());
    }

    return newImage;
}

// Concept for using ThinPlateSplineShapeTransform. Unfortunately it is extremly slow for big images
/*cv::Mat SpreadImage::mercatorProjection(const cv::Mat &image, const PixelGeolocationCalculator &geolocationCalculator, ProgressCallback progressCallback)
{
    cv::Point2f srcTri[3];
    cv::Point2f dstTri[3];

    double MinX = std::min(geolocationCalculator.getTopLeftMercator().x, std::min(geolocationCalculator.getTopRightMercator().x, std::min(geolocationCalculator.getBottomLeftMercator().x, geolocationCalculator.getBottomRightMercator().x)));
    double MinY = std::min(geolocationCalculator.getTopLeftMercator().y, std::min(geolocationCalculator.getTopRightMercator().y, std::min(geolocationCalculator.getBottomLeftMercator().y, geolocationCalculator.getBottomRightMercator().y)));
    double MaxX = std::max(geolocationCalculator.getTopLeftMercator().x, std::max(geolocationCalculator.getTopRightMercator().x, std::max(geolocationCalculator.getBottomLeftMercator().x, geolocationCalculator.getBottomRightMercator().x)));
    double MaxY = std::max(geolocationCalculator.getTopLeftMercator().y, std::max(geolocationCalculator.getTopRightMercator().y, std::max(geolocationCalculator.getBottomLeftMercator().y, geolocationCalculator.getBottomRightMercator().y)));

    int width = static_cast<int>(std::abs(MaxX - MinX));
    int height = static_cast<int>(std::abs(MaxY - MinY));

    int xStart = static_cast<int>(std::min(MinX, MaxX));
    int yStart = static_cast<int>(std::min(MinY, MaxY));

    int imageWithGeorefHeight = geolocationCalculator.getGeorefMaxImageHeight() < image.size().height ? geolocationCalculator.getGeorefMaxImageHeight() : image.size().height;

    cv::Mat newImage;
    cv::Mat paddedImage = cv::Mat::zeros(height, width, image.type());
    image.copyTo(paddedImage.rowRange(0, image.size().height).colRange(0, image.size().width));

    auto tpsTransform = cv::createThinPlateSplineShapeTransformer();
    std::vector<cv::Point2f> sourcePoints, targetPoints;

    for (int y = 0; y < imageWithGeorefHeight; y += 50) {
        for (int x = 0; x < image.size().width; x += 50) {
            const PixelGeolocationCalculator::CartesianCoordinateF p1 = geolocationCalculator.getMercatorAt(x, y);
            sourcePoints.push_back(cv::Point2f(x, y));
            targetPoints.push_back(cv::Point2f((int)(p1.x + (-xStart)), (int)(p1.y + (-yStart))));
        }
    }

    std::vector<cv::DMatch> matches;
    for (unsigned int i = 0; i < sourcePoints.size(); i++) {
        matches.push_back(cv::DMatch(i, i, 0));
    }
    //tpsTransform->estimateTransformation(sourcePoints, targetPoints, matches);
    tpsTransform->estimateTransformation(targetPoints, sourcePoints, matches);
    std::vector<cv::Point2f> transPoints;
    tpsTransform->applyTransformation(sourcePoints, transPoints);
    tpsTransform->warpImage(paddedImage, newImage, cv::INTER_LINEAR);

    //Todo: overlays here

    return newImage;
}*/

cv::Mat SpreadImage::mercatorProjection(const std::list<cv::Mat> &images, const std::list<PixelGeolocationCalculator> &geolocationCalculators, float scale, SpreadImage::ProgressCallback progressCallback)
{
    float MinX = std::numeric_limits<float>::max();
    float MinY = std::numeric_limits<float>::max();
    float MaxX = std::numeric_limits<float>::min();
    float MaxY = std::numeric_limits<float>::min();
    float corner;

    if (images.size() != geolocationCalculators.size()) {
        return cv::Mat();
    }

    std::list<PixelGeolocationCalculator>::const_iterator geolocationCalculator;
    for (geolocationCalculator = geolocationCalculators.begin(); geolocationCalculator != geolocationCalculators.end(); ++geolocationCalculator) {

        PixelGeolocationCalculator::CartesianCoordinateF topLeft = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator->getCoordinateTopLeft(), geolocationCalculator->getSatelliteHeight(), scale);
        PixelGeolocationCalculator::CartesianCoordinateF topRight = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator->getCoordinateTopRight(), geolocationCalculator->getSatelliteHeight(), scale);
        PixelGeolocationCalculator::CartesianCoordinateF bottomLeft = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator->getCoordinateBottomLeft(), geolocationCalculator->getSatelliteHeight(), scale);
        PixelGeolocationCalculator::CartesianCoordinateF bottomRight = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator->getCoordinateBottomRight(), geolocationCalculator->getSatelliteHeight(), scale);

        corner = std::min(topLeft.x, std::min(topRight.x, std::min(bottomLeft.x, bottomRight.x)));
        MinX = corner < MinX ? corner : MinX;

        corner = std::min(topLeft.y, std::min(topRight.y, std::min(bottomLeft.y, bottomRight.y)));
        MinY = corner < MinY ? corner : MinY;

        corner = std::max(topLeft.x, std::max(topRight.x, std::max(bottomLeft.x, bottomRight.x)));
        MaxX = corner > MaxX ? corner : MaxX;

        corner = std::max(topLeft.y, std::max(topRight.y, std::max(bottomLeft.y, bottomRight.y)));
        MaxY = corner > MaxY ? corner : MaxY;
    }

    int width = static_cast<int>(std::abs(MaxX - MinX));
    int height = static_cast<int>(std::abs(MaxY - MinY));
    float xStart = std::min(MinX, MaxX);
    float yStart = std::min(MinY, MaxY);

    std::list<cv::Mat>::const_iterator image;
    image = images.begin();
    geolocationCalculator = geolocationCalculators.begin();

    std::vector<cv::Mat> newImages;
    PixelGeolocationCalculator::CartesianCoordinateF p1, p2, p3;

    for (; image != images.end(); ++image, ++geolocationCalculator) {
        cv::Mat newImage = cv::Mat::zeros(height, width, image->type());
        int imageWithGeorefHeight = geolocationCalculator->getGeorefMaxImageHeight() < image->size().height ? geolocationCalculator->getGeorefMaxImageHeight() : image->size().height;

        for (int y = 0; y < imageWithGeorefHeight - 10; y += 10)
        {
            if(progressCallback) {
                progressCallback(static_cast<float>(y) / imageWithGeorefHeight * 100.0f);
            }
            for (int x = 0; x < image->size().width - 10; x += 10)
            {
                const PixelGeolocationCalculator::CartesianCoordinateF p1 = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator->getCoordinateAt(x, y), geolocationCalculator->getSatelliteHeight(), scale);
                const PixelGeolocationCalculator::CartesianCoordinateF p2 = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator->getCoordinateAt(x + 10, y), geolocationCalculator->getSatelliteHeight(), scale);
                const PixelGeolocationCalculator::CartesianCoordinateF p3 = PixelGeolocationCalculator::coordinateToMercatorProjection<float>(geolocationCalculator->getCoordinateAt(x, y + 10), geolocationCalculator->getSatelliteHeight(), scale);

                cv::Point2f srcTri[3];
                cv::Point2f dstTri[3];

                srcTri[0] = cv::Point2f( x, y );
                srcTri[1] = cv::Point2f( x + 10, y );
                srcTri[2] = cv::Point2f( x, y + 10 );

                dstTri[0] = cv::Point2f(p1.x, p1.y);
                dstTri[1] = cv::Point2f(p2.x, p2.y);
                dstTri[2] = cv::Point2f(p3.x, p3.y);
                affineTransform(*image, newImage, srcTri, dstTri, -xStart, -yStart);
            }
        }

        newImages.push_back(newImage);
    }

    std::vector<cv::Mat>::iterator it = newImages.begin();

    cv::Mat composite = it->clone();
    ++it;

    composite.convertTo(composite, CV_32FC3);

    for(; it != newImages.end(); ++it) {
        it->convertTo(*it, CV_32FC3);

        cv::Mat grayScale1;
        cv::Mat alpha1;
        cv::bilateralFilter(composite, grayScale1, 19, 75, 75);
        cv::cvtColor(grayScale1, grayScale1, cv::COLOR_BGR2GRAY);

        cv::threshold(grayScale1, alpha1, 0, 255, cv::THRESH_BINARY);

        cv::Mat grayScale2;
        cv::Mat alpha2;
        cv::bilateralFilter(*it, grayScale2, 19, 75, 75);
        cv::cvtColor(grayScale2, grayScale2, cv::COLOR_BGR2GRAY);

        cv::threshold(grayScale2, alpha2, 0, 255, cv::THRESH_BINARY);

        cv::Mat mask;
        cv::bitwise_and(alpha1, alpha2, mask);

        std::vector<cv::Mat> channels;
        channels.push_back(mask);
        channels.push_back(mask);
        channels.push_back(mask);
        cv::merge(channels, mask);

        cv::imwrite("mask.png", mask);

        mask.convertTo(mask, CV_32FC3, 1/255.0);

        int start0 = findImageStart(composite);
        int start1 = findImageStart(*it);
        bool leftToRight = start0 < start1;

        cv::Mat blendmask = blendMask(mask, leftToRight);
        cv::multiply(cv::Scalar::all(1.0)-blendmask, composite, composite);
        blendmask = blendMask(mask, !leftToRight);
        cv::multiply(cv::Scalar::all(1.0)-blendmask, *it, *it);

        cv::add(composite, *it, composite);
    }


    cv::Mat mapOverlay = cv::Mat::zeros(height, width, composite.type());
    Settings &settings = Settings::getInstance();
    GIS::ShapeRenderer graticules(settings.getResourcesPath() + settings.getShapeGraticulesFile(), cv::Scalar(settings.getShapeGraticulesColor().B, settings.getShapeGraticulesColor().G, settings.getShapeGraticulesColor().R));
    graticules.setThickness(settings.getShapeGraticulesThickness());
    graticules.drawShapeMercator(mapOverlay, xStart, yStart, scale);

    GIS::ShapeRenderer coastLines(settings.getResourcesPath() + settings.getShapeCoastLinesFile(), cv::Scalar(settings.getShapeCoastLinesColor().B, settings.getShapeCoastLinesColor().G, settings.getShapeCoastLinesColor().R));
    coastLines.setThickness(settings.getShapeCoastLinesThickness());
    coastLines.drawShapeMercator(mapOverlay, xStart, yStart, scale);

    GIS::ShapeRenderer countryBorders(settings.getResourcesPath() + settings.getShapeBoundaryLinesFile(), cv::Scalar(settings.getShapeBoundaryLinesColor().B, settings.getShapeBoundaryLinesColor().G, settings.getShapeBoundaryLinesColor().R));
    countryBorders.setThickness(settings.getShapeBoundaryLinesThickness());
    countryBorders.drawShapeMercator(mapOverlay, xStart, yStart, scale);

    GIS::ShapeRenderer cities(settings.getResourcesPath() + settings.getShapePopulatedPlacesFile(), cv::Scalar(settings.getShapePopulatedPlacesColor().B, settings.getShapePopulatedPlacesColor().G, settings.getShapePopulatedPlacesColor().R));
    cities.addNumericFilter(settings.getShapePopulatedPlacesFilterColumnName(), settings.getShapePopulatedPlacesNumbericFilter());
    cities.setTextFieldName(settings.getShapePopulatedPlacesTextColumnName());
    cities.setFontScale(settings.getShapePopulatedPlacesFontScale());
    cities.setThickness(settings.getShapePopulatedPlacesThickness());
    cities.setPointRadius(settings.getShapePopulatedPlacesPointradius());
    cities.drawShapeMercator(mapOverlay, xStart, yStart, scale);

    if(settings.drawReceiver()) {
        PixelGeolocationCalculator::CartesianCoordinateF coordinate = PixelGeolocationCalculator::coordinateToMercatorProjection<float>({settings.getReceiverLatitude(), settings.getReceiverLongitude(), 0}, mEarthRadius + mAltitude, scale);
        coordinate.x -= xStart;
        coordinate.y -= yStart;
        cv::drawMarker(mapOverlay, cv::Point2d(coordinate.x, coordinate.y), cv::Scalar(0, 0, 0), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness() + 1);
        cv::drawMarker(mapOverlay, cv::Point2d(coordinate.x, coordinate.y), cv::Scalar(settings.getReceiverColor().B, settings.getReceiverColor().G, settings.getReceiverColor().R), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness());
    }

    cv::Mat mapOverlayGrayScale;
    cv::Mat alpha;
    cv::cvtColor(mapOverlay, mapOverlayGrayScale, cv::COLOR_BGR2GRAY);
    cv::threshold(mapOverlayGrayScale, alpha, 0, 255, cv::THRESH_BINARY);
    alpha.convertTo(alpha, CV_8SC3);

    //add map overlay
    cv::bitwise_and(mapOverlay, mapOverlay, composite, alpha);

    return composite;
}

cv::Mat SpreadImage::equidistantProjection(const cv::Mat &image, const PixelGeolocationCalculator &geolocationCalculator, float scale, ProgressCallback progressCallback)
{
    cv::Point2f srcTri[3];
    cv::Point2f dstTri[3];

    const CoordGeodetic &center = geolocationCalculator.getCenterCoordinate();

    PixelGeolocationCalculator::CartesianCoordinateF topLeft = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator.getCoordinateTopLeft(), center, geolocationCalculator.getSatelliteHeight(), scale);
    PixelGeolocationCalculator::CartesianCoordinateF topRight = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator.getCoordinateTopRight(), center, geolocationCalculator.getSatelliteHeight(), scale);
    PixelGeolocationCalculator::CartesianCoordinateF bottomLeft = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator.getCoordinateBottomLeft(), center, geolocationCalculator.getSatelliteHeight(), scale);
    PixelGeolocationCalculator::CartesianCoordinateF bottomRight = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator.getCoordinateBottomRight(), center, geolocationCalculator.getSatelliteHeight(), scale);

    double MinX = std::min(topLeft.x, std::min(topRight.x, std::min(bottomLeft.x, bottomRight.x)));
    double MinY = std::min(topLeft.y, std::min(topRight.y, std::min(bottomLeft.y, bottomRight.y)));
    double MaxX = std::max(topLeft.x, std::max(topRight.x, std::max(bottomLeft.x, bottomRight.x)));
    double MaxY = std::max(topLeft.y, std::max(topRight.y, std::max(bottomLeft.y, bottomRight.y)));

    int width = static_cast<int>(std::abs(MaxX - MinX));
    int height = static_cast<int>(std::abs(MaxY - MinY));

    int xStart = static_cast<int>(std::min(MinX, MaxX));
    int yStart = static_cast<int>(std::min(MinY, MaxY));

    int imageWithGeorefHeight = geolocationCalculator.getGeorefMaxImageHeight() < image.size().height ? geolocationCalculator.getGeorefMaxImageHeight() : image.size().height;

    cv::Mat newImage = cv::Mat::zeros(height, width, image.type());

    for (int y = 0; y < imageWithGeorefHeight - 10; y += 10)
    {
        if(progressCallback) {
            progressCallback(static_cast<float>(y) / imageWithGeorefHeight * 100.0f);
        }
        for (int x = 0; x < image.size().width - 10; x += 10)
        {
            const PixelGeolocationCalculator::CartesianCoordinateF p1 = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator.getCoordinateAt(x, y), center, geolocationCalculator.getSatelliteHeight(), scale);
            const PixelGeolocationCalculator::CartesianCoordinateF p2 = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator.getCoordinateAt(x + 10, y), center, geolocationCalculator.getSatelliteHeight(), scale);
            const PixelGeolocationCalculator::CartesianCoordinateF p3 = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator.getCoordinateAt(x, y + 10), center, geolocationCalculator.getSatelliteHeight(), scale);

            srcTri[0] = cv::Point2f( x, y );
            srcTri[1] = cv::Point2f( x + 10, y );
            srcTri[2] = cv::Point2f( x, y + 10 );

            dstTri[0] = cv::Point2f(p1.x, p1.y);
            dstTri[1] = cv::Point2f(p2.x, p2.y);
            dstTri[2] = cv::Point2f(p3.x, p3.y);
            affineTransform(image, newImage, srcTri, dstTri, -xStart, -yStart);
        }
    }

    float centerLatitude = static_cast<float>(geolocationCalculator.getCenterCoordinate().latitude * (180.0 / M_PI));
    float centerLongitude = static_cast<float>(geolocationCalculator.getCenterCoordinate().longitude * (180.0 / M_PI));

    Settings &settings = Settings::getInstance();

    GIS::ShapeRenderer graticules(settings.getResourcesPath() + settings.getShapeGraticulesFile(), cv::Scalar(settings.getShapeGraticulesColor().B, settings.getShapeGraticulesColor().G, settings.getShapeGraticulesColor().R));
    graticules.setThickness(settings.getShapeGraticulesThickness());
    graticules.drawShapeEquidistant(newImage, xStart, yStart, centerLatitude, centerLongitude, scale);

    GIS::ShapeRenderer coastLines(settings.getResourcesPath() + settings.getShapeCoastLinesFile(), cv::Scalar(settings.getShapeCoastLinesColor().B, settings.getShapeCoastLinesColor().G, settings.getShapeCoastLinesColor().R));
    coastLines.setThickness(settings.getShapeCoastLinesThickness());
    coastLines.drawShapeEquidistant(newImage, xStart, yStart, centerLatitude, centerLongitude, scale);

    GIS::ShapeRenderer countryBorders(settings.getResourcesPath() + settings.getShapeBoundaryLinesFile(), cv::Scalar(settings.getShapeBoundaryLinesColor().B, settings.getShapeBoundaryLinesColor().G, settings.getShapeBoundaryLinesColor().R));
    countryBorders.setThickness(settings.getShapeBoundaryLinesThickness());
    countryBorders.drawShapeEquidistant(newImage, xStart, yStart, centerLatitude, centerLongitude, scale);

    GIS::ShapeRenderer cities(settings.getResourcesPath() + settings.getShapePopulatedPlacesFile(), cv::Scalar(settings.getShapePopulatedPlacesColor().B, settings.getShapePopulatedPlacesColor().G, settings.getShapePopulatedPlacesColor().R));
    cities.setFontScale(settings.getShapePopulatedPlacesFontScale());
    cities.setThickness(settings.getShapePopulatedPlacesThickness());
    cities.setPointRadius(settings.getShapePopulatedPlacesPointradius());
    cities.addNumericFilter(settings.getShapePopulatedPlacesFilterColumnName(), settings.getShapePopulatedPlacesNumbericFilter());
    cities.setTextFieldName(settings.getShapePopulatedPlacesTextColumnName());
    cities.drawShapeEquidistant(newImage, xStart, yStart, centerLatitude, centerLongitude, scale);

    if(settings.drawReceiver()) {
        PixelGeolocationCalculator::CartesianCoordinateF coordinate = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>({settings.getReceiverLatitude(), settings.getReceiverLongitude(), 0}, {centerLatitude, centerLongitude, 0}, mEarthRadius + mAltitude, scale);
        coordinate.x -= xStart;
        coordinate.y -= yStart;
        cv::drawMarker(newImage, cv::Point2d(coordinate.x, coordinate.y), cv::Scalar(0, 0, 0), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness() + 1);
        cv::drawMarker(newImage, cv::Point2d(coordinate.x, coordinate.y), cv::Scalar(settings.getReceiverColor().B, settings.getReceiverColor().G, settings.getReceiverColor().R), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness());
    }

    return newImage;
}

cv::Mat SpreadImage::equidistantProjection(const std::list<cv::Mat> &images, const std::list<PixelGeolocationCalculator> &geolocationCalculators, float scale, SpreadImage::ProgressCallback progressCallback)
{
    float MinX = std::numeric_limits<float>::max();
    float MinY = std::numeric_limits<float>::max();
    float MaxX = std::numeric_limits<float>::min();
    float MaxY = std::numeric_limits<float>::min();
    float corner;

    if (images.size() != geolocationCalculators.size()) {
        return cv::Mat();
    }

    std::list<CoordGeodetic> centerCoordinates;
    for(const auto &c : geolocationCalculators) {
        centerCoordinates.push_back(c.getCenterCoordinate());
    }
    CoordGeodetic sumOfCenterCoordinates = std::accumulate(centerCoordinates.begin(), centerCoordinates.end(), CoordGeodetic(0, 0, 0));

    float centerLatitude = static_cast<float>(sumOfCenterCoordinates.latitude / centerCoordinates.size() * (180.0 / M_PI));
    float centerLongitude = static_cast<float>(sumOfCenterCoordinates.longitude / centerCoordinates.size() * (180.0 / M_PI));
    CoordGeodetic center = CoordGeodetic(centerLatitude, centerLongitude, 0);

    std::list<PixelGeolocationCalculator>::const_iterator geolocationCalculator;
    for (geolocationCalculator = geolocationCalculators.begin(); geolocationCalculator != geolocationCalculators.end(); ++geolocationCalculator) {

        PixelGeolocationCalculator::CartesianCoordinateF topLeft = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator->getCoordinateTopLeft(), center, geolocationCalculator->getSatelliteHeight(), scale);
        PixelGeolocationCalculator::CartesianCoordinateF topRight = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator->getCoordinateTopRight(), center, geolocationCalculator->getSatelliteHeight(), scale);
        PixelGeolocationCalculator::CartesianCoordinateF bottomLeft = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator->getCoordinateBottomLeft(), center, geolocationCalculator->getSatelliteHeight(), scale);
        PixelGeolocationCalculator::CartesianCoordinateF bottomRight = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator->getCoordinateBottomRight(), center, geolocationCalculator->getSatelliteHeight(),scale);

        corner = std::min(topLeft.x, std::min(topRight.x, std::min(bottomLeft.x, bottomRight.x)));
        MinX = corner < MinX ? corner : MinX;

        corner = std::min(topLeft.y, std::min(topRight.y, std::min(bottomLeft.y, bottomRight.y)));
        MinY = corner < MinY ? corner : MinY;

        corner = std::max(topLeft.x, std::max(topRight.x, std::max(bottomLeft.x, bottomRight.x)));
        MaxX = corner > MaxX ? corner : MaxX;

        corner = std::max(topLeft.y, std::max(topRight.y, std::max(bottomLeft.y, bottomRight.y)));
        MaxY = corner > MaxY ? corner : MaxY;
    }

    int width = static_cast<int>(std::abs(MaxX - MinX));
    int height = static_cast<int>(std::abs(MaxY - MinY));
    float xStart = std::min(MinX, MaxX);
    float yStart = std::min(MinY, MaxY);

    std::list<cv::Mat>::const_iterator image;
    image = images.begin();
    geolocationCalculator = geolocationCalculators.begin();

    std::vector<cv::Mat> newImages;
    PixelGeolocationCalculator::CartesianCoordinateF p1, p2, p3;

    for (; image != images.end(); ++image, ++geolocationCalculator) {
        cv::Mat newImage = cv::Mat::zeros(height, width, image->type());
        int imageWithGeorefHeight = geolocationCalculator->getGeorefMaxImageHeight() < image->size().height ? geolocationCalculator->getGeorefMaxImageHeight() : image->size().height;

        for (int y = 0; y < imageWithGeorefHeight - 10; y += 10)
        {
            if(progressCallback) {
                progressCallback(static_cast<float>(y) / imageWithGeorefHeight * 100.0f);
            }
            for (int x = 0; x < image->size().width - 10; x += 10)
            {
                p1 = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator->getCoordinateAt(x, y), center, geolocationCalculator->getSatelliteHeight(), scale);
                p2 = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator->getCoordinateAt(x + 10, y), center, geolocationCalculator->getSatelliteHeight(), scale);
                p3 = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>(geolocationCalculator->getCoordinateAt(x, y + 10), center, geolocationCalculator->getSatelliteHeight(), scale);


                cv::Point2f srcTri[3];
                cv::Point2f dstTri[3];

                srcTri[0] = cv::Point2f( x, y );
                srcTri[1] = cv::Point2f( x + 10, y );
                srcTri[2] = cv::Point2f( x, y + 10 );

                dstTri[0] = cv::Point2f(p1.x, p1.y);
                dstTri[1] = cv::Point2f(p2.x, p2.y);
                dstTri[2] = cv::Point2f(p3.x, p3.y);
                affineTransform(*image, newImage, srcTri, dstTri, -xStart, -yStart);
            }
        }

        newImages.push_back(newImage);
    }

    std::vector<cv::Mat>::iterator it = newImages.begin();

    cv::Mat composite = it->clone();
    ++it;

    composite.convertTo(composite, CV_32FC3);

    for(; it != newImages.end(); ++it) {
        it->convertTo(*it, CV_32FC3);

        cv::Mat grayScale1;
        cv::Mat alpha1;
        cv::bilateralFilter(composite, grayScale1, 19, 75, 75);
        cv::cvtColor(grayScale1, grayScale1, cv::COLOR_BGR2GRAY);

        cv::threshold(grayScale1, alpha1, 0, 255, cv::THRESH_BINARY);

        cv::Mat grayScale2;
        cv::Mat alpha2;
        cv::bilateralFilter(*it, grayScale2, 19, 75, 75);
        cv::cvtColor(grayScale2, grayScale2, cv::COLOR_BGR2GRAY);

        cv::threshold(grayScale2, alpha2, 0, 255, cv::THRESH_BINARY);

        cv::Mat mask;
        cv::bitwise_and(alpha1, alpha2, mask);

        std::vector<cv::Mat> channels;
        channels.push_back(mask);
        channels.push_back(mask);
        channels.push_back(mask);
        cv::merge(channels, mask);

        cv::imwrite("mask.png", mask);

        mask.convertTo(mask, CV_32FC3, 1/255.0);

        int start0 = findImageStart(composite);
        int start1 = findImageStart(*it);
        bool leftToRight = start0 < start1;

        cv::Mat blendmask = blendMask(mask, leftToRight);
        cv::multiply(cv::Scalar::all(1.0)-blendmask, composite, composite);
        blendmask = blendMask(mask, !leftToRight);
        cv::multiply(cv::Scalar::all(1.0)-blendmask, *it, *it);

        cv::add(composite, *it, composite);
    }

    cv::Mat mapOverlay = cv::Mat::zeros(height, width, composite.type());
    Settings &settings = Settings::getInstance();

    GIS::ShapeRenderer graticules(settings.getResourcesPath() + settings.getShapeGraticulesFile(), cv::Scalar(settings.getShapeGraticulesColor().B, settings.getShapeGraticulesColor().G, settings.getShapeGraticulesColor().R));
    graticules.setThickness(settings.getShapeGraticulesThickness());
    graticules.drawShapeEquidistant(mapOverlay, xStart, yStart, centerLatitude, centerLongitude, scale);

    GIS::ShapeRenderer coastLines(settings.getResourcesPath() + settings.getShapeCoastLinesFile(), cv::Scalar(settings.getShapeCoastLinesColor().B, settings.getShapeCoastLinesColor().G, settings.getShapeCoastLinesColor().R));
    coastLines.setThickness(settings.getShapeCoastLinesThickness());
    coastLines.drawShapeEquidistant(mapOverlay, xStart, yStart, centerLatitude, centerLongitude, scale);

    GIS::ShapeRenderer countryBorders(settings.getResourcesPath() + settings.getShapeBoundaryLinesFile(), cv::Scalar(settings.getShapeBoundaryLinesColor().B, settings.getShapeBoundaryLinesColor().G, settings.getShapeBoundaryLinesColor().R));
    countryBorders.setThickness(settings.getShapeBoundaryLinesThickness());
    countryBorders.drawShapeEquidistant(mapOverlay, xStart, yStart, centerLatitude, centerLongitude, scale);

    GIS::ShapeRenderer cities(settings.getResourcesPath() + settings.getShapePopulatedPlacesFile(), cv::Scalar(settings.getShapePopulatedPlacesColor().B, settings.getShapePopulatedPlacesColor().G, settings.getShapePopulatedPlacesColor().R));
    cities.addNumericFilter(settings.getShapePopulatedPlacesFilterColumnName(), settings.getShapePopulatedPlacesNumbericFilter());
    cities.setTextFieldName(settings.getShapePopulatedPlacesTextColumnName());
    cities.setFontScale(settings.getShapePopulatedPlacesFontScale());
    cities.setThickness(settings.getShapePopulatedPlacesThickness());
    cities.setPointRadius(settings.getShapePopulatedPlacesPointradius());
    cities.drawShapeEquidistant(mapOverlay, xStart, yStart, centerLatitude, centerLongitude, scale);

    if(settings.drawReceiver()) {
        PixelGeolocationCalculator::CartesianCoordinateF coordinate = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection<float>({settings.getReceiverLatitude(), settings.getReceiverLongitude(), 0}, center, mEarthRadius + mAltitude, scale);
        coordinate.x -= xStart;
        coordinate.y -= yStart;
        cv::drawMarker(mapOverlay, cv::Point2d(coordinate.x, coordinate.y), cv::Scalar(0, 0, 0), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness() + 1);
        cv::drawMarker(mapOverlay, cv::Point2d(coordinate.x, coordinate.y), cv::Scalar(settings.getReceiverColor().B, settings.getReceiverColor().G, settings.getReceiverColor().R), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness());
    }

    cv::Mat mapOverlayGrayScale;
    cv::Mat alpha;
    cv::cvtColor(mapOverlay, mapOverlayGrayScale, cv::COLOR_BGR2GRAY);
    cv::threshold(mapOverlayGrayScale, alpha, 0, 255, cv::THRESH_BINARY);
    alpha.convertTo(alpha, CV_8SC3);

    //add map overlay
    cv::bitwise_and(mapOverlay, mapOverlay, composite, alpha);

    return composite;
}

void SpreadImage::affineTransform(const cv::Mat& src, cv::Mat& dst, const cv::Point2f source[], const cv::Point2f destination[], int originX, int originY)
{
    // Create an empty 3x1 matrix for storing original frame coordinates
    cv::Mat xOrg = cv::Mat(3, 1, CV_64FC1);

    // Create an empty 3x1 matrix for storing transformed frame coordinates
    cv::Mat xTrans = cv::Mat(3, 1, CV_64FC1);

    cv::Mat transform = cv::getAffineTransform( source, destination );
    transform.at<double>(0,2) += originX;
    transform.at<double>(1,2) += originY;

    xOrg.at<double>(0,0) = source[0].x;
    xOrg.at<double>(1,0) = source[0].y;
    xOrg.at<double>(2,0) = 1;
    xTrans = transform * xOrg;
    int x1 = static_cast<const int>(std::floor(xTrans.at<double>(0,0)));
    int y1 = static_cast<const int>(std::floor(xTrans.at<double>(1,0)));

    xOrg.at<double>(0,0) = source[1].x;
    xOrg.at<double>(1,0) = source[1].y;
    xOrg.at<double>(2,0) = 1;
    xTrans = transform * xOrg;
    int x2 = static_cast<const int>(std::floor(xTrans.at<double>(0,0)));
    int y2 = static_cast<const int>(std::floor(xTrans.at<double>(1,0)));


    xOrg.at<double>(0,0) = source[2].x;
    xOrg.at<double>(1,0) = source[2].y;
    xOrg.at<double>(2,0) = 1;
    xTrans = transform * xOrg;
    int x3 = static_cast<const int>(std::ceil(xTrans.at<double>(0,0)));
    int y3 = static_cast<const int>(std::ceil(xTrans.at<double>(1,0)));

    int x4 = x3 + (x2-x1);
    int y4 = y3 + (y2-y1);

    int minX = std::min(x1, std::min(x2, std::min(x3, x4)));
    int minY = std::min(y1, std::min(y2, std::min(y3, y4)));

    int maxX = std::max(x1, std::max(x2, std::max(x3, x4)));
    int maxY = std::max(y1, std::max(y2, std::max(y3, y4)));

    maxX += 5; //Should be available better solution than + 5
    maxY += 5;

    if(maxX >= dst.size().width) {
        maxX = dst.size().width-1;
    }

    if(maxY >= dst.size().height) {
        maxY = dst.size().height-1;
    }

    if(minX < 0) {
        minX = 0;
    }

    if(minY < 0) {
        minY = 0;
    }

    cv::Mat inverseTransform;
    cv::invertAffineTransform(transform, inverseTransform);

    for(int y = minY; y < maxY; y++) {
        for(int x = minX; x < maxX; x++) {
            xOrg.at<double>(0,0) = x;
            xOrg.at<double>(1,0) = y;
            xOrg.at<double>(2,0) = 1;

            xTrans = inverseTransform * xOrg;

            // Homogeneous to cartesian transformation
            const double srcX = xTrans.at<double>(0,0);
            const double srcY = xTrans.at<double>(1,0);

            // Make sure input boundary is not exceeded
            if(srcX >= src.size().width || srcY >= src.size().height || srcX < 0 || srcY < 0)
            {
                continue;
            }

            // Make sure src boundary is not exceeded
            if(srcX >= source[1].x || srcY >= source[2].y || srcX < source[0].x || srcY < source[0].y)
            {
                continue;
            }

            int y1 = static_cast<int>(floor(srcY));
            int y2 = y1+1;
            if(y2 >= src.size().height) {
                y2 = src.size().height - 1;
            }

            int x1 = static_cast<int>(floor(srcX));
            int x2 = x1 + 1;
            if(x2 >= src.size().width) {
                x2 = src.size().width - 1;
            }

            // Other possible solution but much slower
            //cv::Mat interpolated;
            //cv::Point2f srcPoint(srcX, srcY);
            //cv::remap(src, interpolated, cv::Mat(1, 1, CV_32FC2, &srcPoint), cv::noArray(), cv::INTER_LINEAR, cv::BORDER_REFLECT_101);
            //dst.at<cv::Vec3b>(y, x) = interpolated.at<cv::Vec3b>(0,0);

            //Todo: order does matter when image is flipped
            cv::Vec3b interpolated = (src.at<cv::Vec3b>(y1, x2)) * ((srcX-x1)*(y2-srcY)) / ((y2-y1)*(x2-x1)) +
                (src.at<cv::Vec3b>(y2, x1)) * (((srcY-y1)*(x2-srcX)) / ((y2-y1)*(x2-x1))) +
                (src.at<cv::Vec3b>(y1, x1)) * (((x2-srcX)*(y2-srcY)) / ((y2-y1)*(x2-x1))) +
                (src.at<cv::Vec3b>(y2, x2)) * ((srcY-y1)*(srcX-x1)) / ((y2-y1)*(x2-x1));

            dst.at<cv::Vec3b>(y, x) = interpolated;

        }
    }
}

cv::Mat SpreadImage::blendMask(const cv::Mat &mask, bool leftToRight)
{
    int xStart = 0;
    int xEnd = 0;
    int blendWidth = 0;
    float alpha;

    cv::Mat blendedMask = cv::Mat::zeros(mask.size().height, mask.size().width, mask.type());

    for(int y = 0; y < mask.size().height; ++y) {
        bool foundStart = false;
        bool foundEnd = false;
        for(int x = 0; x < mask.size().width; x++) {
            if(!foundStart && mask.at<cv::Vec3f>(y, x) != cv::Vec3f(0, 0, 0)) {
                foundStart = true;
                xStart = x;

                for (int temp = x; temp < mask.size().width; temp++) {
                    if(mask.at<cv::Vec3f>(y, temp) != cv::Vec3f(1, 1, 1)) {
                        xEnd = temp;
                        foundEnd = true;
                        blendWidth = xEnd - xStart;
                        break;
                    }
                }
            }

            if(foundStart && foundEnd && (x >= xStart && x <= xEnd)) {
                alpha = static_cast<float>(x - xStart) / blendWidth;
                alpha = leftToRight ? alpha : 1.0f - alpha;

                blendedMask.at<cv::Vec3f>(y, x) = mask.at<cv::Vec3f>(y, x) * alpha;
            } else if(foundStart && foundEnd && (x > xEnd)) {
                foundStart = false;
                foundEnd = false;
            }
        }
    }


    return blendedMask;
}

int SpreadImage::findImageStart(const cv::Mat &img)
{
    int i = img.size().width;
    for(int y = 0; y < img.size().height; ++y) {
        for(int x = 0; x < img.size().width; x++) {
            if(img.at<cv::Vec3f>(y, x) != cv::Vec3f(0, 0, 0)) {
                if(x < i) {
                    i = x;
                }
            }
        }
    }

    return i;
}

cv::MarkerTypes SpreadImage::stringToMarkerType(const std::string &markerType)
{
    auto itr = MarkerLookup.find(markerType);
    if( itr != MarkerLookup.end() ) {
        return itr->second;
    }
    return cv::MARKER_TRIANGLE_UP;
}
