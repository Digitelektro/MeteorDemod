#include "settings.h"

#include <ctime>
#include <fstream>
#include <regex>
#include <sstream>

#include "version.h"

#if defined(_MSC_VER)
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

Settings& Settings::getInstance() {
    static Settings instance;
    return instance;
}

Settings::Settings() {
    mSettingsList.push_back(SettingsData("--help", "-h", "Print help"));
    mSettingsList.push_back(SettingsData("--tle", "-t", "TLE file required for pass calculation"));
    mSettingsList.push_back(SettingsData("--input", "-i", "Input S file containing softbits"));
    mSettingsList.push_back(SettingsData("--output", "-o", "Output folder where generated files will be placed"));
    mSettingsList.push_back(SettingsData("--date", "-d", "Specify pass date, format should be dd-mm-yyyy"));
    mSettingsList.push_back(SettingsData("--format", "-f", "Output image format (bmp, jpg)"));
    mSettingsList.push_back(SettingsData("--symbolrate", "-s", "Set symbol rate for demodulator"));
    mSettingsList.push_back(SettingsData("--mode", "-m", "Set demodulator mode to qpsk or oqpsk"));
    mSettingsList.push_back(SettingsData("--diff", "-diff", "Use differential decoding (Maybe required for newer satellites)"));
    mSettingsList.push_back(SettingsData("--int", "-int", "Deinterleave (Maybe required for newer satellites)"));
    mSettingsList.push_back(SettingsData("--brokenM2", "-b", "Broken M2 modulation"));
    mSettingsList.push_back(SettingsData("--compmaxage", "-c", "Maximum image age in hours for creating composite image"));
    mSettingsList.push_back(SettingsData("--satellite", "-sat", "Name of the satellite settings in settings.ini file"));
}

void Settings::parseArgs(int argc, char** argv) {
    if(argc == 1) {
        throw std::runtime_error("Invalid number or arguments, at least one argument shall be provided!");
    } else {
        for(int i = 1; i < argc; i++) {
            if(i + 1 < argc) {
                if(*argv[i + 1] == '-') {
                    mArgs.insert(std::make_pair(argv[i], "true"));
                } else {
                    mArgs.insert(std::make_pair(argv[i], argv[i + 1]));
                    i++;
                }
            } else {
                if(*argv[i] == '-') {
                    mArgs.insert(std::make_pair(argv[i], "true"));
                } else {
                    mArgs.insert(std::make_pair("-o", argv[i]));
                }
            }
        }
    }
}

void Settings::parseIni(const std::string& path) {
    std::ifstream ifStream(path);
    if(!ifStream.is_open()) {
        std::cout << "Unable to open settings.ini at: '" << path << "' Program will use default settings." << std::endl;
    } else {
        mIniParser.parse(ifStream);
    }
    // mIniParser.generate(std::cout);

    ini::extract(mIniParser.sections["Program"]["AzimuthalEquidistantProjection"], mEquidistantProjection, true);
    ini::extract(mIniParser.sections["Program"]["MercatorProjection"], mMercatorProjection, true);
    ini::extract(mIniParser.sections["Program"]["SpreadImage"], mSpreadImage, true);
    ini::extract(mIniParser.sections["Program"]["AddRainOverlay"], mAddRainOverlay, true);
    ini::extract(mIniParser.sections["Program"]["JpgQuality"], mJpegQuality, 90);
    ini::extract(mIniParser.sections["Program"]["NightPassTreshold"], mNightPassTreshold, 10.0f);
    ini::extract(mIniParser.sections["Program"]["ProjectionScale"], mProjectionScale, 0.75f);
    ini::extract(mIniParser.sections["Program"]["CompositeProjectionScale"], mCompositeProjectionScale, 0.75f);
    ini::extract(mIniParser.sections["Program"]["CompositeAzimuthalEquidistantProjection"], mCompositeEquadistantProjection, true);
    ini::extract(mIniParser.sections["Program"]["CompositeMercatorProjection"], mCompositeMercatorProjection, false);
    ini::extract(mIniParser.sections["Program"]["GenerateComposite123"], mGenerateComposite123, true);
    ini::extract(mIniParser.sections["Program"]["GenerateComposite125"], mGenerateComposite125, true);
    ini::extract(mIniParser.sections["Program"]["GenerateComposite221"], mGenerateComposite221, true);
    ini::extract(mIniParser.sections["Program"]["GenerateComposite68"], mGenerateComposite68, true);
    ini::extract(mIniParser.sections["Program"]["GenerateCompositeThermal"], mGenerateCompositeThermal, true);
    ini::extract(mIniParser.sections["Program"]["GenerateComposite68Rain"], mGenerateComposite68Rain, true);

    ini::extract(mIniParser.sections["Demodulator"]["CostasBandwidth"], mCostasBw, 50);
    ini::extract(mIniParser.sections["Demodulator"]["RRCFilterOrder"], mRRCFilterOrder, 64);
    ini::extract(mIniParser.sections["Demodulator"]["WaitForLock"], mWaitForLock, true);

    ini::extract(mIniParser.sections["Treatment"]["FillBlackLines"], mFillBackLines, true);

    ini::extract(mIniParser.sections["Watermark"]["Place"], mWaterMarkPlace);
    ini::extract(mIniParser.sections["Watermark"]["Color"], mWaterMarkColor, HTMLColor(0xAD880C));
    ini::extract(mIniParser.sections["Watermark"]["Size"], mWaterMarkSize, 80);
    ini::extract(mIniParser.sections["Watermark"]["Width"], mWaterMarkLineWidth, 3);
    ini::extract(mIniParser.sections["Watermark"]["Text"], mWaterMarkText);

    ini::extract(mIniParser.sections["ReceiverLocation"]["Draw"], mDrawreceiver, false);
    ini::extract(mIniParser.sections["ReceiverLocation"]["Latitude"], mReceiverLatitude, 0.0f);
    ini::extract(mIniParser.sections["ReceiverLocation"]["Longitude"], mReceiverLongitude, 0.0f);
    ini::extract(mIniParser.sections["ReceiverLocation"]["Color"], mReceiverColor, HTMLColor(0xCC3030));
    ini::extract(mIniParser.sections["ReceiverLocation"]["Size"], mReceiverSize, 5);
    ini::extract(mIniParser.sections["ReceiverLocation"]["Thickness"], mReceiverThickness, 5);
    ini::extract(mIniParser.sections["ReceiverLocation"]["MarkType"], mReceiverMarkType);

    ini::extract(mIniParser.sections["ShapeFileGraticules"]["FileName"], mShapeGraticulesFile);
    ini::extract(mIniParser.sections["ShapeFileGraticules"]["Color"], mShapeGraticulesColor, HTMLColor(0xC8C8C8));
    ini::extract(mIniParser.sections["ShapeFileGraticules"]["Thickness"], mShapeGraticulesThickness, 1);

    ini::extract(mIniParser.sections["ShapeFileCoastLines"]["FileName"], mShapeCoastLinesFile);
    ini::extract(mIniParser.sections["ShapeFileCoastLines"]["Color"], mShapeCoastLinesColor, HTMLColor(0x808000));
    ini::extract(mIniParser.sections["ShapeFileCoastLines"]["Thickness"], mShapeCoastLinesThickness, 2);

    ini::extract(mIniParser.sections["ShapeFileBoundaryLines"]["FileName"], mShapeBoundaryLinesFile);
    ini::extract(mIniParser.sections["ShapeFileBoundaryLines"]["Color"], mShapeBoundaryLinesColor, HTMLColor(0xC8C8C8));
    ini::extract(mIniParser.sections["ShapeFileBoundaryLines"]["Thickness"], mShapeBoundaryLinesThickness, 2);

    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["FileName"], mShapePopulatedPlacesFile);
    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["Color"], mShapePopulatedPlacesColor, HTMLColor(0x5A42F5));
    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["Width"], mShapePopulatedPlacesFontWidth, 2);
    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["Size"], mShapePopulatedPlacesFontSize, 40);
    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["PointRadius"], mShapePopulatedPlacesPointradius, 10);
    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["FilterColumnName"], mShapePopulatedPlacesFilterColumnName, std::string("ADM0CAP"));
    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["NumericFilter"], mShapePopulatedPlacesNumbericFilter, 1);
    ini::extract(mIniParser.sections["ShapeFilePopulatedPlaces"]["TextColumnName"], mShapePopulatedPlacesTextColumnName, std::string("NAME"));
}

std::string Settings::getHelp() const {
    std::list<SettingsData>::const_iterator it;
    std::stringstream ss;

    ss << "MeteorDemod Version " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_FIX << std::endl;
    for(it = mSettingsList.begin(); it != mSettingsList.end(); ++it) {
        ss << (*it).argNameShort << "\t" << (*it).argName << "\t" << (*it).helpText << std::endl;
    }

    return ss.str();
}

std::string Settings::getInputFilePath() const {
    if(mArgs.count("-i")) {
        return mArgs.at("-i");
    }
    if(mArgs.count("--input")) {
        return mArgs.at("--input");
    }
    return std::string();
}

std::string Settings::getTlePath() const {
    if(mArgs.count("-t")) {
        return mArgs.at("-t");
    }
    if(mArgs.count("--tle")) {
        return mArgs.at("--tle");
    }
    return std::string();
}

std::string Settings::getSateliteName() const {
    if(mArgs.count("-sat")) {
        return mArgs.at("-sat");
    }
    if(mArgs.count("--satellite")) {
        return mArgs.at("--satellite");
    }
    return std::string();
}

int Settings::getCompositeMaxAgeHours() const {
    if(mArgs.count("-c")) {
        return atoi(mArgs.at("-c").c_str());
    }
    if(mArgs.count("--compmaxage")) {
        return atoi(mArgs.at("-compmaxage").c_str());
    }
    return 6; // default 6H
}

std::string Settings::getResourcesPath() const {
#if defined(_MSC_VER)
    CHAR path[MAX_PATH];

    GetModuleFileNameA(nullptr, path, MAX_PATH);
    PathRemoveFileSpecA(path);

    return std::string(path) + "\\resources\\";
#else
    struct passwd* pw = getpwuid(getuid());
    return std::string(pw->pw_dir) + "/.config/meteordemod/";
#endif
}

std::string Settings::getOutputPath() const {
    std::string path{"./"};

    if(mArgs.count("-o")) {
        path = mArgs.at("-o");
    }
    if(mArgs.count("--output")) {
        path = mArgs.at("--output");
    }

    if(path.back() != '/') {
        path += '/';
    }

    return path;
}

std::string Settings::getOutputFormat() const {
    std::string imgFormat;

    imgFormat = std::string("bmp");

    if(mArgs.count("-f")) {
        imgFormat = mArgs.at("-f");
    }
    if(mArgs.count("--format")) {
        imgFormat = mArgs.at("--format");
    }

    // Todo: validate format

    return imgFormat;
}

DateTime Settings::getPassDate() const {
    int year, month, day;
    std::string dateTimeStr;
    DateTime dateTime(0);

    if(mArgs.count("-d")) {
        dateTimeStr = mArgs.at("-d");
    }
    if(mArgs.count("--date")) {
        dateTimeStr = mArgs.at("--date");
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
            std::replace(dateTimeStr.begin(), dateTimeStr.end(), '-', ' ');
            std::istringstream(dateTimeStr) >> day >> month >> year;
            dateTime.Initialise(year, month, day, 12, 0, 0, 0);
        } else {
            std::cout << "Invalid given Date format, using today's date" << std::endl;
        }
    } catch(...) {
        std::cout << "Extracting date parameter failed, regex might not be supported on your system. GCC version >=4.9.2 is required. Using default Date" << std::endl;
    }

    return dateTime;
}

float Settings::getSymbolRate() const {
    float symbolRate = 72000.0f;

    if(mArgs.count("-s")) {
        symbolRate = atof(mArgs.at("-s").c_str());
    }
    if(mArgs.count("--symbolrate")) {
        symbolRate = atof(mArgs.at("--symbolrate").c_str());
    }

    return symbolRate;
}

std::string Settings::getDemodulatorMode() const {
    std::string modeStr = std::string("qpsk");

    if(mArgs.count("-m")) {
        modeStr = mArgs.at("-m");
    }
    if(mArgs.count("--mode")) {
        modeStr = mArgs.at("--mode");
    }

    return modeStr;
}

bool Settings::differentialDecode() const {
    bool result = false;

    if(mArgs.count("--diff")) {
        result = true;
    }

    return result;
}

bool Settings::deInterleave() const {
    bool result = false;

    if(mArgs.count("--int")) {
        result = true;
    }

    return result;
}

bool Settings::getBrokenModulation() const {
    bool result = false;

    if(mArgs.count("-b")) {
        result = true;
    }
    if(mArgs.count("--brokenM2")) {
        result = true;
    }

    return result;
}
