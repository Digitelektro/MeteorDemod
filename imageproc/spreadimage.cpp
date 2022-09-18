#include "spreadimage.h"
#include "GIS/shaperenderer.h"
#include "settings.h"
#include <opencv2/shape/shape_transformer.hpp>
#include <cmath>

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

cv::Mat SpreadImage::mercatorProjection(const cv::Mat &image, const PixelGeolocationCalculator &geolocationCalculator, ProgressCallback progressCallback)
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

    //cv::Mat newImage(height, width, image.type());
    cv::Mat newImage = cv::Mat::zeros(height, width, image.type());

    for (int y = 0; y < imageWithGeorefHeight - 10; y += 10)
    {
        if(progressCallback) {
            progressCallback(static_cast<float>(y) / imageWithGeorefHeight * 100.0f);
        }
        for (int x = 0; x < image.size().width - 10; x += 10)
        {
            const PixelGeolocationCalculator::CartesianCoordinateF &p1 = geolocationCalculator.getMercatorAt(x, y);
            const PixelGeolocationCalculator::CartesianCoordinateF &p2 = geolocationCalculator.getMercatorAt(x + 10, y);
            const PixelGeolocationCalculator::CartesianCoordinateF &p3 = geolocationCalculator.getMercatorAt(x, y + 10);

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
    graticules.drawShapeMercator(newImage, xStart, yStart);

    GIS::ShapeRenderer coastLines(settings.getResourcesPath() + settings.getShapeCoastLinesFile(), cv::Scalar(settings.getShapeCoastLinesColor().B, settings.getShapeCoastLinesColor().G, settings.getShapeCoastLinesColor().R));
    coastLines.setThickness(settings.getShapeCoastLinesThickness());
    coastLines.drawShapeMercator(newImage, xStart, yStart);

    GIS::ShapeRenderer countryBorders(settings.getResourcesPath() + settings.getShapeBoundaryLinesFile(), cv::Scalar(settings.getShapeBoundaryLinesColor().B, settings.getShapeBoundaryLinesColor().G, settings.getShapeBoundaryLinesColor().R));
    countryBorders.setThickness(settings.getShapeBoundaryLinesThickness());
    countryBorders.drawShapeMercator(newImage, xStart, yStart);

    GIS::ShapeRenderer cities(settings.getResourcesPath() + settings.getShapePopulatedPlacesFile(), cv::Scalar(settings.getShapePopulatedPlacesColor().B, settings.getShapePopulatedPlacesColor().G, settings.getShapePopulatedPlacesColor().R));
    cities.addNumericFilter(settings.getShapePopulatedPlacesFilterColumnName(), settings.getShapePopulatedPlacesNumbericFilter());
    cities.setTextFieldName(settings.getShapePopulatedPlacesTextColumnName());
    cities.setFontScale(settings.getShapePopulatedPlacesFontScale());
    cities.setThickness(settings.getShapePopulatedPlacesThickness());
    cities.setPointRadius(settings.getShapePopulatedPlacesPointradius());
    cities.drawShapeMercator(newImage, xStart, yStart);

    if(settings.drawReceiver()) {
        PixelGeolocationCalculator::CartesianCoordinateF coordinate = PixelGeolocationCalculator::coordinateToMercatorProjection(settings.getReceiverLatitude(), settings.getReceiverLongitude(), mEarthRadius + mAltitude);
        coordinate.x -= xStart;
        coordinate.y -= yStart;
        cv::drawMarker(newImage, cv::Point2d(coordinate.x, coordinate.y), cv::Scalar(0, 0, 0), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness() + 1);
        cv::drawMarker(newImage, cv::Point2d(coordinate.x, coordinate.y), cv::Scalar(settings.getReceiverColor().B, settings.getReceiverColor().G, settings.getReceiverColor().R), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness());
    }

    return newImage;
}

cv::Mat SpreadImage::equidistantProjection(const cv::Mat &image, const PixelGeolocationCalculator &geolocationCalculator, ProgressCallback progressCallback)
{
    cv::Point2f srcTri[3];
    cv::Point2f dstTri[3];

    double MinX = std::min(geolocationCalculator.getTopLeftEquidistant().x, std::min(geolocationCalculator.getTopRightEquidistant().x, std::min(geolocationCalculator.getBottomLeftEquidistant().x, geolocationCalculator.getBottomRightEquidistant().x)));
    double MinY = std::min(geolocationCalculator.getTopLeftEquidistant().y, std::min(geolocationCalculator.getTopRightEquidistant().y, std::min(geolocationCalculator.getBottomLeftEquidistant().y, geolocationCalculator.getBottomRightEquidistant().y)));
    double MaxX = std::max(geolocationCalculator.getTopLeftEquidistant().x, std::max(geolocationCalculator.getTopRightEquidistant().x, std::max(geolocationCalculator.getBottomLeftEquidistant().x, geolocationCalculator.getBottomRightEquidistant().x)));
    double MaxY = std::max(geolocationCalculator.getTopLeftEquidistant().y, std::max(geolocationCalculator.getTopRightEquidistant().y, std::max(geolocationCalculator.getBottomLeftEquidistant().y, geolocationCalculator.getBottomRightEquidistant().y)));

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
            const PixelGeolocationCalculator::CartesianCoordinateF &p1 = geolocationCalculator.getEquidistantAt(x, y);
            const PixelGeolocationCalculator::CartesianCoordinateF &p2 = geolocationCalculator.getEquidistantAt(x + 10, y);
            const PixelGeolocationCalculator::CartesianCoordinateF &p3 = geolocationCalculator.getEquidistantAt(x, y + 10);

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
    graticules.drawShapeEquidistant(newImage, xStart, yStart, centerLatitude, centerLongitude);

    GIS::ShapeRenderer coastLines(settings.getResourcesPath() + settings.getShapeCoastLinesFile(), cv::Scalar(settings.getShapeCoastLinesColor().B, settings.getShapeCoastLinesColor().G, settings.getShapeCoastLinesColor().R));
    coastLines.setThickness(settings.getShapeCoastLinesThickness());
    coastLines.drawShapeEquidistant(newImage, xStart, yStart, centerLatitude, centerLongitude);

    GIS::ShapeRenderer countryBorders(settings.getResourcesPath() + settings.getShapeBoundaryLinesFile(), cv::Scalar(settings.getShapeBoundaryLinesColor().B, settings.getShapeBoundaryLinesColor().G, settings.getShapeBoundaryLinesColor().R));
    countryBorders.setThickness(settings.getShapeBoundaryLinesThickness());
    countryBorders.drawShapeEquidistant(newImage, xStart, yStart, centerLatitude, centerLongitude);

    GIS::ShapeRenderer cities(settings.getResourcesPath() + settings.getShapePopulatedPlacesFile(), cv::Scalar(settings.getShapePopulatedPlacesColor().B, settings.getShapePopulatedPlacesColor().G, settings.getShapePopulatedPlacesColor().R));
    cities.setFontScale(settings.getShapePopulatedPlacesFontScale());
    cities.setThickness(settings.getShapePopulatedPlacesThickness());
    cities.setPointRadius(settings.getShapePopulatedPlacesPointradius());
    cities.addNumericFilter(settings.getShapePopulatedPlacesFilterColumnName(), settings.getShapePopulatedPlacesNumbericFilter());
    cities.setTextFieldName(settings.getShapePopulatedPlacesTextColumnName());
    cities.drawShapeEquidistant(newImage, xStart, yStart, centerLatitude, centerLongitude);

    if(settings.drawReceiver()) {
        PixelGeolocationCalculator::CartesianCoordinateF coordinate = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection(settings.getReceiverLatitude(), settings.getReceiverLongitude(), centerLatitude, centerLongitude, mEarthRadius + mAltitude);
        coordinate.x -= xStart;
        coordinate.y -= yStart;
        cv::drawMarker(newImage, cv::Point2d(coordinate.x, coordinate.y), cv::Scalar(0, 0, 0), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness() + 1);
        cv::drawMarker(newImage, cv::Point2d(coordinate.x, coordinate.y), cv::Scalar(settings.getReceiverColor().B, settings.getReceiverColor().G, settings.getReceiverColor().R), stringToMarkerType(settings.getReceiverMarkType()), settings.getReceiverSize(), settings.getReceiverThickness());
    }

    return newImage;
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

cv::MarkerTypes SpreadImage::stringToMarkerType(const std::string &markerType)
{
    auto itr = MarkerLookup.find(markerType);
    if( itr != MarkerLookup.end() ) {
        return itr->second;
    }
    return cv::MARKER_TRIANGLE_UP;
}
