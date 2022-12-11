#ifndef THREATIMAGE_H
#define THREATIMAGE_H

#include <map>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

class ThreatImage {
  private:
    enum WatermarkPosition { TOP_LEFT, TOP_CENTER, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_CENTER, BOTTOM_RIGHT };

  public:
    static void fillBlackLines(cv::Mat& image, int minimumHeight, int maximumHeight);
    static cv::Mat irToTemperature(const cv::Mat& irImage, const cv::Mat& ref);
    static cv::Mat irToRain(const cv::Mat& irImage, const cv::Mat& ref);
    static cv::Mat invertIR(const cv::Mat& image);
    static cv::Mat addRainOverlay(const cv::Mat& image, const cv::Mat& rain);
    static cv::Mat gamma(const cv::Mat& image, double gamma);
    static cv::Mat sharpen(const cv::Mat& image);

    // Constrast 0.0-inf, brightnes -225 to 255
    static cv::Mat contrast(const cv::Mat& image, double contrast, double brightness);
    static void drawWatermark(cv::Mat image, const std::string& date);
    static bool isNightPass(const cv::Mat& image, float treshold);

  private:
    static void fill(cv::Mat& image, int x, int start, int end);
    static cv::Vec3b blend(const cv::Vec3b& color, const cv::Vec3b& backColor, float amount);
    static void replaceAll(std::string& str, const std::string& from, const std::string& to);

  private:
    static std::map<std::string, WatermarkPosition> WatermarkPositionLookup;

    static constexpr int SCAN_WIDTH = 112;
};

#endif // THREATIMAGE_H
