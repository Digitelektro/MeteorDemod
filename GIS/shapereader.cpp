#include "shapereader.h"

#include <fstream>

namespace GIS {

ShapeReader::ShapeReader(const std::string &shapeFile)
    : mFilePath(shapeFile)
    , mpBinaryData(nullptr)
    , mLoaded(false)
{

}

ShapeReader::~ShapeReader()
{
    if(mpBinaryData) {
        std::ifstream *streamreader = dynamic_cast<std::ifstream*>(mpBinaryData);
        if(streamreader) {
            streamreader->close();
        }
        delete mpBinaryData;
    }
}

void ShapeReader::load()
{
    if(mLoaded) {
        return;
    }

    mpBinaryData = new std::ifstream(mFilePath, std::ifstream::binary);

    do {
        if(!mpBinaryData) {
            break;
        }

        DataBuffer headerBuffer(100);
        mpBinaryData->read(reinterpret_cast<char*>(headerBuffer.buffer()), headerBuffer.size());
        mShapeHeader = ShapeHeader(headerBuffer);

        mLoaded = true;

        RecordIterator it(*mpBinaryData);

        //Test
        /*for(it.begin(); it != it.end(); ++it) {
            std::cout << it.recordHeader.recordNumber << std::endl;

            if(it.recordHeader.shapeType == stPolyline) {
                PolyLineIterator iterator(*mpBinaryData, it.mRecordPosition);

                for(iterator.begin(); iterator != iterator.end(); ++iterator) {
                    std::cout << iterator.point.x << " " << iterator.point.y << std::endl;
                }
            } else if(it.recordHeader.shapeType == stMultiPoint) {
                MultiPointIterator iterator(*mpBinaryData, it.mRecordPosition);

                for(iterator.begin(); iterator != iterator.end(); ++iterator) {
                    std::cout << iterator.point.x << " " << iterator.point.y << std::endl;
                }
            } else if(it.recordHeader.shapeType == stPoint) {
                Point pt(*mpBinaryData, it.mRecordPosition);
                std::cout << pt.x << " " << pt.y << std::endl;
            }

        }*/

    } while (false);

}

} //namespace GIS
