#include "shapereader.h"

#include <fstream>

namespace GIS {

ShapeReader::ShapeReader(const std::string &shapeFile)
    : mFilePath(shapeFile)
    , mpBinaryData(nullptr)
    , mLoaded(false)
    , mDbFileReader(shapeFile)
    , mHasDbFile(false)
{

}

ShapeReader::~ShapeReader()
{
    if(mpBinaryData) {
        std::ifstream *streamreader = dynamic_cast<std::ifstream*>(mpBinaryData);
        if(streamreader && streamreader->is_open()) {
            streamreader->close();
        }
        delete mpBinaryData;
    }
}

bool ShapeReader::load()
{
    bool success = true;

    if(mLoaded) {
        return success;
    }

    mpBinaryData = new std::ifstream(mFilePath, std::ifstream::binary);

    do {
        if(!mpBinaryData) {
            success = false;
            break;
        }

        DataBuffer headerBuffer(100);
        mpBinaryData->read(reinterpret_cast<char*>(headerBuffer.buffer()), headerBuffer.size());

        if(mpBinaryData->fail()) {
            success = false;
            break;
        }

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

        if(mDbFileReader.load() == true) {
            mHasDbFile = true;
        }

    } while (false);

    return success;
}

} //namespace GIS
