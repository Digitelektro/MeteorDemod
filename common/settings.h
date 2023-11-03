#ifndef SETTINGS_H
#define SETTINGS_H

#include <list>
#include <map>

#include "DateTime.h"
#include "iniparser.h"

class Settings {
  private:
    struct SettingsData {
        SettingsData(const std::string arg, const std::string argShort, const std::string help)
            : argName(arg)
            , argNameShort(argShort)
            , helpText(help) {}
        std::string argName;
        std::string argNameShort;
        std::string helpText;
    };

  public:
    struct HTMLColor {

        HTMLColor()
            : R(0)
            , G(0)
            , B(0) {}

        HTMLColor(int32_t rgb)
            : R(0)
            , G(0)
            , B(0) {
            R = static_cast<uint8_t>((rgb & 0xFF0000) >> 16);
            G = static_cast<uint8_t>((rgb & 0xFF00) >> 8);
            B = static_cast<uint8_t>(rgb & 0xFF);
        }

        HTMLColor(const std::string& hex)
            : R(0)
            , G(0)
            , B(0) {
            int32_t rgb = 0;

            try {
                rgb = std::stoi(hex.substr(1, hex.length() - 1), nullptr, 16);
            } catch(...) {
                std::cout << "Unable to parse color code: " << hex << std::endl;
            }

            R = static_cast<uint8_t>((rgb & 0xFF0000) >> 16);
            G = static_cast<uint8_t>((rgb & 0xFF00) >> 8);
            B = static_cast<uint8_t>(rgb & 0xFF);
        }

        uint8_t R;
        uint8_t G;
        uint8_t B;
    };

    friend std::istream& operator>>(std::istream& is, HTMLColor& color) {
        std::string rgb;
        is >> rgb;
        if(!rgb.empty()) {
            color = HTMLColor(rgb);
        }
        return is;
    }


  public:
    static Settings& getInstance();

  private:
    Settings();
    Settings(Settings const&);
    void operator=(Settings const&);

  public:
    void parseArgs(int argc, char** argv);
    void parseIni(const std::string& path);
    std::string getHelp() const;

  public: // getters
    std::string getInputFilePath() const;
    std::string getTlePath() const;
    std::string getSateliteName() const;
    int getCompositeMaxAgeHours() const;

    std::string getResourcesPath() const;
    std::string getOutputPath() const;
    std::string getOutputFormat() const;
    DateTime getPassDate() const;
    float getSymbolRate() const;
    std::string getDemodulatorMode() const;
    bool differentialDecode() const;
    bool deInterleave() const;
    bool getBrokenModulation() const;

    bool showHelp() const {
        return mArgs.count("-h") > 0 || mArgs.count("--help") > 0;
    }

    int getJpegQuality() const {
        return mJpegQuality;
    }

    std::string getSatNameInTLE() {
        std::string name;
        ini::extract(mIniParser.sections[getSateliteName()]["SatNameInTLE"], name);
        return name;
    }
    float getScanAngle() {
        float angle;
        ini::extract(mIniParser.sections[getSateliteName()]["ScanAngle"], angle, 110.8f);
        return angle;
    }
    float getRoll() {
        float roll;
        ini::extract(mIniParser.sections[getSateliteName()]["Roll"], roll, 0.0f);
        return roll;
    }
    float getPitch() {
        float pitch;
        ini::extract(mIniParser.sections[getSateliteName()]["Pitch"], pitch, 0.0f);
        return pitch;
    }
    float getYaw() {
        float yaw;
        ini::extract(mIniParser.sections[getSateliteName()]["Yaw"], yaw, 0.0f);
        return yaw;
    }
    float getTimeOffsetMs() {
        float timeOffset;
        ini::extract(mIniParser.sections[getSateliteName()]["TimeOffset"], timeOffset, 0.0f);
        return timeOffset;
    }

    bool equadistantProjection() const {
        return mEquidistantProjection;
    }
    bool mercatorProjection() const {
        return mMercatorProjection;
    }
    bool spreadImage() const {
        return mSpreadImage;
    }
    bool addRainOverlay() const {
        return mAddRainOverlay;
    }
    float getNightPassTreshold() const {
        return mNightPassTreshold;
    }
    float getProjectionScale() const {
        return mProjectionScale;
    }
    float getCompositeProjectionScale() const {
        return mCompositeProjectionScale;
    }

    bool compositeEquadistantProjection() const {
        return mCompositeEquadistantProjection;
    }
    bool compositeMercatorProjection() const {
        return mCompositeMercatorProjection;
    }
    bool generateComposite321() const {
        return mGenerateComposite321;
    }
    bool generateComposite125() const {
        return mGenerateComposite125;
    }
    bool generateComposite221() const {
        return mGenerateComposite221;
    }
    bool generateComposite224() const {
        return mGenerateComposite224;
    }
    bool generateComposite68() const {
        return mGenerateComposite68;
    }
    bool generateCompositeThermal() const {
        return mGenerateCompositeThermal;
    }
    bool generateComposite68Rain() const {
        return mGenerateComposite68Rain;
    }

    int getCostasBandwidth() const {
        return mCostasBw;
    }
    int getRRCFilterOrder() const {
        return mRRCFilterOrder;
    }
    bool waitForlock() const {
        return mWaitForLock;
    }

    bool fillBackLines() const {
        return mFillBackLines;
    }

    const std::string& getWaterMarkPlace() const {
        return mWaterMarkPlace;
    }
    const HTMLColor& getWaterMarkColor() const {
        return mWaterMarkColor;
    }
    int getWaterMarkSize() const {
        return mWaterMarkSize;
    }
    int getWaterMarkLineWidth() const {
        return mWaterMarkLineWidth;
    }
    const std::string& getWaterMarkText() const {
        return mWaterMarkText;
    }

    bool drawReceiver() const {
        return mDrawreceiver;
    }
    float getReceiverLatitude() const {
        return mReceiverLatitude;
    }
    float getReceiverLongitude() const {
        return mReceiverLongitude;
    }
    const HTMLColor& getReceiverColor() const {
        return mReceiverColor;
    }
    int getReceiverSize() const {
        return mReceiverSize;
    }
    int getReceiverThickness() const {
        return mReceiverThickness;
    }
    const std::string& getReceiverMarkType() const {
        return mReceiverMarkType;
    }

    const std::string& getShapeGraticulesFile() const {
        return mShapeGraticulesFile;
    }
    const HTMLColor& getShapeGraticulesColor() const {
        return mShapeGraticulesColor;
    }
    int getShapeGraticulesThickness() const {
        return mShapeGraticulesThickness;
    }

    const std::string& getShapeCoastLinesFile() const {
        return mShapeCoastLinesFile;
    }
    const HTMLColor& getShapeCoastLinesColor() const {
        return mShapeCoastLinesColor;
    }
    int getShapeCoastLinesThickness() const {
        return mShapeCoastLinesThickness;
    }

    const std::string& getShapeBoundaryLinesFile() const {
        return mShapeBoundaryLinesFile;
    }
    const HTMLColor& getShapeBoundaryLinesColor() const {
        return mShapeBoundaryLinesColor;
    }
    int getShapeBoundaryLinesThickness() const {
        return mShapeBoundaryLinesThickness;
    }

    const std::string& getShapePopulatedPlacesFile() const {
        return mShapePopulatedPlacesFile;
    }
    const HTMLColor& getShapePopulatedPlacesColor() const {
        return mShapePopulatedPlacesColor;
    }
    int getShapePopulatedPlacesFontWidth() const {
        return mShapePopulatedPlacesFontWidth;
    }
    int getShapePopulatedPlacesFontSize() const {
        return mShapePopulatedPlacesFontSize;
    }
    int getShapePopulatedPlacesPointradius() const {
        return mShapePopulatedPlacesPointradius;
    }
    const std::string& getShapePopulatedPlacesFilterColumnName() const {
        return mShapePopulatedPlacesFilterColumnName;
    }
    int getShapePopulatedPlacesNumbericFilter() const {
        return mShapePopulatedPlacesNumbericFilter;
    }
    const std::string& getShapePopulatedPlacesTextColumnName() const {
        return mShapePopulatedPlacesTextColumnName;
    }
    void resetArgs() {
        mArgs.clear();
    }

  private:
    std::map<std::string, std::string> mArgs;
    std::list<SettingsData> mSettingsList;
    ini::IniParser<char> mIniParser;

    // ini section: Program
    int mJpegQuality;
    bool mEquidistantProjection;
    bool mMercatorProjection;
    bool mSpreadImage;
    bool mAddRainOverlay;
    float mNightPassTreshold;
    float mProjectionScale;
    float mCompositeProjectionScale;

    bool mCompositeEquadistantProjection;
    bool mCompositeMercatorProjection;
    bool mGenerateComposite321;
    bool mGenerateComposite125;
    bool mGenerateComposite221;
    bool mGenerateComposite224;
    bool mGenerateComposite68;
    bool mGenerateCompositeThermal;
    bool mGenerateComposite68Rain;

    // ini section: Demodulator
    int mCostasBw;
    int mRRCFilterOrder;
    bool mWaitForLock;

    // ini section: Treatment
    bool mFillBackLines;

    // ini section: watermark
    std::string mWaterMarkPlace;
    HTMLColor mWaterMarkColor;
    int mWaterMarkSize;
    int mWaterMarkLineWidth;
    std::string mWaterMarkText;

    // ini section: ReceiverLocation
    bool mDrawreceiver;
    float mReceiverLatitude;
    float mReceiverLongitude;
    HTMLColor mReceiverColor;
    int mReceiverSize;
    int mReceiverThickness;
    std::string mReceiverMarkType;

    // ini section: ShapeFileGraticules
    std::string mShapeGraticulesFile;
    HTMLColor mShapeGraticulesColor;
    int mShapeGraticulesThickness;

    // ini section: ShapeFileCoastLines
    std::string mShapeCoastLinesFile;
    HTMLColor mShapeCoastLinesColor;
    int mShapeCoastLinesThickness;

    // ini section: ShapeFileBoundaryLines
    std::string mShapeBoundaryLinesFile;
    HTMLColor mShapeBoundaryLinesColor;
    int mShapeBoundaryLinesThickness;

    // ini section: ShapeFilePopulatedPlaces
    std::string mShapePopulatedPlacesFile;
    HTMLColor mShapePopulatedPlacesColor;
    int mShapePopulatedPlacesFontWidth;
    int mShapePopulatedPlacesFontSize;
    int mShapePopulatedPlacesPointradius;
    std::string mShapePopulatedPlacesFilterColumnName;
    int mShapePopulatedPlacesNumbericFilter;
    std::string mShapePopulatedPlacesTextColumnName;
};

#endif // SETTINGS_H
