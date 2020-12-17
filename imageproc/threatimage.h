#ifndef THREATIMAGE_H
#define THREATIMAGE_H

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

class ThreatImage
{
public:
    static void fillBlackLines(cv::Mat &image, int minimumHeight, int maximumHeight);
    static cv::Mat irToTemperature(const cv::Mat &irImage, const cv::Mat ref);
    static cv::Mat gamma(const cv::Mat &image, double gamma);

    static bool isNightPass(const cv::Mat &image);

private:
    static void fill(cv::Mat &image, int x, int start, int end);
    static cv::Vec3b blend(const cv::Vec3b &color, const cv::Vec3b &backColor, float amount);

private:
    static constexpr int SCAN_WIDTH = 112;
};

#endif // THREATIMAGE_H
