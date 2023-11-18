#pragma once

#include <map>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
//#include <opencv2/shape/shape_transformer.hpp>
#include <tps.h>

#include "pixelgeolocationcalculator.h"

class ProjectImage {
  public:
    enum Projection { Equidistant, Mercator, Rectify };

  public:
    static std::list<ProjectImage> createCompositeProjector(Projection projection, const std::list<PixelGeolocationCalculator>& gcpCalclulators, float scale, int earthRadius = 6378, int altitude = 825);

  public:
    ProjectImage(Projection projection, const PixelGeolocationCalculator& geolocationCalculator, float scale, int earthRadius = 6378, int altitude = 825);

    void calculateTransformation(const cv::Size& imageSize);
    cv::Mat project(const cv::Mat& image);

  private:
    void calculateImageBoundaries();
    void rectify(const cv::Size& imageSize);
    void drawMapOverlay(cv::Mat& image);
    cv::MarkerTypes stringToMarkerType(const std::string& markerType);
    bool transform(double& x, double& y);
    bool equidistantCheck(float latitude, float longitude, float centerLatitude, float centerLongitude);

  protected:
    Projection mProjection;
    const PixelGeolocationCalculator& mGeolocationCalculator;
    float mScale;
    int mEarthRadius;
    int mAltitude; // Meteor MN2 orbit altitude
    float mTheta;
    int mInc;
    int mHalfChord;
    float mScanAngle;
    int mNewImageHalfWidth;
    int mOriginalImageHalfWidth;
    CoordGeodetic mCenterCoordinate;
    int mWidth;
    int mHeight;
    int mXStart;
    int mYStart;
    bool mBoundariesCalcNeeded = true;
    cv::Mat mMapX;
    cv::Mat mMapY;
    bool mFlip = false;

    cv::Ptr<cv::ThinPlateSplineShapeTransformer> mTransformer;

    static std::map<std::string, cv::MarkerTypes> MarkerLookup;

    static constexpr int SWATH = 2800; // Meteor M2M swath width
};
