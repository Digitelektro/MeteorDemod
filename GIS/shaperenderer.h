#ifndef SHAPERENDERER_H
#define SHAPERENDERER_H

#include "shapereader.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <map>

namespace GIS {

class ShapeRenderer : public ShapeReader
{
public:
    ShapeRenderer(const std::string shapeFile, const cv::Scalar &color, int earthRadius = 6378, int altitude = 825);

    ShapeRenderer(const ShapeRenderer&) = delete;
    ShapeRenderer &operator = (const ShapeRenderer&) = delete;

    //Todo: these should be more generic
    void addNumericFilter(const std::string name, int value);
    void setTextFieldName(const std::string &name);

    void drawShapeMercator(cv::Mat &src, float xStart, float yStart);
    void drawShapeEquidistant(cv::Mat &src, float xStart, float yStart, float xCenter, float yCenter);

private:
    bool equidistantCheck(float x, float y, float centerLongitude, float centerLatitude);

private:
    cv::Scalar mColor;
    int mEarthRadius;
    int mAltitude;
    std::map<std::string, int> mfilter;
    std::string mTextFieldName;
};

} //namespace GIS

#endif // SHAPERENDERER_H
