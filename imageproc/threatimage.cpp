#include "threatimage.h"
#include <iostream>

void ThreatImage::fillBlackLines(cv::Mat &bitmap, int minimumHeight, int maximumHeight)
{
    int start = 0;
    int end;
    bool found = false;

    for (int x = 0; x < bitmap.size().width; x += SCAN_WIDTH)
    {
        for (int y = 0; y < bitmap.size().height; y++)
        {
            const cv::Vec3b &pixel = bitmap.at<cv::Vec3b>(y, x + 4);

            if (!found && (pixel[0] == 0 || pixel[1] == 0 || pixel[2] == 0))   //Check 4th one, first column on image is black
            {
                found = true;
                start = y;
            }

            if (found && pixel[0] != 0 && pixel[1] != 0 && pixel[2] != 0)   //Check 4th one, first column on image is black
            {
                found = false;
                end = y;
                if ((end - start) >= minimumHeight && (end - start) <= maximumHeight)
                {
                    int blankHeight = end - start;
                    if (start - ((blankHeight/2) + 1) > 0 && bitmap.size().height > (end + (blankHeight/2 + 1)))
                    {
                        fill(bitmap, x, start, end);
                    }
                }
            }
        }
    }
}

cv::Mat ThreatImage::irToTemperature(const cv::Mat &irImage, const cv::Mat ref)
{
    if(ref.cols != 256) {
        return cv::Mat();
    }

    cv::Mat thermalImage = cv::Mat::zeros(irImage.size(), irImage.type());

    for (int x = 0; x < irImage.cols; x++) {
        for (int y = 0; y < irImage.rows; y++) {
            uint8_t temp = irImage.at<cv::Vec3b>(y, x)[0];
            thermalImage.at<cv::Vec3b>(y, x) = ref.at<cv::Vec3b>(0,temp);
        }
    }
    return thermalImage;
}

cv::Mat ThreatImage::gamma(const cv::Mat &image, double gamma)
{
    cv::Mat newImage = image.clone();
    cv::Mat lookUpTable(1, 256, CV_8U);
    uchar* p = lookUpTable.ptr();

       for( int i = 0; i < 256; ++i) {
           p[i] = cv::saturate_cast<uchar>(std::pow(i / 255.0, gamma) * 255.0);
       }

       LUT(image, lookUpTable, newImage);

       return  newImage;
}

bool ThreatImage::isNightPass(const cv::Mat &image)
{
    if(image.size().width > 0 && image.size().height > 0) {
        cv::Scalar result = cv::mean(image);

        if(result[0] < 20 && result[1] < 20 && result[2] < 20) {
            return true;
        }
    }
    return false;
}

void ThreatImage::fill(cv::Mat &image, int x, int start, int end)
{
    int blankHeight = end - start;

    for (int i = 0; i < SCAN_WIDTH; i++)
    {
        for (int y = start, z = 0; z < (blankHeight / 2) + 1; y++, z++)
        {
            const cv::Vec3b &color1 = image.at<cv::Vec3b>(start - z - 1, x + i);
            const cv::Vec3b &color2 = image.at<cv::Vec3b>(end + z + 1, x + i);

            float alpha = static_cast<float>(z) / blankHeight;
            image.at<cv::Vec3b>(y, x + i) = blend(color2, color1, alpha);
            image.at<cv::Vec3b>(end - z, x + i) = blend(color1, color2, alpha);
        }
    }
}

cv::Vec3b ThreatImage::blend(const cv::Vec3b &color, const cv::Vec3b &backColor, float amount)
{
    uint8_t r = static_cast<uint8_t>((color[2] * amount) + backColor[2] * (1 - amount));
    uint8_t g = static_cast<uint8_t>((color[1] * amount) + backColor[1] * (1 - amount));
    uint8_t b = static_cast<uint8_t>((color[0] * amount) + backColor[0] * (1 - amount));
    return cv::Vec3b(b, g, r);
}


