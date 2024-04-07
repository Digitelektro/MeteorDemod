#include "tps.h"

#include <omp.h>

#include <future>
#include <iostream>
#include <list>

namespace cv {

void ThinPlateSplineShapeTransformerImpl::warpImage(InputArray transformingImage, OutputArray output, int flags, int borderMode, const Scalar& borderValue) const {

    CV_Assert(tpsComputed == true);

    Mat theinput = transformingImage.getMat();
    Mat mapX(theinput.rows, theinput.cols, CV_32FC1);
    Mat mapY(theinput.rows, theinput.cols, CV_32FC1);


    for(int row = 0; row < theinput.rows; row++) {
        for(int col = 0; col < theinput.cols; col++) {
            Point2f pt = _applyTransformation(shapeReference, Point2f(float(col), float(row)), tpsParameters);
            mapX.at<float>(row, col) = pt.x;
            mapY.at<float>(row, col) = pt.y;
        }
    }
    remap(transformingImage, output, mapX, mapY, flags, borderMode, borderValue);
}

float ThinPlateSplineShapeTransformerImpl::applyTransformation(InputArray inPts, OutputArray outPts) {

    CV_Assert(tpsComputed);
    Mat pts1 = inPts.getMat();
    CV_Assert((pts1.channels() == 2) && (pts1.cols > 0));

    // Apply transformation in the complete set of points
    //  Ensambling output //
    if(outPts.needed()) {
        outPts.create(1, pts1.cols, CV_32FC2);
        Mat outMat = outPts.getMat();
        bool openclSuccess = false;

#ifdef OPENCL_FOUND
        // For small calculations not worth to use opencl
        if(pts1.cols > 100) {
            try {
                auto devices = OpenCL::Context::getDevices(CL_DEVICE_TYPE_GPU);
                if(devices.size() == 0) {
                    throw std::runtime_error("OpenCL GPU Device not found");
                }
                std::shared_ptr<OpenCL::Context> opencl = std::make_shared<OpenCL::Context>(devices[0]);
                opencl->init();
                TransformCalculator transform(mKernelPath, opencl, shapeReference.rows, tpsParameters.rows, pts1.cols);
                transform(reinterpret_cast<cv::Point2f*>(shapeReference.data), reinterpret_cast<cv::Point2f*>(tpsParameters.data), reinterpret_cast<cv::Point2f*>(pts1.data), reinterpret_cast<cv::Point2f*>(outMat.data));
                openclSuccess = true;
            } catch(const std::exception& ex) {
                std::cout << "Failed to run Opencl, error: " << ex.what() << std::endl;
            }
        }

#endif // OPENCL_FOUND

        /*if(openclSuccess == false) {
            for(int i = 0; i < pts1.cols; i++) {
                outMat.at<Point2f>(0, i) = _applyTransformation(shapeReference, pts1.at<Point2f>(0, i), tpsParameters);
            }
        }*/

        if(openclSuccess == false) {
            if(pts1.cols > 100) {
                std::list<std::future<void>> futures;
                auto numberofCores = std::thread::hardware_concurrency();
                auto itemPerTask = (pts1.cols + numberofCores - 1) / numberofCores;
                std::cout << "numberofCores=" << numberofCores << " cols=" << pts1.cols << std::endl;
                for(auto i = 0; i < numberofCores; i++) {
                    auto start = i * itemPerTask;
                    auto end = (i + 1) * itemPerTask;
                    if(end > pts1.cols) {
                        end = pts1.cols;
                    };
                    futures.emplace_back(std::async(std::launch::async, [this, &pts1, &outMat, start, end]() {
                        std::cout << "Start=" << start << " end=" << end << std::endl;
                        for(int i = start; i < end; i++) {
                            outMat.at<Point2f>(0, i) = _applyTransformation(shapeReference, pts1.at<Point2f>(0, i), tpsParameters);
                        }
                    }));
                }
            } else {
                for(int i = 0; i < pts1.cols; i++) {
                    outMat.at<Point2f>(0, i) = _applyTransformation(shapeReference, pts1.at<Point2f>(0, i), tpsParameters);
                }
            }
        }
    }

    return transformCost;
}

void ThinPlateSplineShapeTransformerImpl::estimateTransformation(InputArray _pts1, InputArray _pts2, std::vector<DMatch>& _matches) {
    Mat pts1 = _pts1.getMat();
    Mat pts2 = _pts2.getMat();
    CV_Assert((pts1.channels() == 2) && (pts1.cols > 0) && (pts2.channels() == 2) && (pts2.cols > 0));
    CV_Assert(_matches.size() > 1);

    if(pts1.type() != CV_32F)
        pts1.convertTo(pts1, CV_32F);
    if(pts2.type() != CV_32F)
        pts2.convertTo(pts2, CV_32F);

    // Use only valid matchings //
    std::vector<DMatch> matches;
    for(size_t i = 0; i < _matches.size(); i++) {
        if(_matches[i].queryIdx < pts1.cols && _matches[i].trainIdx < pts2.cols) {
            matches.push_back(_matches[i]);
        }
    }

    // Organizing the correspondent points in matrix style //
    Mat shape1((int)matches.size(), 2, CV_32F); // transforming shape
    Mat shape2((int)matches.size(), 2, CV_32F); // target shape
    for(int i = 0, end = (int)matches.size(); i < end; i++) {
        Point2f pt1 = pts1.at<Point2f>(0, matches[i].queryIdx);
        shape1.at<float>(i, 0) = pt1.x;
        shape1.at<float>(i, 1) = pt1.y;

        Point2f pt2 = pts2.at<Point2f>(0, matches[i].trainIdx);
        shape2.at<float>(i, 0) = pt2.x;
        shape2.at<float>(i, 1) = pt2.y;
    }
    shape1.copyTo(shapeReference);

    // Building the matrices for solving the L*(w|a)=(v|0) problem with L={[K|P];[P'|0]}

    // Building K and P (Needed to build L)
    Mat matK((int)matches.size(), (int)matches.size(), CV_32F);
    Mat matP((int)matches.size(), 3, CV_32F);
    for(int i = 0, end = (int)matches.size(); i < end; i++) {
        for(int j = 0; j < end; j++) {
            if(i == j) {
                matK.at<float>(i, j) = float(regularizationParameter);
            } else {
                matK.at<float>(i, j) = distance(Point2f(shape1.at<float>(i, 0), shape1.at<float>(i, 1)), Point2f(shape1.at<float>(j, 0), shape1.at<float>(j, 1)));
            }
        }
        matP.at<float>(i, 0) = 1;
        matP.at<float>(i, 1) = shape1.at<float>(i, 0);
        matP.at<float>(i, 2) = shape1.at<float>(i, 1);
    }

    // Building L
    Mat matL = Mat::zeros((int)matches.size() + 3, (int)matches.size() + 3, CV_32F);
    Mat matLroi(matL, Rect(0, 0, (int)matches.size(), (int)matches.size())); // roi for K
    matK.copyTo(matLroi);
    matLroi = Mat(matL, Rect((int)matches.size(), 0, 3, (int)matches.size())); // roi for P
    matP.copyTo(matLroi);
    Mat matPt;
    transpose(matP, matPt);
    matLroi = Mat(matL, Rect(0, (int)matches.size(), (int)matches.size(), 3)); // roi for P'
    matPt.copyTo(matLroi);

    // Building B (v|0)
    Mat matB = Mat::zeros((int)matches.size() + 3, 2, CV_32F);
    for(int i = 0, end = (int)matches.size(); i < end; i++) {
        matB.at<float>(i, 0) = shape2.at<float>(i, 0); // x's
        matB.at<float>(i, 1) = shape2.at<float>(i, 1); // y's
    }

    // Obtaining transformation params (w|a)
    solve(matL, matB, tpsParameters, DECOMP_LU);
    // tpsParameters = matL.inv()*matB;

    // Setting transform Cost and Shape reference
    Mat w(tpsParameters, Rect(0, 0, 2, tpsParameters.rows - 3));
    Mat Q = w.t() * matK * w;
    transformCost = fabs(Q.at<float>(0, 0) * Q.at<float>(1, 1)); // fabs(mean(Q.diag(0))[0]);//std::max(Q.at<float>(0,0),Q.at<float>(1,1));
    tpsComputed = true;
}

} // namespace cv