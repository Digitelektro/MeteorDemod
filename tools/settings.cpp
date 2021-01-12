#include "settings.h"
#include <sstream>
#include <fstream>
#include <ctime>
#include <regex>

#if defined(_MSC_VER)
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

Settings &Settings::getInstance()
{
    static Settings instance;
    return instance;
}

Settings::Settings()
{
    mSettingsList.push_back(SettingsData("--help",  "-h", "Print help"));
    mSettingsList.push_back(SettingsData("--tle",   "-t", "TLE file required for pass calculation"));
    mSettingsList.push_back(SettingsData("--input", "-i", "Input S file containing softbits"));
    mSettingsList.push_back(SettingsData("--output","-o", "Output folder where generated files will be placed"));
    mSettingsList.push_back(SettingsData("--date",  "-d", "Specify pass date, format should  be dd:mm:yyyy"));
    mSettingsList.push_back(SettingsData("--format",  "-f", "Output image format (bmp, jpg)"));
}

void Settings::parseArgs(int argc, char **argv)
{
    for(int i = 1; i < (argc-1); i+=2) {
        mArgs.insert(std::make_pair(argv[i], argv[i+1]));
    }
}

void Settings::parseIni(const std::string &path)
{
    std::ifstream ifStream(path);
    mIniParser.parse(ifStream);
    //mIniParser.generate(std::cout);

    ini::extract(mIniParser.sections["Program"]["AzimuthalEquidistantProjection"], mEquidistantProjection, true);
    ini::extract(mIniParser.sections["Program"]["MercatorProjection"], mMercatorProjection, true);
    ini::extract(mIniParser.sections["Program"]["JpgQuality"], mJpegQuality, 90);
    ini::extract(mIniParser.sections["Program"]["AlfaM2"], mAlfaM2, 110.8f);
    ini::extract(mIniParser.sections["Program"]["DeltaM2"], DeltaM2, 3.2f);

    ini::extract(mIniParser.sections["Treatment"]["FillBlackLines"], mFillBackLines, true);

    ini::extract(mIniParser.sections["Watermark"]["Place"], mWaterMarkPlace, std::string("top_center"));
    ini::extract(mIniParser.sections["Watermark"]["Color"], mWaterMarkColor, HTMLColor());
    ini::extract(mIniParser.sections["Watermark"]["Size"], mWaterMarkSize, 5);
    ini::extract(mIniParser.sections["Watermark"]["Text"], mWaterMarkText, std::string());

    ini::extract(mIniParser.sections["ReceiverLocation"]["Draw"], mDrawreceiver, false);
    ini::extract(mIniParser.sections["ReceiverLocation"]["Latitude"], mReceiveLatitude, 0.0f);
    ini::extract(mIniParser.sections["ReceiverLocation"]["Longitude"], mReceiveLongitude, 0.0f);
    ini::extract(mIniParser.sections["ReceiverLocation"]["Color"], mReceiveColor, HTMLColor());
    ini::extract(mIniParser.sections["ReceiverLocation"]["Size"], mReceiveSize, 5);
    ini::extract(mIniParser.sections["ReceiverLocation"]["Thickness"], mReceiveThickness, 5);
    ini::extract(mIniParser.sections["ReceiverLocation"]["MarkType"], mReceiveMarkType);

    ini::extract(mIniParser.sections["ShapeFileGraticules"]["FileName"], mShapeGraticulesFile);
    ini::extract(mIniParser.sections["ShapeFileGraticules"]["Color"], mShapeGraticulesColor, HTMLColor());
    ini::extract(mIniParser.sections["ShapeFileGraticules"]["Thickness"], mShapeGraticulesThickness, 5);

    ini::extract(mIniParser.sections["ShapeFileCoastLines"]["FileName"], mShapeCoastLinesFile);
    ini::extract(mIniParser.sections["ShapeFileCoastLines"]["Color"], mShapeCoastLinesColor, HTMLColor());
    ini::extract(mIniParser.sections["ShapeFileCoastLines"]["Thickness"], mShapeCoastLinesThickness, 5);

    ini::extract(mIniParser.sections["ShapeFileBoundaryLines"]["FileName"], mShapeBoundaryLinesFile);
    ini::extract(mIniParser.sections["ShapeFileBoundaryLines"]["Color"], mShapeBoundaryLinesColor, HTMLColor());
    ini::extract(mIniParser.sections["ShapeFileBoundaryLines"]["Thickness"], mShapeBoundaryLinesThickness, 5);

    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["FileName"], mShapePopulatedPlacesFile);
    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["Color"], mShapePopulatedPlacesColor, HTMLColor());
    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["Thickness"], mShapePopulatedPlacesThickness, 5);
    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["FilterColumnName"], mShapePopulatedPlacesFilterColumnName, std::string("ADM0CAP"));
    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["NumericFilter"], mShapePopulatedPlacesNumbericFilter, 1);
    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["TextColumnName"], mShapePopulatedPlacesTextColumnName, std::string("NAME"));
}

std::string Settings::getHelp() const
{
    std::list<SettingsData>::const_iterator it;
    std::stringstream ss;

    for(it = mSettingsList.begin(); it != mSettingsList.end(); ++it) {
        ss << (*it).argNameShort << "\t" << (*it).argName << "\t" << (*it).helpText << std::endl;
    }

    return ss.str();
}

std::string Settings::getInputFilePath() const
{
    if(mArgs.count("-i")) {
        return mArgs.at("-i");
    }
    if(mArgs.count("--input")) {
        return mArgs.at("--input");
    }
    return std::string();
}

std::string Settings::getTlePath() const
{
    if(mArgs.count("-t")) {
        return mArgs.at("-t");
    }
    if(mArgs.count("--tle")) {
        return mArgs.at("--tle");
    }
    return std::string();
}

std::string Settings::getResourcesPath() const
{
#if defined(_MSC_VER)
    CHAR path[MAX_PATH];

	GetModuleFileNameA(nullptr, path, MAX_PATH);
	PathRemoveFileSpecA(path);

    return std::string(path) + "\\resources\\";
#else
    struct passwd *pw = getpwuid(getuid());
    return std::string(pw->pw_dir) + "/meteordemod/";
#endif
}

std::string Settings::getOutputPath() const
{
    if(mArgs.count("-o")) {
        return mArgs.at("-o");
    }
    if(mArgs.count("--output")) {
        return mArgs.at("--output");
    }
    return std::string("./");
}

std::string Settings::getOutputFormat() const
{
    std::string imgFormat;

    imgFormat =  std::string("bmp");

    if(mArgs.count("-f")) {
        imgFormat = mArgs.at("-f");
    }
    if(mArgs.count("--format")) {
        imgFormat =  mArgs.at("--format");
    }

    //Todo: validate format

    return imgFormat;
}

DateTime Settings::getPassDate() const
{
    int year, month, day;
    std::string dateTimeStr;
    DateTime dateTime(0);

    if(mArgs.count("-d")) {
        dateTimeStr = mArgs.at("-d");
    }
    if(mArgs.count("--date")) {
        dateTimeStr =  mArgs.at("--date");
    }

    const time_t now = time(nullptr);
    tm today;
    #if defined(_MSC_VER)
        gmtime_s(&today, &now);
    #else
        gmtime_r(&now, &today);
    #endif

    dateTime.Initialise(1900 + today.tm_year, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min, today.tm_sec, 0);

    if(dateTimeStr.empty()) {
        return dateTime;
    }

    try {
        std::regex dateTimeRegex("\\d{2}-\\d{2}-\\d{4}");

        if(std::regex_search(dateTimeStr, dateTimeRegex)) {
            std::replace( dateTimeStr.begin(), dateTimeStr.end(), '-', ' ');
            std::istringstream( dateTimeStr ) >> day >> month >> year;
            dateTime.Initialise(year, month, day, today.tm_hour, today.tm_min, today.tm_sec, 0);
        } else {
            std::cout << "Invalid given Date format, using today's date" << std::endl;
        }
    } catch (...) {
        std::cout << "Extracting Date parameter is failed, regex might be not supported on yur system. GCC version >=4.9.2 is required. Using default Date" << std::endl;
    }

    return dateTime;
}
