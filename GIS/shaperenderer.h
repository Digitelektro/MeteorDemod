#ifndef SHAPERENDERER_H
#define SHAPERENDERER_H

#include <map>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "shapereader.h"

namespace GIS {

class ShapeRenderer : public ShapeReader {
  public:
    ShapeRenderer(const std::string shapeFile, const cv::Scalar& color, int earthRadius = 6378, int altitude = 825);

    ShapeRenderer(const ShapeRenderer&) = delete;
    ShapeRenderer& operator=(const ShapeRenderer&) = delete;

    // Todo: these should be more generic
    void addNumericFilter(const std::string name, int value);
    void setTextFieldName(const std::string& name);

    void drawShapeMercator(cv::Mat& src, float xStart, float yStart, float scale);
    void drawShapeEquidistant(cv::Mat& src, float xStart, float yStart, float centerLatitude, float centerLongitude, float scale);

  public: // setters
    void setThickness(int thickness) {
        mThicknes = thickness;
    }
    void setPointRadius(int radius) {
        mPointRadius = radius;
    }
    void setFontHeight(int height) {
        mFontHeight = height;
    }
    void setFontLineWidth(int width) {
        mFontLineWidth = width;
    }

  private:
    bool equidistantCheck(float latitude, float longitude, float centerLatitude, float centerLongitude);

  private:
    cv::Scalar mColor;
    int mEarthRadius;
    int mAltitude;
    std::map<std::string, int> mfilter;
    std::string mTextFieldName;
    int mThicknes;
    int mPointRadius;
    int mFontHeight;
    int mFontLineWidth;
};

} // namespace GIS

#endif // SHAPERENDERER_H
