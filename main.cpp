#include <chrono>
#include <ctime>
#include <exception>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
#include <tuple>

#include "DSP/meteordemodulator.h"
#include "DSP/wavreader.h"
#include "GIS/shapereader.h"
#include "GIS/shaperenderer.h"
#include "meteordecoder.h"
#include "pixelgeolocationcalculator.h"
#include "settings.h"
#include "spreadimage.h"
#include "threadpool.h"
#include "tlereader.h"


namespace fs = std::experimental::filesystem;

struct ImageForSpread {
    ImageForSpread(cv::Mat img, std::string fileNamebase)
        : image(img)
        , fileNameBase(fileNamebase) {}

    cv::Mat image;
    std::string fileNameBase;
};

void searchForImages(std::list<cv::Mat>& imagesOut, std::list<PixelGeolocationCalculator>& geolocationCalculatorsOut, const std::string& channelName);
void saveImage(const std::string fileName, const cv::Mat& image);
void writeSymbolToFile(std::ostream& stream, const Wavreader::complex& sample);

static std::mutex saveImageMutex;
static Settings& mSettings = Settings::getInstance();
static ThreadPool mThreadPool(std::thread::hardware_concurrency());

int main(int argc, char* argv[]) {
    mSettings.parseArgs(argc, argv);
    mSettings.parseIni(mSettings.getResourcesPath() + "settings.ini");

    if(mSettings.showHelp()) {
        std::cout << mSettings.getHelp() << std::endl;
        return 0;
    }

    if(mSettings.getSateliteName() == "") {
        std::cout << mSettings.getHelp() << std::endl;
        throw std::runtime_error("Satellite name is not given in command line arguments!");
    }

    mThreadPool.start();

    MeteorDecoder meteorDecoder(mSettings.deInterleave(), mSettings.getDemodulatorMode() == "oqpsk", mSettings.differentialDecode());

    size_t decodedPacketCounter = 0;
    std::string inputPath = mSettings.getInputFilePath();
    try {
        if(inputPath.substr(inputPath.find_last_of(".") + 1) == "wav") {
            std::cout << "Input is a .wav file, processing it..." << std::endl;

            const std::string outputPath = inputPath.substr(0, inputPath.find_last_of(".") + 1) + "s";
            std::ofstream outputStream;
            outputStream.open(outputPath, std::ios::binary);

            if(!outputStream.is_open()) {
                throw std::runtime_error("Creating output .S file failed, demodulating aborted");
            }

            Wavreader wavReader;
            if(!wavReader.openFile(inputPath)) {
                throw std::runtime_error("Opening .wav file failed, demodulating aborted");
            }

            DSP::MeteorCostas::Mode mode = DSP::MeteorCostas::QPSK;
            if(mSettings.getDemodulatorMode() == "oqpsk") {
                mode = DSP::MeteorCostas::OQPSK;
            }


            DSP::MeteorDemodulator demodulator(mode, mSettings.getSymbolRate(), mSettings.getCostasBandwidth(), mSettings.getRRCFilterOrder(), mSettings.waitForlock(), mSettings.getBrokenModulation());
            demodulator.process(wavReader, [&outputStream](const Wavreader::complex& sample, float) {
                writeSymbolToFile(outputStream, sample);
            });

            outputStream.flush();
            outputStream.close();
            inputPath = outputPath;
        }

        std::ifstream binaryData(inputPath, std::ifstream::binary);
        if(!binaryData) {
            throw std::runtime_error("Opening input file failed");
        }

        binaryData.seekg(0, binaryData.end);
        int64_t fileLength = binaryData.tellg();
        binaryData.seekg(0, binaryData.beg);

        auto softBits = std::make_unique<uint8_t[]>(fileLength);

        binaryData.read(reinterpret_cast<char*>(softBits.get()), fileLength);
        decodedPacketCounter = meteorDecoder.decode(softBits.get(), fileLength);

        if(binaryData && binaryData.is_open()) {
            binaryData.close();
        }

    } catch(std::exception ex) {
        std::cout << ex.what() << std::endl;
    }

    if(decodedPacketCounter == 0) {
        std::cout << "No data received, try to make composite images" << std::endl;
    } else {
        // std::cout << "Decoded packets:" << decodedPacketCounter << std::endl;

        DateTime passStart;
        DateTime passDate = mSettings.getPassDate();
        TimeSpan passStartTime = meteorDecoder.getFirstTimeStamp();
        TimeSpan passLength = meteorDecoder.getLastTimeStamp() - passStartTime;

        passStartTime = passStartTime.Add(TimeSpan(0, 0, mSettings.getTimeOffsetSec()));
        passLength = passLength.Add(TimeSpan(0, 0, mSettings.getTimeOffsetSec()));

        passDate = passDate.AddHours(3); // Convert UTC 0 to Moscow time zone (UTC + 3)

        // Satellite's date time
        passStart.Initialise(passDate.Year(), passDate.Month(), passDate.Day(), passStartTime.Hours(), passStartTime.Minutes(), passStartTime.Seconds(), passStartTime.Microseconds());
        // Convert satellite's Moscow time zone to UTC 0
        passStart = passStart.AddHours(-3);

        std::string fileNameDate = std::to_string(passStart.Year()) + "-" + std::to_string(passStart.Month()) + "-" + std::to_string(passStart.Day()) + "-" + std::to_string(passStart.Hour()) + "-" + std::to_string(passStart.Minute()) + "-"
                                   + std::to_string(passStart.Second());

        std::list<ImageForSpread> imagesToSpread;

        if(meteorDecoder.isChannel64Available() && meteorDecoder.isChannel65Available() && meteorDecoder.isChannel68Available()) {
            cv::Mat threatedImage1 = meteorDecoder.getRGBImage(PacketParser::APID65, PacketParser::APID65, PacketParser::APID64, mSettings.fillBackLines());
            cv::Mat irImage = meteorDecoder.getChannelImage(PacketParser::APID68, mSettings.fillBackLines());
            cv::Mat threatedImage2 = meteorDecoder.getRGBImage(PacketParser::APID64, PacketParser::APID65, PacketParser::APID68, mSettings.fillBackLines());

            cv::Mat rainRef = cv::imread(mSettings.getResourcesPath() + "rain.bmp");
            cv::Mat rainOverlay = ThreatImage::irToRain(irImage, rainRef);

            if(!ThreatImage::isNightPass(threatedImage1, mSettings.getNightPassTreshold())) {
                threatedImage1 = ThreatImage::sharpen(threatedImage1);
                threatedImage2 = ThreatImage::sharpen(threatedImage2);

                imagesToSpread.push_back(ImageForSpread(threatedImage1, "221_"));
                imagesToSpread.push_back(ImageForSpread(threatedImage2, "125_"));

                if(mSettings.addRainOverlay()) {
                    imagesToSpread.push_back(ImageForSpread(ThreatImage::addRainOverlay(threatedImage1, rainOverlay), "rain_221_"));
                    imagesToSpread.push_back(ImageForSpread(ThreatImage::addRainOverlay(threatedImage2, rainOverlay), "rain_125_"));
                }

                saveImage(mSettings.getOutputPath() + fileNameDate + "_221.bmp", threatedImage1);
                saveImage(mSettings.getOutputPath() + fileNameDate + "_125.bmp", threatedImage2);
            } else {
                std::cout << "Night pass, RGB image skipped, threshold set to: " << mSettings.getNightPassTreshold() << std::endl;
            }

            cv::Mat ch64 = meteorDecoder.getChannelImage(PacketParser::APID64, mSettings.fillBackLines());
            cv::Mat ch65 = meteorDecoder.getChannelImage(PacketParser::APID65, mSettings.fillBackLines());
            cv::Mat ch68 = meteorDecoder.getChannelImage(PacketParser::APID68, mSettings.fillBackLines());

            saveImage(mSettings.getOutputPath() + fileNameDate + "_64.bmp", ch64);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_65.bmp", ch65);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_68.bmp", ch68);

            cv::Mat thermalRef = cv::imread(mSettings.getResourcesPath() + "thermal_ref.bmp");
            cv::Mat thermalImage = ThreatImage::irToTemperature(irImage, thermalRef);
            imagesToSpread.push_back(ImageForSpread(thermalImage, "thermal_"));

            irImage = ThreatImage::invertIR(irImage);
            irImage = ThreatImage::gamma(irImage, 1.4);
            irImage = ThreatImage::contrast(irImage, 1.3, -40);
            irImage = ThreatImage::sharpen(irImage);
            imagesToSpread.push_back(ImageForSpread(irImage, "IR_"));

            if(mSettings.addRainOverlay()) {
                imagesToSpread.push_back(ImageForSpread(ThreatImage::addRainOverlay(irImage, rainOverlay), "rain_IR_"));
            }

        } else if(meteorDecoder.isChannel64Available() && meteorDecoder.isChannel65Available() && meteorDecoder.isChannel67Available()) {
            cv::Mat ch64 = meteorDecoder.getChannelImage(PacketParser::APID64, mSettings.fillBackLines());
            cv::Mat ch65 = meteorDecoder.getChannelImage(PacketParser::APID65, mSettings.fillBackLines());
            cv::Mat ch67 = meteorDecoder.getChannelImage(PacketParser::APID67, mSettings.fillBackLines());
            cv::Mat irImage = meteorDecoder.getChannelImage(PacketParser::APID67, mSettings.fillBackLines());

            cv::Mat rainRef = cv::imread(mSettings.getResourcesPath() + "rain.bmp");
            cv::Mat rainOverlay = ThreatImage::irToRain(irImage, rainRef);

            saveImage(mSettings.getOutputPath() + fileNameDate + "_64.bmp", ch64);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_65.bmp", ch65);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_67.bmp", ch67);

            cv::Mat thermalRef = cv::imread(mSettings.getResourcesPath() + "thermal_ref.bmp");
            cv::Mat thermalImage = ThreatImage::irToTemperature(irImage, thermalRef);
            imagesToSpread.push_back(ImageForSpread(thermalImage, "thermal_"));

            irImage = ThreatImage::invertIR(irImage);
            irImage = ThreatImage::gamma(irImage, 1.4);
            irImage = ThreatImage::contrast(irImage, 1.3, -40);
            irImage = ThreatImage::sharpen(irImage);
            imagesToSpread.push_back(ImageForSpread(irImage, "IR_"));

            if(mSettings.addRainOverlay()) {
                imagesToSpread.push_back(ImageForSpread(ThreatImage::addRainOverlay(irImage, rainOverlay), "rain_IR_"));
            }
            cv::Mat image221 = meteorDecoder.getRGBImage(PacketParser::APID65, PacketParser::APID65, PacketParser::APID64, mSettings.fillBackLines());
            cv::Mat image224 = meteorDecoder.getRGBImage(PacketParser::APID65, PacketParser::APID65, PacketParser::APID67, mSettings.fillBackLines());

            if(!ThreatImage::isNightPass(image221, mSettings.getNightPassTreshold())) {
                image221 = ThreatImage::sharpen(image221);
                image224 = ThreatImage::sharpen(image224);

                imagesToSpread.push_back(ImageForSpread(image221, "221_"));
                imagesToSpread.push_back(ImageForSpread(image224, "224_"));


                if(mSettings.addRainOverlay()) {
                    imagesToSpread.push_back(ImageForSpread(ThreatImage::addRainOverlay(image221, rainOverlay), "rain_221_"));
                    imagesToSpread.push_back(ImageForSpread(ThreatImage::addRainOverlay(image224, rainOverlay), "rain_224_"));
                }

                saveImage(mSettings.getOutputPath() + fileNameDate + "_221.bmp", image221);
                saveImage(mSettings.getOutputPath() + fileNameDate + "_224.bmp", image224);
            } else {
                std::cout << "Night pass, RGB image skipped, threshold set to: " << mSettings.getNightPassTreshold() << std::endl;
            }

        } else if(meteorDecoder.isChannel64Available() && meteorDecoder.isChannel65Available() && meteorDecoder.isChannel66Available()) {
            cv::Mat threatedImage1 = meteorDecoder.getRGBImage(PacketParser::APID66, PacketParser::APID65, PacketParser::APID64, mSettings.fillBackLines());
            cv::Mat threatedImage2 = meteorDecoder.getRGBImage(PacketParser::APID65, PacketParser::APID65, PacketParser::APID64, mSettings.fillBackLines());

            if(!ThreatImage::isNightPass(threatedImage1, mSettings.getNightPassTreshold())) {
                threatedImage1 = ThreatImage::sharpen(threatedImage1);
                threatedImage2 = ThreatImage::sharpen(threatedImage2);

                imagesToSpread.push_back(ImageForSpread(threatedImage1, "321_"));
                saveImage(mSettings.getOutputPath() + fileNameDate + "_321.bmp", threatedImage1);

                imagesToSpread.push_back(ImageForSpread(threatedImage2, "221_"));
                saveImage(mSettings.getOutputPath() + fileNameDate + "_221.bmp", threatedImage2);
            } else {
                std::cout << "Night pass, RGB image skipped, threshold set to: " << mSettings.getNightPassTreshold() << std::endl;
            }

            meteorDecoder.getChannelImage(PacketParser::APID64, mSettings.fillBackLines());
            meteorDecoder.getChannelImage(PacketParser::APID65, mSettings.fillBackLines());
            meteorDecoder.getChannelImage(PacketParser::APID66, mSettings.fillBackLines());

            cv::Mat ch64 = meteorDecoder.getChannelImage(PacketParser::APID64, mSettings.fillBackLines());
            cv::Mat ch65 = meteorDecoder.getChannelImage(PacketParser::APID65, mSettings.fillBackLines());
            cv::Mat ch66 = meteorDecoder.getChannelImage(PacketParser::APID66, mSettings.fillBackLines());

            saveImage(mSettings.getOutputPath() + fileNameDate + "_64.bmp", ch64);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_65.bmp", ch65);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_66.bmp", ch66);
        } else if(meteorDecoder.isChannel67Available() && meteorDecoder.isChannel68Available() && meteorDecoder.isChannel69Available()) {
            cv::Mat ch67 = meteorDecoder.getChannelImage(PacketParser::APID67, mSettings.fillBackLines());
            cv::Mat ch68 = meteorDecoder.getChannelImage(PacketParser::APID68, mSettings.fillBackLines());
            cv::Mat ch69 = meteorDecoder.getChannelImage(PacketParser::APID69, mSettings.fillBackLines());
            cv::Mat ch654 = meteorDecoder.getRGBImage(PacketParser::APID67, PacketParser::APID68, PacketParser::APID69, mSettings.fillBackLines());

            saveImage(mSettings.getOutputPath() + fileNameDate + "_67.bmp", ch67);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_68.bmp", ch68);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_69.bmp", ch69);

            imagesToSpread.push_back(ImageForSpread(ch654, "654_"));
            saveImage(mSettings.getOutputPath() + fileNameDate + "_654.bmp", ch654);

            cv::Mat thermalRef = cv::imread(mSettings.getResourcesPath() + "thermal_ref.bmp");
            cv::Mat thermalImage = ThreatImage::irToTemperature(ch68, thermalRef);
            imagesToSpread.push_back(ImageForSpread(thermalImage, "thermal_"));

            ch68 = ThreatImage::invertIR(ch68);
            ch68 = ThreatImage::gamma(ch68, 1.4);
            ch68 = ThreatImage::contrast(ch68, 1.3, -40);
            ch68 = ThreatImage::sharpen(ch68);
            imagesToSpread.push_back(ImageForSpread(ch68, "IR_"));

            if(mSettings.addRainOverlay()) {
                cv::Mat rainRef = cv::imread(mSettings.getResourcesPath() + "rain.bmp");
                cv::Mat rainOverlay = ThreatImage::irToRain(ch68, rainRef);
                imagesToSpread.push_back(ImageForSpread(ThreatImage::addRainOverlay(ch68, rainOverlay), "rain_IR_"));
            }

        } else if(meteorDecoder.isChannel64Available() && meteorDecoder.isChannel65Available()) {
            cv::Mat threatedImage = meteorDecoder.getRGBImage(PacketParser::APID65, PacketParser::APID65, PacketParser::APID64, mSettings.fillBackLines());

            if(!ThreatImage::isNightPass(threatedImage, mSettings.getNightPassTreshold())) {
                threatedImage = ThreatImage::sharpen(threatedImage);

                imagesToSpread.push_back(ImageForSpread(threatedImage, "221_"));
                saveImage(mSettings.getOutputPath() + fileNameDate + "_221.bmp", threatedImage);
            } else {
                std::cout << "Night pass, RGB image skipped, threshold set to: " << mSettings.getNightPassTreshold() << std::endl;
            }

            cv::Mat ch64 = meteorDecoder.getChannelImage(PacketParser::APID64, mSettings.fillBackLines());
            cv::Mat ch65 = meteorDecoder.getChannelImage(PacketParser::APID65, mSettings.fillBackLines());

            saveImage(mSettings.getOutputPath() + fileNameDate + "_64.bmp", ch64);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_65.bmp", ch65);
        } else if(meteorDecoder.isChannel68Available()) {
            cv::Mat ch68 = meteorDecoder.getChannelImage(PacketParser::APID68, mSettings.fillBackLines());
            saveImage(mSettings.getOutputPath() + fileNameDate + "_68.bmp", ch68);

            if(mSettings.addRainOverlay()) {
                cv::Mat rainRef = cv::imread(mSettings.getResourcesPath() + "rain.bmp");
                cv::Mat rainOverlay = ThreatImage::irToRain(ch68, rainRef);
                imagesToSpread.push_back(ImageForSpread(ThreatImage::addRainOverlay(ch68, rainOverlay), "rain_IR_"));
            }

            ch68 = ThreatImage::invertIR(ch68);
            ch68 = ThreatImage::gamma(ch68, 1.4);
            ch68 = ThreatImage::contrast(ch68, 1.3, -40);
            ch68 = ThreatImage::sharpen(ch68);
            imagesToSpread.push_back(ImageForSpread(ch68, "IR_"));

            cv::Mat thermalRef = cv::imread(mSettings.getResourcesPath() + "thermal_ref.bmp");
            cv::Mat thermalImage = ThreatImage::irToTemperature(ch68, thermalRef);
            imagesToSpread.push_back(ImageForSpread(thermalImage, "thermal_"));
        } else {
            std::cout << "No usable channel data found!" << std::endl;

            return 0;
        }

        TleReader reader(mSettings.getTlePath());
        TleReader::TLE tle;
        reader.processFile();
        if(!reader.getTLE(mSettings.getSatNameInTLE(), tle)) {
            std::cout << "TLE data not found in TLE file, unable to create projected images..." << std::endl;
            return -1;
        }

        PixelGeolocationCalculator calc(tle, passStart, passLength, mSettings.getScanAngle(), mSettings.getRoll(), mSettings.getPitch(), mSettings.getYaw());
        calc.calcPixelCoordinates();
        calc.save(mSettings.getOutputPath() + fileNameDate + ".gcp");

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << passStart.Day() << "/" << std::setw(2) << passStart.Month() << "/" << passStart.Year() << " " << std::setw(2) << passStart.Hour() << ":" << std::setw(2) << passStart.Minute() << ":"
            << std::setw(2) << passStart.Second() << " UTC";
        std::string dateStr = oss.str();

        std::list<ImageForSpread>::const_iterator it;
        for(it = imagesToSpread.begin(); it != imagesToSpread.end(); ++it) {
            std::string fileName = (*it).fileNameBase + fileNameDate + "." + mSettings.getOutputFormat();

            if(mSettings.spreadImage()) {
                mThreadPool.addJob([=]() {
                    SpreadImage spreadImage;
                    cv::Mat strechedImg = spreadImage.stretch((*it).image);

                    if(!strechedImg.empty()) {
                        ThreatImage::drawWatermark(strechedImg, dateStr);
                        saveImage(mSettings.getOutputPath() + std::string("spread_") + fileName, strechedImg);
                    } else {
                        std::cout << "Failed to strech image" << std::endl;
                    }
                });
            }

            if(mSettings.mercatorProjection()) {
                mThreadPool.addJob([=]() {
                    SpreadImage spreadImage;
                    cv::Mat mercator = spreadImage.mercatorProjection((*it).image, calc, mSettings.getProjectionScale());

                    if(!mercator.empty()) {
                        ThreatImage::drawWatermark(mercator, dateStr);
                        saveImage(mSettings.getOutputPath() + std::string("mercator_") + fileName, mercator);
                    } else {
                        std::cout << "Failed to create mercator projection" << std::endl;
                    }
                });
            }

            if(mSettings.equadistantProjection()) {
                mThreadPool.addJob([=]() {
                    SpreadImage spreadImage;
                    cv::Mat equidistant = spreadImage.equidistantProjection((*it).image, calc, mSettings.getProjectionScale());

                    if(!equidistant.empty()) {
                        ThreatImage::drawWatermark(equidistant, dateStr);
                        saveImage(mSettings.getOutputPath() + std::string("equidistant_") + fileName, equidistant);
                    } else {
                        std::cout << "Failed to create equidistant projection" << std::endl;
                    }
                });
            }
        }

        std::cout << "Generate images" << std::endl;
        mThreadPool.waitForAllJobsDone();
        std::cout << "Generate images done" << std::endl;
        imagesToSpread.clear();
    }

    std::cout << "Generate composite images" << std::endl;
    std::time_t now = std::time(nullptr);
    std::stringstream compositeFileNameDateSS;
    compositeFileNameDateSS << std::put_time(std::localtime(&now), "%Y-%m-%d-%H-%M-%S");

    if(mSettings.generateComposite321()) {
        std::list<cv::Mat> images321;
        std::list<PixelGeolocationCalculator> geolocationCalculators321;
        searchForImages(images321, geolocationCalculators321, "321");

        if(images321.size() > 1 && images321.size() == geolocationCalculators321.size()) {
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(auto& img : images321) {
                    img = ThreatImage::sharpen(img);
                }
            }

            SpreadImage spreadImage;
            if(mSettings.compositeEquadistantProjection()) {
                cv::Mat composite = spreadImage.equidistantProjection(images321, geolocationCalculators321, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate equidistant channel 321 composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_321_composite.jpg", composite);
            }
            if(mSettings.compositeMercatorProjection()) {
                cv::Mat composite = spreadImage.mercatorProjection(images321, geolocationCalculators321, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate mercator channel 321 composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_321_composite.jpg", composite);
            }
        }
    }

    if(mSettings.generateComposite125()) {
        std::list<cv::Mat> images125;
        std::list<PixelGeolocationCalculator> geolocationCalculators125;
        searchForImages(images125, geolocationCalculators125, "125");

        if(images125.size() > 1 && images125.size() == geolocationCalculators125.size()) {
            SpreadImage spreadImage;
            if(mSettings.compositeEquadistantProjection()) {
                cv::Mat composite = spreadImage.equidistantProjection(images125, geolocationCalculators125, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate equidistant channel 125 composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_125_composite.jpg", composite);
            }
            if(mSettings.compositeMercatorProjection()) {
                cv::Mat composite = spreadImage.mercatorProjection(images125, geolocationCalculators125, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate mercator channel 125 composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_125_composite.jpg", composite);
            }
        }
    }

    if(mSettings.generateComposite221()) {
        std::list<cv::Mat> images221;
        std::list<PixelGeolocationCalculator> geolocationCalculators221;
        searchForImages(images221, geolocationCalculators221, "221");

        if(images221.size() > 1 && images221.size() == geolocationCalculators221.size()) {
            SpreadImage spreadImage;
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(auto& img : images221) {
                    img = ThreatImage::sharpen(img);
                }
            }

            if(mSettings.compositeEquadistantProjection()) {
                cv::Mat composite = spreadImage.equidistantProjection(images221, geolocationCalculators221, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate equidistant channel 221 composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_221_composite.jpg", composite);
            }
            if(mSettings.compositeMercatorProjection()) {
                cv::Mat composite = spreadImage.mercatorProjection(images221, geolocationCalculators221, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate mercator channel 221 composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_221_composite.jpg", composite);
            }
        }
    }

    if(mSettings.generateComposite224()) {
        std::list<cv::Mat> images224;
        std::list<PixelGeolocationCalculator> geolocationCalculators224;
        searchForImages(images224, geolocationCalculators224, "224");

        if(images224.size() > 1 && images224.size() == geolocationCalculators224.size()) {
            SpreadImage spreadImage;
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(auto& img : images224) {
                    img = ThreatImage::sharpen(img);
                }
            }

            if(mSettings.compositeEquadistantProjection()) {
                cv::Mat composite = spreadImage.equidistantProjection(images224, geolocationCalculators224, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate equidistant channel 224 composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_224_composite.jpg", composite);
            }
            if(mSettings.compositeMercatorProjection()) {
                cv::Mat composite = spreadImage.mercatorProjection(images224, geolocationCalculators224, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate mercator channel 224 composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_224_composite.jpg", composite);
            }
        }
    }

    if(mSettings.generateComposite68()) {
        std::list<cv::Mat> images68;
        std::list<PixelGeolocationCalculator> geolocationCalculators68;
        searchForImages(images68, geolocationCalculators68, "68");

        if(images68.size() > 1 && images68.size() == geolocationCalculators68.size()) {
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(auto& img : images68) {
                    img = ThreatImage::invertIR(img);
                    img = ThreatImage::gamma(img, 1.4);
                    img = ThreatImage::contrast(img, 1.3, -40);
                    img = ThreatImage::sharpen(img);
                }
            }

            SpreadImage spreadImage;
            if(mSettings.compositeEquadistantProjection()) {
                cv::Mat composite = spreadImage.equidistantProjection(images68, geolocationCalculators68, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate equidistant channel 68 composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_68_composite.jpg", composite);
            }
            if(mSettings.compositeMercatorProjection()) {
                cv::Mat composite = spreadImage.mercatorProjection(images68, geolocationCalculators68, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate mercator channel 68 composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_68_composite.jpg", composite);
            }
        }
    }

    if(mSettings.generateComposite68Rain()) {
        std::list<cv::Mat> images68;
        std::list<PixelGeolocationCalculator> geolocationCalculators68;
        searchForImages(images68, geolocationCalculators68, "68");

        if(images68.size() > 1 && images68.size() == geolocationCalculators68.size()) {
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                cv::Mat rainRef = cv::imread(mSettings.getResourcesPath() + "rain.bmp");
                for(auto& img : images68) {
                    cv::Mat rainOverlay = ThreatImage::irToRain(img, rainRef);
                    img = ThreatImage::invertIR(img);
                    img = ThreatImage::gamma(img, 1.4);
                    img = ThreatImage::contrast(img, 1.3, -40);
                    img = ThreatImage::sharpen(img);
                    img = ThreatImage::addRainOverlay(img, rainOverlay);
                }
            }

            SpreadImage spreadImage;
            if(mSettings.compositeEquadistantProjection()) {
                cv::Mat composite = spreadImage.equidistantProjection(images68, geolocationCalculators68, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate equidistant channel 68 rain composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_68_rain_composite.jpg", composite);
            }
            if(mSettings.compositeMercatorProjection()) {
                cv::Mat composite = spreadImage.mercatorProjection(images68, geolocationCalculators68, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate mercator channel 68 rain composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_68_rain_composite.jpg", composite);
            }
        }
    }

    if(mSettings.generateCompositeThermal()) {
        std::list<cv::Mat> images68;
        std::list<PixelGeolocationCalculator> geolocationCalculators68;
        searchForImages(images68, geolocationCalculators68, "68");

        if(images68.size() > 1 && images68.size() == geolocationCalculators68.size()) {
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                cv::Mat thermalRef = cv::imread(mSettings.getResourcesPath() + "thermal_ref.bmp");
                for(auto& img : images68) {
                    img = ThreatImage::irToTemperature(img, thermalRef);
                }
            }

            SpreadImage spreadImage;
            if(mSettings.compositeEquadistantProjection()) {
                cv::Mat composite = spreadImage.equidistantProjection(images68, geolocationCalculators68, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate equidistant thermal composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_thermal_composite.jpg", composite);
            }
            if(mSettings.compositeMercatorProjection()) {
                cv::Mat composite = spreadImage.mercatorProjection(images68, geolocationCalculators68, mSettings.getCompositeProjectionScale(), [](float progress) {
                    std::cout << "Generate mercator thermal composite image " << (int)progress << "% \t\t\r" << std::flush;
                });
                std::cout << std::endl;
                saveImage(mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_thermal_composite.jpg", composite);
            }
        }
    }

    std::cout << "Generate composite images done" << std::endl;

    mThreadPool.stop();
    return 0;
}

void searchForImages(std::list<cv::Mat>& imagesOut, std::list<PixelGeolocationCalculator>& geolocationCalculatorsOut, const std::string& channelName) {
    std::time_t now = std::time(nullptr);
    std::map<std::time_t, std::tuple<std::string, std::string>> map;

    for(const auto& entry : fs::directory_iterator(mSettings.getOutputPath())) {
        auto ftime = fs::last_write_time(entry);
        std::time_t cftime = std::chrono::system_clock::to_time_t((ftime));
        std::time_t fileCreatedSec = now - cftime;

        if(entry.path().extension() == ".gcp" && fileCreatedSec < (mSettings.getCompositeMaxAgeHours() * 3600)) {
            std::string folder = entry.path().parent_path().generic_string();
            std::string gcpFileName = entry.path().filename().generic_string();
            std::string fileNameBase = gcpFileName.substr(0, gcpFileName.size() - 4);

            do {
                fs::path fileJPG(folder + "/" + fileNameBase + "_" + channelName + ".jpg");

                if(fs::exists(fileJPG)) {
                    map[cftime] = std::make_tuple(entry.path().generic_string(), fileJPG.generic_string());
                    break;
                }

                fs::path fileBMP(folder + "/" + fileNameBase + "_" + channelName + ".bmp");

                if(fs::exists(fileBMP)) {
                    map[cftime] = std::make_tuple(entry.path().generic_string(), fileBMP.generic_string());

                    break;
                }
            } while(false);
        }
    }

    if(map.size() > 1) {
        for(auto const& [time, paths] : map) {
            std::cout << std::get<1>(paths) << std::endl;

            geolocationCalculatorsOut.emplace_back(PixelGeolocationCalculator::load(std::get<0>(paths)));
            imagesOut.emplace_back(cv::imread(std::get<1>(paths)));
        }
    }
}

void saveImage(const std::string fileName, const cv::Mat& image) {
    std::unique_lock<decltype(saveImageMutex)> lock(saveImageMutex);

    std::vector<int> compression_params;
    compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
    compression_params.push_back(mSettings.getJpegQuality());

    try {
        cv::imwrite(fileName, image, compression_params);
    } catch(const cv::Exception& ex) {
        std::cout << "Saving image " << fileName << " failed. error: " << ex.what() << std::endl;
    }
}

void writeSymbolToFile(std::ostream& stream, const Wavreader::complex& sample) {
    int8_t outBuffer[2];

    outBuffer[0] = static_cast<int8_t>(std::clamp(std::imag(sample) * 127.0f, -128.0f, 127.0f));
    outBuffer[1] = static_cast<int8_t>(std::clamp(std::real(sample) * 127.0f, -128.0f, 127.0f));

    stream.write(reinterpret_cast<char*>(outBuffer), sizeof(outBuffer));
}
