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

void GIS::ShapeRenderer::addNumericFilter(const std::string name, int value)
{
    mfilter.insert(std::make_pair(name, value));
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
        ShapeReader::RecordIterator *recordIterator = mShapeReader.getRecordIterator();

        if(mfilter.size() == 0) {
            if(recordIterator) {
                for(recordIterator->begin(); *recordIterator != recordIterator->end(); ++(*recordIterator)) {
                    ShapeReader::Point point(*recordIterator);

                    PixelGeolocationCalculator::CartesianCoordinateF coordinate = PixelGeolocationCalculator::coordinateToMercatorProjection(point.y, point.x, mEarthRadius + mAltitude);
                    coordinate.x += -xStart;
                    coordinate.y += -yStart;

                    cv::drawMarker(src, cv::Point2d(coordinate.x, coordinate.y), mColor, cv::MARKER_TRIANGLE_UP, 20, 10);
                }
            }
        } else {
            const DbFileReader &dbFilereader = mShapeReader.getDbFilereader();
            const std::vector<DbFileReader::Field> fieldAttributes = dbFilereader.getFieldAttributes();

            if(recordIterator && mShapeReader.hasDbFile()) {
                uint32_t i = 0;
                for(recordIterator->begin(); *recordIterator != recordIterator->end(); ++(*recordIterator), ++i) {
                    ShapeReader::Point point(*recordIterator);
                    std::vector<std::string> fieldValues = dbFilereader.getFieldValues(i);

                    for(size_t n = 0; n < fieldAttributes.size(); n++) {
                        if(mfilter.count(fieldAttributes[n].fieldName) == 1) {
                            int population = 0;

                            try {
                                population = std::stoi(fieldValues[n]);
                            } catch (...) {
                                continue;
                            }

                            if(population >= mfilter[fieldAttributes[n].fieldName]) {
                                PixelGeolocationCalculator::CartesianCoordinateF coordinate = PixelGeolocationCalculator::coordinateToMercatorProjection(point.y, point.x, mEarthRadius + mAltitude);
                                coordinate.x += -xStart;
                                coordinate.y += -yStart;

                                cv::circle(src, cv::Point2d(coordinate.x, coordinate.y), 10, mColor, cv::FILLED);
                                cv::circle(src, cv::Point2d(coordinate.x, coordinate.y), 10, cv::Scalar(0,0,0), 2);
                            }

                            break;
                        }
                    }
                }
            }
        }
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
        ShapeReader::RecordIterator *recordIterator = mShapeReader.getRecordIterator();

        if(mfilter.size() == 0) {
            if(recordIterator) {
                for(recordIterator->begin(); *recordIterator != recordIterator->end(); ++(*recordIterator)) {
                    ShapeReader::Point point(*recordIterator);

                    PixelGeolocationCalculator::CartesianCoordinateF coordinate = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection(point.y, point.x, xCenter, yCenter, mEarthRadius + mAltitude);
                    coordinate.x += -xStart;
                    coordinate.y += -yStart;

                    cv::drawMarker(src, cv::Point2d(coordinate.x, coordinate.y), mColor, cv::MARKER_TRIANGLE_UP, 20, 10);
                }
            }
        } else {
            const DbFileReader &dbFilereader = mShapeReader.getDbFilereader();
            const std::vector<DbFileReader::Field> fieldAttributes = dbFilereader.getFieldAttributes();

            if(recordIterator && mShapeReader.hasDbFile()) {
                uint32_t i = 0;
                for(recordIterator->begin(); *recordIterator != recordIterator->end(); ++(*recordIterator), ++i) {
                    ShapeReader::Point point(*recordIterator);
                    std::vector<std::string> fieldValues = dbFilereader.getFieldValues(i);

                    for(size_t n = 0; n < fieldAttributes.size(); n++) {
                        if(mfilter.count(fieldAttributes[n].fieldName) == 1) {
                            int population = 0;

                            try {
                                population = std::stoi(fieldValues[n]);
                            } catch (...) {
                                continue;
                            }

                            if(population >= mfilter[fieldAttributes[n].fieldName]) {
                                PixelGeolocationCalculator::CartesianCoordinateF coordinate = PixelGeolocationCalculator::coordinateToAzimuthalEquidistantProjection(point.y, point.x, xCenter, yCenter, mEarthRadius + mAltitude);
                                coordinate.x += -xStart;
                                coordinate.y += -yStart;

                                cv::circle(src, cv::Point2d(coordinate.x, coordinate.y), 10, mColor, cv::FILLED);
                                cv::circle(src, cv::Point2d(coordinate.x, coordinate.y), 10, cv::Scalar(0,0,0), 2);
                            }

                            break;
                        }
                    }
                }
            }
        }
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
