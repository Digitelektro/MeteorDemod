#ifndef SETTINGS_H
#define SETTINGS_H

#include <map>
#include <list>

#include "DateTime.h"
#include "iniparser.h"

class Settings
{
private:
    struct SettingsData {
        SettingsData(const std::string arg, const std::string argShort, const std::string help)
            : argName(arg)
            , argNameShort(argShort)
            , helpText(help) {

        }
        std::string argName;
        std::string argNameShort;
        std::string helpText;
    };

public:
    struct HTMLColor {

        HTMLColor()
            : R(0)
            , G(0)
            , B(0)
        {

        }

        HTMLColor(int32_t rgb)
            : R(0)
            , G(0)
            , B(0)
        {
            R = static_cast<uint8_t>((rgb & 0xFF0000) >> 16);
            G = static_cast<uint8_t>((rgb & 0xFF00) >> 8);
            B = static_cast<uint8_t>(rgb & 0xFF);
        }

        HTMLColor(const std::string &hex)
            : R(0)
            , G(0)
            , B(0)
        {
            int32_t rgb = 0;

            try {
                rgb = std::stoi(hex.substr(1, hex.length()-1) , nullptr, 16);
            } catch (...) {
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

    friend std::istream &operator>>(std::istream& is, HTMLColor &color) {
        std::string rgb;
        is >> rgb;
        color = HTMLColor(rgb);
        return is;
    }


public:
    static Settings &getInstance();

private:
    Settings();
    Settings(Settings const&);
    void operator=(Settings const&);

public:
    void parseArgs(int argc, char **argv);
    void parseIni(const std::string &path);
    std::string getHelp() const;

public: //getters
    std::string getInputFilePath() const;
    std::string getTlePath() const;

    std::string getResourcesPath() const;
    std::string getOutputPath() const;
    std::string getOutputFormat() const;
    DateTime getPassDate() const;

    int getJpegQuality() const { return mJpegQuality; }
    float getM2Alfa() const { return mAlfaM2; }
    float getM2Delta() const { return DeltaM2; }
    bool EquadistantProjection() const { return mEquidistantProjection; }
    bool MercatorProjection() const { return mMercatorProjection; }

    bool FillBackLines() const { return mFillBackLines; }

    const std::string &getWaterMarkPlace() const { return mWaterMarkPlace; }
    const HTMLColor &getWaterMarkColor() const { return mWaterMarkColor; }
    int getWaterMarkSize() const { return mWaterMarkSize; }
    const std::string &getWaterMarkText() const { return mWaterMarkText; }

    bool drawReceiver() const { return mDrawreceiver; }
    float getReceiveLatitude() const { return mReceiveLatitude; }
    float getReceiveLongitude() const { return mReceiveLongitude; }
    const HTMLColor &getReceiveColor() const { return mReceiveColor; }
    int getReceiveSize() const { return mReceiveSize; }
    int getReceiveThickness() const { return mReceiveThickness; }
    const std::string &getReceiveMarkType() const { return mReceiveMarkType; }

    const std::string &getShapeGraticulesFile() const { return mShapeGraticulesFile; }
    const HTMLColor &getShapeGraticulesColor() const { return mShapeGraticulesColor; }
    int getShapeGraticulesThickness() const { return mShapeGraticulesThickness; }

    const std::string &getShapeCoastLinesFile() const { return mShapeCoastLinesFile; }
    const HTMLColor &getShapeCoastLinesColor() const { return mShapeCoastLinesColor; }
    int getShapeCoastLinesThickness() const { return mShapeCoastLinesThickness; }

    const std::string &getShapeBoundaryLinesFile() const { return mShapeBoundaryLinesFile; }
    const HTMLColor &getShapeBoundaryLinesColor() const { return mShapeBoundaryLinesColor; }
    int getShapeBoundaryLinesThickness() const { return mShapeBoundaryLinesThickness; }

    const std::string &getShapePopulatedPlacesFile() const { return mShapePopulatedPlacesFile; }
    const HTMLColor &getShapePopulatedPlacesColor() const { return mShapePopulatedPlacesColor; }
    int getShapePopulatedPlacesThickness() const { return mShapePopulatedPlacesThickness; }
    const std::string &getShapePopulatedPlacesFilterColumnName() const { return mShapePopulatedPlacesFilterColumnName; }
    int getShapePopulatedPlacesNumbericFilter() const { return mShapePopulatedPlacesNumbericFilter; }
    const std::string &getShapePopulatedPlacesTextColumnName() const { return mShapePopulatedPlacesTextColumnName; }

private:
    std::map<std::string, std::string> mArgs;
    std::list<SettingsData> mSettingsList;
    ini::IniParser<char> mIniParser;

    //ini section: Program
    int mJpegQuality;
    float mAlfaM2;
    float DeltaM2;
    bool mEquidistantProjection;
    bool mMercatorProjection;

    //ini section: Treatment
    bool mFillBackLines;

    //ini section: watermark
    std::string mWaterMarkPlace;
    HTMLColor mWaterMarkColor;
    int mWaterMarkSize;
    std::string mWaterMarkText;

    //ini section: ReceiverLocation
    bool mDrawreceiver;
    float mReceiveLatitude;
    float mReceiveLongitude;
    HTMLColor mReceiveColor;
    int mReceiveSize;
    int mReceiveThickness;
    std::string mReceiveMarkType;

    //ini section: ShapeFileGraticules
    std::string mShapeGraticulesFile;
    HTMLColor mShapeGraticulesColor;
    int mShapeGraticulesThickness;

    //ini section: ShapeFileCoastLines
    std::string mShapeCoastLinesFile;
    HTMLColor mShapeCoastLinesColor;
    int mShapeCoastLinesThickness;

    //ini section: ShapeFileBoundaryLines
    std::string mShapeBoundaryLinesFile;
    HTMLColor mShapeBoundaryLinesColor;
    int mShapeBoundaryLinesThickness;

    //ini section: ShapeFilePopulatedPlaces
    std::string mShapePopulatedPlacesFile;
    HTMLColor mShapePopulatedPlacesColor;
    int mShapePopulatedPlacesThickness;
    std::string mShapePopulatedPlacesFilterColumnName;
    int mShapePopulatedPlacesNumbericFilter;
    std::string mShapePopulatedPlacesTextColumnName;

};

#endif // SETTINGS_H
