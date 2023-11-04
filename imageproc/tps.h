// This TPS implementation is the optimized version of the original OpenCV algorithm.

/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2009, Willow Garage Inc., all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#include <opencv2/shape/shape_transformer.hpp>

#include "opencl.h"

namespace cv {

class ThinPlateSplineShapeTransformerImpl CV_FINAL : public ThinPlateSplineShapeTransformer {
  private:
#ifdef OPENCL_FOUND
    class TransformCalculator : public OpenCL::Program {
      public:
        TransformCalculator(const std::string& kernelPath, std::shared_ptr<OpenCL::Context> context, const uint shapeRefRows, const uint tpsParamRows, const uint size)
            : Program(context, kernelPath, "apply")
            , mshapeRefRows(shapeRefRows)
            , mTpsParamRows(tpsParamRows)
            , mSize(size) {
            createBuffer<cv::Point2f>(CL_MEM_READ_ONLY, mshapeRefRows, 0);
            createBuffer<cv::Point2f>(CL_MEM_READ_ONLY, mTpsParamRows, 1);
            createBuffer<cv::Point2f>(CL_MEM_READ_ONLY, mSize, 2);
            createBuffer<cv::Point2f>(CL_MEM_WRITE_ONLY, mSize, 6);
        }

        inline void operator()(const cv::Point2f* shapeRef, const cv::Point2f* tpsParameters, const cv::Point2f* point, cv::Point2f* result) {
            enqueueWriteBuffer(shapeRef, mshapeRefRows, 0);
            enqueueWriteBuffer(tpsParameters, mTpsParamRows, 1);
            enqueueWriteBuffer(point, mSize, 2);
            setKernelArg(mshapeRefRows, 3);
            setKernelArg(mTpsParamRows, 4);
            setKernelArg(mSize, 5);
            size_t totalWorkgroupSize = getContext()->getWorkGroupSize() * getContext()->getMaxComputeUnits();
            execute(totalWorkgroupSize);
            enqueueReadBuffer(result, mSize, 6);
        }

      protected:
        uint mshapeRefRows;
        uint mTpsParamRows;
        uint mSize;
    };

#endif // OPENCL_FOUND

  public:
    ThinPlateSplineShapeTransformerImpl(const std::string& kernelPath, double _regularizationParameter = 0.0)
        : mKernelPath(kernelPath)
        , regularizationParameter(_regularizationParameter)
        , name_("ShapeTransformer.TPS")
        , tpsComputed(false)
        , transformCost(0) {}

    ~ThinPlateSplineShapeTransformerImpl() CV_OVERRIDE {}

    // the main operators
    virtual void estimateTransformation(InputArray transformingShape, InputArray targetShape, std::vector<DMatch>& matches) CV_OVERRIDE;
    virtual float applyTransformation(InputArray inPts, OutputArray output = noArray()) CV_OVERRIDE;
    virtual void warpImage(InputArray transformingImage, OutputArray output, int flags, int borderMode, const Scalar& borderValue) const CV_OVERRIDE;

    // Setters/Getters
    virtual void setRegularizationParameter(double _regularizationParameter) CV_OVERRIDE {
        regularizationParameter = _regularizationParameter;
    }
    virtual double getRegularizationParameter() const CV_OVERRIDE {
        return regularizationParameter;
    }

    // write/read
    virtual void write(FileStorage& fs) const CV_OVERRIDE {
        writeFormat(fs);
        fs << "name" << name_ << "regularization" << regularizationParameter;
    }

    virtual void read(const FileNode& fn) CV_OVERRIDE {
        CV_Assert((String)fn["name"] == name_);
        regularizationParameter = (int)fn["regularization"];
    }

  private:
    inline Point2f _applyTransformation(const Mat& shapeRef, const Point2f& point, const Mat& tpsParameters) const {
        float affinex = tpsParameters.at<float>(tpsParameters.rows - 3, 0) + tpsParameters.at<float>(tpsParameters.rows - 2, 0) * point.x + tpsParameters.at<float>(tpsParameters.rows - 1, 0) * point.y;
        float affiney = tpsParameters.at<float>(tpsParameters.rows - 3, 1) + tpsParameters.at<float>(tpsParameters.rows - 2, 1) * point.x + tpsParameters.at<float>(tpsParameters.rows - 1, 1) * point.y;
        float nonrigidx = 0;
        float nonrigidy = 0;
        for(int j = 0; j < shapeRef.rows; j++) {
            nonrigidx += tpsParameters.at<float>(j, 0) * distance(Point2f(shapeRef.at<float>(j, 0), shapeRef.at<float>(j, 1)), point);
            nonrigidy += tpsParameters.at<float>(j, 1) * distance(Point2f(shapeRef.at<float>(j, 0), shapeRef.at<float>(j, 1)), point);
        }
        return {affinex + nonrigidx, affiney + nonrigidy};
    }

    // featuring floating point bit level hacking, Remez algorithm
    //  x=m*2^p => ln(x)=ln(m)+ln(2)p
    static constexpr float ln(float x) {
        uint32_t bx = *(uint32_t*)(&x);
        const uint32_t ex = bx >> 23;
        int32_t t = (int32_t)ex - (int32_t)127;
        uint32_t s = (t < 0) ? (-t) : t;
        bx = 1065353216 | (bx & 8388607);
        x = *(float*)(&bx);
        return -1.7417939f + (2.8212026f + (-1.4699568f + (0.44717955f - 0.056570851f * x) * x) * x) * x + 0.6931471806f * t;
    }

    inline float distance(const Point2f& p, const Point2f& q) const {
        Point2f diff = p - q;
        float norma = diff.x * diff.x + diff.y * diff.y; // - 2*diff.x*diff.y;
        if(norma < 0) {
            norma = 0;
        }
        // else norma = std::sqrt(norma);
        norma = norma * ln(norma + FLT_EPSILON);
        return norma;
    }

  private:
    bool tpsComputed;
    double regularizationParameter;
    float transformCost;
    Mat tpsParameters;
    Mat shapeReference;
    std::string mKernelPath;

  protected:
    String name_;
};

inline Ptr<ThinPlateSplineShapeTransformer> createThinPlateSplineShapeTransformer2(const std::string& kernelPath, double regularizationParameter = 0) {
    return Ptr<ThinPlateSplineShapeTransformer>(new ThinPlateSplineShapeTransformerImpl(kernelPath, regularizationParameter));
}

} // namespace cv
