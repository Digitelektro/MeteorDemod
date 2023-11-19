#include "blendimages.h"

cv::Mat BlendImages::merge(std::list<cv::Mat>& images) {
    std::list<cv::Mat>::iterator it = images.begin();

    cv::Mat composite = it->clone();
    ++it;

    composite.convertTo(composite, CV_32FC3);

    for(; it != images.end(); ++it) {
        it->convertTo(*it, CV_32FC3);

        cv::Mat grayScale1;
        cv::Mat alpha1;
        cv::medianBlur(composite, grayScale1, 5);
        cv::cvtColor(grayScale1, grayScale1, cv::COLOR_BGR2GRAY);

        cv::threshold(grayScale1, alpha1, 0, 255, cv::THRESH_BINARY);
        grayScale1.release();

        cv::Mat grayScale2;
        cv::Mat alpha2;
        cv::medianBlur(*it, grayScale2, 5);
        cv::cvtColor(grayScale2, grayScale2, cv::COLOR_BGR2GRAY);

        cv::threshold(grayScale2, alpha2, 0, 255, cv::THRESH_BINARY);
        grayScale2.release();

        cv::Mat mask;
        cv::bitwise_and(alpha1, alpha2, mask);
        alpha1.release();
        alpha2.release();

        std::vector<cv::Mat> channels;
        channels.push_back(mask);
        channels.push_back(mask);
        channels.push_back(mask);
        cv::merge(channels, mask);

        mask.convertTo(mask, CV_32FC3, 1 / 255.0);

        int start0 = findImageStart(composite);
        int start1 = findImageStart(*it);
        bool leftToRight = start0 < start1;

        cv::Mat blendmask = blendMask(mask, leftToRight);
        cv::multiply(cv::Scalar::all(1.0) - blendmask, composite, composite);
        blendmask = blendMask(mask, !leftToRight);
        cv::multiply(cv::Scalar::all(1.0) - blendmask, *it, *it);

        cv::add(composite, *it, composite);
    }

    return composite;
}

int BlendImages::findImageStart(const cv::Mat& img) {
    int i = img.size().width;
    for(int y = 0; y < img.size().height; ++y) {
        for(int x = 0; x < img.size().width; x++) {
            if(img.at<cv::Vec3f>(y, x) != cv::Vec3f(0, 0, 0)) {
                if(x < i) {
                    i = x;
                }
            }
        }
    }
    return i;
}

cv::Mat BlendImages::blendMask(const cv::Mat& mask, bool leftToRight) {
    int xStart = 0;
    int xEnd = 0;
    int blendWidth = 0;
    float alpha;

    cv::Mat blendedMask = cv::Mat::zeros(mask.size().height, mask.size().width, mask.type());

    for(int y = 0; y < mask.size().height; ++y) {
        bool foundStart = false;
        bool foundEnd = false;
        for(int x = 0; x < mask.size().width; x++) {
            if(!foundStart && mask.at<cv::Vec3f>(y, x) != cv::Vec3f(0, 0, 0)) {
                foundStart = true;
                xStart = x;

                for(int temp = x; temp < mask.size().width; temp++) {
                    if(mask.at<cv::Vec3f>(y, temp) != cv::Vec3f(1, 1, 1)) {
                        xEnd = temp;
                        foundEnd = true;
                        blendWidth = xEnd - xStart;
                        break;
                    }
                }
            }

            if(foundStart && foundEnd && (x >= xStart && x <= xEnd)) {
                alpha = static_cast<float>(x - xStart) / blendWidth;
                alpha = leftToRight ? alpha : 1.0f - alpha;

                blendedMask.at<cv::Vec3f>(y, x) = mask.at<cv::Vec3f>(y, x) * alpha;
            } else if(foundStart && foundEnd && (x > xEnd)) {
                foundStart = false;
                foundEnd = false;
            }
        }
    }
    return blendedMask;
}