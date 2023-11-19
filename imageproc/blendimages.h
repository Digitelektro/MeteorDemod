#pragma once

#include <list>
#include <opencv2/imgproc.hpp>

class BlendImages {
  public:
    static cv::Mat merge(std::list<cv::Mat>& images);

  private:
    static int findImageStart(const cv::Mat& img);
    static cv::Mat blendMask(const cv::Mat& mask, bool leftToRight);
};