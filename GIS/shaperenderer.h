#ifndef SHAPERENDERER_H
#define SHAPERENDERER_H

#include <map>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "shapereader.h"

namespace GIS {

class ShapeRenderer : public ShapeReader {
  public:
    typedef std::function<bool(double& x, double& y)> Transform_t;

  public:
    ShapeRenderer(const std::string shapeFile, const cv::Scalar& color);

    ShapeRenderer(const ShapeRenderer&) = delete;
    ShapeRenderer& operator=(const ShapeRenderer&) = delete;

    // Todo: these should be more generic
    void addNumericFilter(const std::string name, int value);
    void setTextFieldName(const std::string& name);

    void drawShape(const cv::Mat& src, Transform_t transform);

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
    cv::Scalar mColor;

    std::map<std::string, int> mfilter;
    std::string mTextFieldName;
    int mThicknes;
    int mPointRadius;
    int mFontHeight;
    int mFontLineWidth;
};

} // namespace GIS

#endif // SHAPERENDERER_H
