#include "shaperenderer.h"
#include <vector>
#include "pixelgeolocationcalculator.h"

GIS::ShapeRenderer::ShapeRenderer(const ShapeReader &shapeReader, const cv::Scalar &color, int earthRadius, int altitude)
    : mShapeReader(shapeReader)
    , mColor(color)
    , mEarthRadius(earthRadius)
    , mAltitude(altitude)
{

}

GIS::ShapeRenderer::ShapeRenderer(const std::string shapeFile, const cv::Scalar &color, int earthRadius, int altitude)
    : mShapeReader(shapeFile)
    , mColor(color)
    , mEarthRadius(earthRadius)
    , mAltitude(altitude)
{

}

void GIS::ShapeRenderer::drawShapeMercator(cv::Mat &src, float xStart, float yStart)
{
    mShapeReader.load();

    if(mShapeReader.getShapeType() == ShapeReader::ShapeType::stPolyline) {
        ShapeReader::RecordIterator *recordIterator = mShapeReader.getRecordIterator();

        if(recordIterator) {
            for(recordIterator->begin(); *recordIterator != recordIterator->end(); ++(*recordIterator)) {
                ShapeReader::PolyLineIterator *polyLineIterator = mShapeReader.getPolyLineIterator(*recordIterator);

                if(polyLineIterator) {
                    std::vector<cv::Point> polyLines;
                    for(polyLineIterator->begin(); *polyLineIterator != polyLineIterator->end(); ++(*polyLineIterator)) {
                        //std::cout << polyLineIterator->point.x << " " << polyLineIterator->point.y << std::endl;

                        PixelGeolocationCalculator::CartesianCoordinateF coordinate = PixelGeolocationCalculator::coordinateToMercatorProjection(polyLineIterator->point.y, polyLineIterator->point.x, mEarthRadius + mAltitude);

                        coordinate.x += -xStart;
                        coordinate.y += -yStart;

                        polyLines.push_back(cv::Point2d(coordinate.x, coordinate.y));
                    }

                    if(polyLines.size() > 1) {
                        cv::polylines(src, polyLines, false, mColor, 5);
                    }

                    delete polyLineIterator;
                }
            }

            delete recordIterator;
        }
    } else if(mShapeReader.getShapeType() == ShapeReader::ShapeType::stPoint) {

    }
}

void GIS::ShapeRenderer::drawShapeEquidistant(cv::Mat &src, float xStart, float yStart, float xCenter, float yCenter)
{
    mShapeReader.load();

    if(mShapeReader.getShapeType() == ShapeReader::ShapeType::stPolyline) {
        ShapeReader::RecordIterator *recordIterator = mShapeReader.getRecordIterator();

        if(recordIterator) {
            for(recordIterator->begin(); *recordIterator != recordIterator->end(); ++(*recordIterator)) {
                ShapeReader::PolyLineIterator *polyLineIterator = mShapeReader.getPolyLineIterator(*recordIterator);

                if(polyLineIterator) {
                    std::vector<cv::Point> polyLines;
                    for(polyLineIterator->begin(); *polyLineIterator != polyLineIterator->end(); ++(*polyLineIterator)) {
                        //std::cout << polyLineIterator->point.x << " " << polyLineIterator->point.y << std::endl;

                        PixelGeolocationCalculator::CartesianCoordinateF coordinate = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection(polyLineIterator->point.y, polyLineIterator->point.x, xCenter, yCenter, mEarthRadius + mAltitude);

                        coordinate.x += -xStart;
                        coordinate.y += -yStart;

                        if(equidistantCheck(polyLineIterator->point.y, polyLineIterator->point.x, xCenter, yCenter)) {
                            polyLines.push_back(cv::Point2d(coordinate.x, coordinate.y));
                        }
                    }

                    if(polyLines.size() > 1) {
                        cv::polylines(src, polyLines, false, mColor, 5);
                    }

                    delete polyLineIterator;
                }
            }

            delete recordIterator;
        }
    } else if(mShapeReader.getShapeType() == ShapeReader::ShapeType::stPoint) {

    }
}

bool GIS::ShapeRenderer::equidistantCheck(float x, float y, float centerLongitude, float centerLatitude)
{
    int minLongitude = static_cast<int>(centerLongitude - 90);
    int maxLongitude = static_cast<int>(centerLongitude + 90);
    int minLatitude = static_cast<int>(centerLatitude - 45);
    int maxLatitude = static_cast<int>(centerLatitude + 45);

    //Normalize
    minLongitude = (minLongitude + 540) % 360 - 180;
    maxLongitude = (maxLongitude + 540) % 360 - 180;

    if(minLatitude < -90)
    {
        minLatitude = ((minLatitude + 270) % 180 - 90) * -1;
    }
    if(maxLatitude > 90)
    {
        maxLatitude = ((maxLatitude + 270) % 180 - 90) * -1;
    }



    if (x < minLongitude || x > maxLongitude || y < minLatitude || y > maxLatitude)
    {
        return false;
    }
    else
    {
        return true;
    }
}
