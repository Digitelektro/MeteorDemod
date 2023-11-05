#include "shaperenderer.h"

#include <vector>


GIS::ShapeRenderer::ShapeRenderer(const std::string shapeFile, const cv::Scalar& color)
    : ShapeReader(shapeFile)
    , mColor(color)
    , mThicknes(5)
    , mPointRadius(10)
    , mFontHeight(40)
    , mFontLineWidth(2) {}

void GIS::ShapeRenderer::addNumericFilter(const std::string name, int value) {
    mfilter.insert(std::make_pair(name, value));
}

void GIS::ShapeRenderer::setTextFieldName(const std::string& name) {
    mTextFieldName = name;
}

void GIS::ShapeRenderer::drawShape(const cv::Mat& src, Transform_t transform) {
    if(!load()) {
        return;
    }

    if(getShapeType() == ShapeReader::ShapeType::stPolyline) {
        auto recordIterator = getRecordIterator();

        if(recordIterator) {
            for(recordIterator->begin(); *recordIterator != recordIterator->end(); ++(*recordIterator)) {
                auto polyLineIterator = getPolyLineIterator(*recordIterator);

                if(polyLineIterator) {
                    std::vector<cv::Point> polyLines;
                    for(polyLineIterator->begin(); *polyLineIterator != polyLineIterator->end(); ++(*polyLineIterator)) {
                        if(transform(polyLineIterator->point.y, polyLineIterator->point.x)) {
                            polyLines.push_back(cv::Point2d(polyLineIterator->point.y, polyLineIterator->point.x));
                        }
                    }

                    if(polyLines.size() > 1) {
                        cv::polylines(src, polyLines, false, mColor, mThicknes);
                    }
                }
            }
        }
    } else if(getShapeType() == ShapeReader::ShapeType::stPolygon) {
        auto recordIterator = getRecordIterator();

        if(recordIterator) {
            for(recordIterator->begin(); *recordIterator != recordIterator->end(); ++(*recordIterator)) {
                auto polyLineIterator = getPolyLineIterator(*recordIterator);

                if(polyLineIterator) {
                    bool isFirst = true;
                    ShapeReader::Point first;
                    std::vector<cv::Point> polyLines;
                    for(polyLineIterator->begin(); *polyLineIterator != polyLineIterator->end(); ++(*polyLineIterator)) {
                        if(!isFirst && (first == polyLineIterator->point)) {
                            if(polyLines.size() > 1) {
                                cv::polylines(src, polyLines, true, mColor, mThicknes);
                            }
                            isFirst = true;
                            polyLines.clear();
                            continue;
                        }
                        if(isFirst) {
                            first = polyLineIterator->point;
                            isFirst = false;
                        }
                        if(transform(polyLineIterator->point.y, polyLineIterator->point.x)) {
                            polyLines.push_back(cv::Point2d(polyLineIterator->point.y, polyLineIterator->point.x));
                        }
                    }
                }
            }
        }
    } else if(getShapeType() == ShapeReader::ShapeType::stPoint) {
        auto recordIterator = getRecordIterator();

        if(mfilter.size() == 0) {
            if(recordIterator) {
                for(recordIterator->begin(); *recordIterator != recordIterator->end(); ++(*recordIterator)) {
                    ShapeReader::Point point(*recordIterator);

                    if(transform(point.y, point.x)) {
                        cv::circle(src, cv::Point2d(point.y, point.x), mPointRadius, mColor, cv::FILLED);
                        cv::circle(src, cv::Point2d(point.y, point.x), mPointRadius, cv::Scalar(0, 0, 0), 1);
                    }
                }
            }
        } else {
            const DbFileReader& dbFilereader = getDbFilereader();
            const std::vector<DbFileReader::Field> fieldAttributes = dbFilereader.getFieldAttributes();

            if(recordIterator && hasDbFile()) {
                uint32_t i = 0;
                for(recordIterator->begin(); *recordIterator != recordIterator->end(); ++(*recordIterator), ++i) {
                    ShapeReader::Point point(*recordIterator);
                    std::vector<std::string> fieldValues = dbFilereader.getFieldValues(i);

                    if(!transform(point.y, point.x)) {
                        continue;
                    }

                    bool drawName = false;
                    size_t namePos = 0;

                    for(size_t n = 0; n < fieldAttributes.size(); n++) {
                        if(mfilter.count(fieldAttributes[n].fieldName) == 1) {
                            int population = 0;
                            try {
                                population = std::stoi(fieldValues[n]);
                            } catch(...) {
                                continue;
                            }

                            if(population >= mfilter[fieldAttributes[n].fieldName]) {
                                cv::circle(src, cv::Point2d(point.y, point.x), mPointRadius, mColor, cv::FILLED);
                                cv::circle(src, cv::Point2d(point.y, point.x), mPointRadius, cv::Scalar(0, 0, 0), 1);

                                drawName = true;
                            }
                        }

                        if(std::string(fieldAttributes[n].fieldName) == mTextFieldName) {
                            namePos = n;
                        }
                    }

                    if(drawName) {
                        double fontScale = cv::getFontScaleFromHeight(cv::FONT_ITALIC, mFontHeight, mFontLineWidth);
                        int baseLine;
                        cv::Size size = cv::getTextSize(fieldValues[namePos], cv::FONT_ITALIC, fontScale, mFontLineWidth, &baseLine);
                        cv::putText(src, fieldValues[namePos], cv::Point2d(point.y - (size.width / 2), point.x - size.height + baseLine), cv::FONT_ITALIC, fontScale, cv::Scalar(0, 0, 0), mFontLineWidth + 1, cv::LINE_AA);
                        cv::putText(src, fieldValues[namePos], cv::Point2d(point.y - (size.width / 2), point.x - size.height + baseLine), cv::FONT_ITALIC, fontScale, mColor, mFontLineWidth, cv::LINE_AA);
                    }
                }
            }
        }
    }
}