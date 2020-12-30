#include "dbfilereader.h"
#include <fstream>

namespace GIS {

DbFileReader::DbFileReader(const std::string &filePath)
    : mFilePath(filePath)
    , mpBinaryData(nullptr)
    , mIsLoaded(false)
    , mLargestRecordSize(0)
{

}

DbFileReader::~DbFileReader()
{
    if(mpBinaryData) {
        if(mpBinaryData && mpBinaryData->is_open()) {
            mpBinaryData->close();
        }
        delete mpBinaryData;
    }
}

bool DbFileReader::load()
{
    bool success = true;

    if(mIsLoaded) {
        return success;
    }

    size_t pos = mFilePath.find("shp");
    if(pos < mFilePath.length()) {
        mFilePath.replace(pos, 3, "dbf");
    }

    mpBinaryData = new std::ifstream(mFilePath, std::ifstream::binary);

    do {
        if(!mpBinaryData && mpBinaryData->is_open()) {
            success = false;
            break;
        }

        DataBuffer headerBuffer(Header::size());
        mpBinaryData->read(reinterpret_cast<char*>(headerBuffer.buffer()), headerBuffer.size());
        mFileHeader = Header(headerBuffer);

        Field field;
        DataBuffer fieldDescriptorBuffer(Field::size());
        do {
            mpBinaryData->read(reinterpret_cast<char*>(fieldDescriptorBuffer.buffer()), fieldDescriptorBuffer.size());

            if(mpBinaryData->fail()) {
                success = false;
                break;
            }

            //End of fields marker
            if(fieldDescriptorBuffer.constBuffer()[0] == 0x0D) {
                break;
            }

            mFields.emplace_back(fieldDescriptorBuffer);

            mLargestRecordSize = mFields.back().fieldLength > mLargestRecordSize ? mFields.back().fieldLength : mLargestRecordSize;

        } while(true);

        mRecordBuffer.resize(mLargestRecordSize);

        mIsLoaded = true;

    } while(false);

    return success;
}

void DbFileReader::test()
{
    std::cout << mFileHeader << std::endl;
    std::cout << std::endl;

    std::vector<GIS::DbFileReader::Field> fieldAttributes = getFieldAttributes();
    for(size_t i = 0; i < getRecordCount(); i++) {
        std::vector<std::string> values = getFieldValues(i);
        for(size_t n = 0; n < fieldAttributes.size(); n++) {
            if(fieldAttributes[n].fieldName == std::string("NAMEASCII"))
                std::cout << fieldAttributes[n].fieldName << ": " << values[n] << "\t";
            if(fieldAttributes[n].fieldName == std::string("POP_MAX"))
                std::cout << fieldAttributes[n].fieldName << ": " << values[n] << std::endl;
        }
    }
}

std::vector<std::string> DbFileReader::getFieldValues(uint32_t record) const
{
    std::vector<std::string> attributeValues;
    char isRecordDeleted;
    do {
        if(!mIsLoaded) {
            break;
        }

        if(!mpBinaryData && mpBinaryData->is_open()) {
            break;
        }

        if(mFileHeader.numberOfRecords <= record) {
            break;
        }

        mpBinaryData->seekg(mFileHeader.headerSize + record * mFileHeader.recordSize);

        if(mpBinaryData->fail()) {
            break;
        }

        mpBinaryData->read(&isRecordDeleted, 1);

        if(isRecordDeleted == '*') {
            std::cout << "Record " << record << " is deleted" << std::endl;
            break;
        }

        std::vector<Field>::const_iterator it = mFields.begin();
        for(it = mFields.begin(); it != mFields.end(); ++it) {

            mpBinaryData->read(mRecordBuffer.data(), it->fieldLength);

            attributeValues.emplace_back(std::string(mRecordBuffer.data(), it->fieldLength));
        }

    } while(false);

    return attributeValues;
}

DbFileReader::Header::Header(const DataBuffer &buffer)
{
    size_t index = 0;
    buffer.valueAtIndex(index, type, LittleEndian);
    buffer.valueAtIndex(index, lastUpdated, LittleEndian);
    buffer.valueAtIndex(index, numberOfRecords, LittleEndian);
    buffer.valueAtIndex(index, headerSize, LittleEndian);
    buffer.valueAtIndex(index, recordSize, LittleEndian);
    buffer.valueAtIndex(index, _reserved,  LittleEndian);
}

DbFileReader::Field::Field(const DataBuffer &buffer)
{
    size_t index = 0;
    buffer.valueAtIndex(index, fieldName, LittleEndian);
    buffer.valueAtIndex(index, fieldtype, LittleEndian);
    buffer.valueAtIndex(index, fieldAddress, LittleEndian);
    buffer.valueAtIndex(index, fieldLength, LittleEndian);
    buffer.valueAtIndex(index, fieldCount, LittleEndian);
    buffer.valueAtIndex(index, _reserved, LittleEndian);
    buffer.valueAtIndex(index, workAreaID, LittleEndian);
    buffer.valueAtIndex(index, _reserved2, LittleEndian);
    buffer.valueAtIndex(index, setFieldsFlag, LittleEndian);
    buffer.valueAtIndex(index, _reserved3, LittleEndian);
}

} //namespace
