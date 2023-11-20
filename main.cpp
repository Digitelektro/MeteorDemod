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
#include "blendimages.h"
#include "meteordecoder.h"
#include "pixelgeolocationcalculator.h"
#include "projectimage.h"
#include "protocol/lrpt/decoder.h"
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

struct ImageSearchResult {
    std::list<PixelGeolocationCalculator> geolocationCalculators;
    std::list<cv::Size> imageSizes;
    std::list<cv::Mat> images221;
    std::list<cv::Mat> images321;
    std::list<cv::Mat> images125;
    std::list<cv::Mat> images224;
    std::list<cv::Mat> images67;
    std::list<cv::Mat> images68;
};

ImageSearchResult searchForImages();
void saveImage(const std::string fileName, const cv::Mat& image);
void writeSymbolToFile(std::ostream& stream, const Wavreader::complex& sample);

static std::mutex saveImageMutex;
static Settings& mSettings = Settings::getInstance();
static ThreadPool mThreadPool(std::thread::hardware_concurrency());
static decoder::protocol::lrpt::Decoder mLrptDecoder;

using APID = decoder::protocol::lrpt::Decoder::APIDs;

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

        if(inputPath.substr(inputPath.find_last_of(".") + 1) == "cadu") {
            std::cout << "Input is a .cadu file, processing it..." << std::endl;

            std::ifstream caduBitsStream(inputPath, std::ifstream::binary);
            if(!caduBitsStream) {
                throw std::runtime_error("Opening input file failed");
            }

            uint8_t caduBuffer[1024];
            while(!caduBitsStream.eof()) {
                caduBitsStream.read(reinterpret_cast<char*>(caduBuffer), sizeof(caduBuffer));
                if(caduBitsStream.gcount() == sizeof(caduBuffer)) {
                    mLrptDecoder.process(caduBuffer);
                    std::cout << "Number of readed cadu: " << (decodedPacketCounter++) + 1 << "\t\t\r" << std::endl;
                }
            }
            std::cout << std::endl;

            if(caduBitsStream && caduBitsStream.is_open()) {
                caduBitsStream.close();
            }
        } else {
            std::ifstream softbitsStream(inputPath, std::ifstream::binary);
            if(!softbitsStream) {
                throw std::runtime_error("Opening input file failed");
            }

            const std::string outputPath = inputPath.substr(0, inputPath.find_last_of(".") + 1) + "cadu";
            std::ofstream caduFileStream;
            caduFileStream.open(outputPath, std::ios::binary);

            softbitsStream.seekg(0, softbitsStream.end);
            int64_t fileLength = softbitsStream.tellg();
            softbitsStream.seekg(0, softbitsStream.beg);

            auto softBits = std::make_unique<uint8_t[]>(fileLength);

            softbitsStream.read(reinterpret_cast<char*>(softBits.get()), fileLength);
            decodedPacketCounter = meteorDecoder.decode(softBits.get(), fileLength, [&caduFileStream](const uint8_t* cadu, std::size_t size) {
                if(size == 1024) {
                    mLrptDecoder.process(cadu);
                    caduFileStream.write(reinterpret_cast<const char*>(cadu), size);
                }
            });

            if(softbitsStream && softbitsStream.is_open()) {
                softbitsStream.close();
            }
            if(caduFileStream && caduFileStream.is_open()) {
                caduFileStream.close();
            }
        }
    } catch(const std::exception& ex) {
        std::cout << ex.what() << std::endl;
    }

    if(decodedPacketCounter == 0) {
        std::cout << "No data received, try to make composite images" << std::endl;
    } else {
        // std::cout << "Decoded packets:" << decodedPacketCounter << std::endl;

        DateTime passStart;
        DateTime passDate = mSettings.getPassDate();
        TimeSpan passStartTime = mLrptDecoder.getFirstTimeStamp();
        TimeSpan passLength = mLrptDecoder.getLastTimeStamp() - passStartTime;

        passStartTime = passStartTime.Add(TimeSpan(0, 0, 0, 0, static_cast<int>(mSettings.getTimeOffsetMs() * 1000)));
        passLength = passLength.Add(TimeSpan(0, 0, 0, 0, static_cast<int>(mSettings.getTimeOffsetMs() * 1000)));

        passDate = passDate.AddHours(3); // Convert UTC 0 to Moscow time zone (UTC + 3)

        // Satellite's date time
        passStart.Initialise(passDate.Year(), passDate.Month(), passDate.Day(), passStartTime.Hours(), passStartTime.Minutes(), passStartTime.Seconds(), passStartTime.Microseconds());
        // Convert satellite's Moscow time zone to UTC 0
        passStart = passStart.AddHours(-3);

        std::string fileNameDate = std::to_string(passStart.Year()) + "-" + std::to_string(passStart.Month()) + "-" + std::to_string(passStart.Day()) + "-" + std::to_string(passStart.Hour()) + "-" + std::to_string(passStart.Minute()) + "-"
                                   + std::to_string(passStart.Second());

        std::ofstream datFileStream(mSettings.getOutputPath() + fileNameDate + ".dat");
        if(datFileStream) {
            datFileStream << std::to_string(passStart.Ticks()) << std::endl;
            datFileStream << std::to_string(passLength.Ticks()) << std::endl;
            datFileStream.close();
        }

        std::list<ImageForSpread> imagesToSpread;

        if(mLrptDecoder.isChannel64Available() && mLrptDecoder.isChannel65Available() && mLrptDecoder.isChannel68Available()) {
            cv::Mat threatedImage1 = mLrptDecoder.getRGBImage(APID::APID65, APID::APID65, APID::APID64, mSettings.fillBackLines());
            cv::Mat irImage = mLrptDecoder.getChannelImage(APID::APID68, mSettings.fillBackLines());
            cv::Mat threatedImage2 = mLrptDecoder.getRGBImage(APID::APID64, APID::APID65, APID::APID68, mSettings.fillBackLines());

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

            cv::Mat ch64 = mLrptDecoder.getChannelImage(APID::APID64, mSettings.fillBackLines());
            cv::Mat ch65 = mLrptDecoder.getChannelImage(APID::APID65, mSettings.fillBackLines());
            cv::Mat ch68 = mLrptDecoder.getChannelImage(APID::APID68, mSettings.fillBackLines());

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
            imagesToSpread.push_back(ImageForSpread(irImage, "68_"));

            if(mSettings.addRainOverlay()) {
                imagesToSpread.push_back(ImageForSpread(ThreatImage::addRainOverlay(irImage, rainOverlay), "rain_68_"));
            }

        } else if(mLrptDecoder.isChannel64Available() && mLrptDecoder.isChannel65Available() && mLrptDecoder.isChannel67Available()) {
            cv::Mat ch64 = mLrptDecoder.getChannelImage(APID::APID64, mSettings.fillBackLines());
            cv::Mat ch65 = mLrptDecoder.getChannelImage(APID::APID65, mSettings.fillBackLines());
            cv::Mat ch67 = mLrptDecoder.getChannelImage(APID::APID67, mSettings.fillBackLines());
            cv::Mat irImage = mLrptDecoder.getChannelImage(APID::APID67, mSettings.fillBackLines());

            cv::Mat rainRef = cv::imread(mSettings.getResourcesPath() + "rain.bmp");
            cv::Mat rainOverlay = ThreatImage::irToRain(irImage, rainRef);

            saveImage(mSettings.getOutputPath() + fileNameDate + "_64.bmp", ch64);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_65.bmp", ch65);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_67.bmp", ch67);

            irImage = ThreatImage::equalize(irImage);
            cv::Mat thermalRef = cv::imread(mSettings.getResourcesPath() + "thermal_ref.bmp");
            cv::Mat thermalImage = ThreatImage::irToTemperature(irImage, thermalRef);
            imagesToSpread.push_back(ImageForSpread(thermalImage, "thermal_"));

            irImage = ThreatImage::invertIR(irImage);
            imagesToSpread.push_back(ImageForSpread(irImage, "67_"));

            if(mSettings.addRainOverlay()) {
                imagesToSpread.push_back(ImageForSpread(ThreatImage::addRainOverlay(irImage, rainOverlay), "rain_67_"));
            }
            cv::Mat image221 = mLrptDecoder.getRGBImage(APID::APID65, APID::APID65, APID::APID64, mSettings.fillBackLines());
            cv::Mat image224 = mLrptDecoder.getRGBImage(APID::APID65, APID::APID65, APID::APID67, mSettings.fillBackLines());

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

        } else if(mLrptDecoder.isChannel64Available() && mLrptDecoder.isChannel65Available() && mLrptDecoder.isChannel66Available()) {
            cv::Mat threatedImage1 = mLrptDecoder.getRGBImage(APID::APID66, APID::APID65, APID::APID64, mSettings.fillBackLines());
            cv::Mat threatedImage2 = mLrptDecoder.getRGBImage(APID::APID65, APID::APID65, APID::APID64, mSettings.fillBackLines());

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

            mLrptDecoder.getChannelImage(APID::APID64, mSettings.fillBackLines());
            mLrptDecoder.getChannelImage(APID::APID65, mSettings.fillBackLines());
            mLrptDecoder.getChannelImage(APID::APID66, mSettings.fillBackLines());

            cv::Mat ch64 = mLrptDecoder.getChannelImage(APID::APID64, mSettings.fillBackLines());
            cv::Mat ch65 = mLrptDecoder.getChannelImage(APID::APID65, mSettings.fillBackLines());
            cv::Mat ch66 = mLrptDecoder.getChannelImage(APID::APID66, mSettings.fillBackLines());

            saveImage(mSettings.getOutputPath() + fileNameDate + "_64.bmp", ch64);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_65.bmp", ch65);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_66.bmp", ch66);
        } else if(mLrptDecoder.isChannel67Available() && mLrptDecoder.isChannel68Available() && mLrptDecoder.isChannel69Available()) {
            cv::Mat ch67 = mLrptDecoder.getChannelImage(APID::APID67, mSettings.fillBackLines());
            cv::Mat ch68 = mLrptDecoder.getChannelImage(APID::APID68, mSettings.fillBackLines());
            cv::Mat ch69 = mLrptDecoder.getChannelImage(APID::APID69, mSettings.fillBackLines());
            cv::Mat ch654 = mLrptDecoder.getRGBImage(APID::APID67, APID::APID68, APID::APID69, mSettings.fillBackLines());

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
            imagesToSpread.push_back(ImageForSpread(ch68, "68_"));

            if(mSettings.addRainOverlay()) {
                cv::Mat rainRef = cv::imread(mSettings.getResourcesPath() + "rain.bmp");
                cv::Mat rainOverlay = ThreatImage::irToRain(ch68, rainRef);
                imagesToSpread.push_back(ImageForSpread(ThreatImage::addRainOverlay(ch68, rainOverlay), "rain_68_"));
            }

        } else if(mLrptDecoder.isChannel64Available() && mLrptDecoder.isChannel65Available()) {
            cv::Mat threatedImage = mLrptDecoder.getRGBImage(APID::APID65, APID::APID65, APID::APID64, mSettings.fillBackLines());

            if(!ThreatImage::isNightPass(threatedImage, mSettings.getNightPassTreshold())) {
                threatedImage = ThreatImage::sharpen(threatedImage);

                imagesToSpread.push_back(ImageForSpread(threatedImage, "221_"));
                saveImage(mSettings.getOutputPath() + fileNameDate + "_221.bmp", threatedImage);
            } else {
                std::cout << "Night pass, RGB image skipped, threshold set to: " << mSettings.getNightPassTreshold() << std::endl;
            }

            cv::Mat ch64 = mLrptDecoder.getChannelImage(APID::APID64, mSettings.fillBackLines());
            cv::Mat ch65 = mLrptDecoder.getChannelImage(APID::APID65, mSettings.fillBackLines());

            saveImage(mSettings.getOutputPath() + fileNameDate + "_64.bmp", ch64);
            saveImage(mSettings.getOutputPath() + fileNameDate + "_65.bmp", ch65);
        } else if(mLrptDecoder.isChannel68Available()) {
            cv::Mat ch68 = mLrptDecoder.getChannelImage(APID::APID68, mSettings.fillBackLines());
            saveImage(mSettings.getOutputPath() + fileNameDate + "_68.bmp", ch68);

            if(mSettings.addRainOverlay()) {
                cv::Mat rainRef = cv::imread(mSettings.getResourcesPath() + "rain.bmp");
                cv::Mat rainOverlay = ThreatImage::irToRain(ch68, rainRef);
                imagesToSpread.push_back(ImageForSpread(ThreatImage::addRainOverlay(ch68, rainOverlay), "rain_68_"));
            }

            ch68 = ThreatImage::invertIR(ch68);
            ch68 = ThreatImage::gamma(ch68, 1.4);
            ch68 = ThreatImage::contrast(ch68, 1.3, -40);
            ch68 = ThreatImage::sharpen(ch68);
            imagesToSpread.push_back(ImageForSpread(ch68, "68_"));

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

        PixelGeolocationCalculator calc(
            tle, passStart, passLength, mSettings.getScanAngle(), mSettings.getRoll(), mSettings.getPitch(), mSettings.getYaw(), imagesToSpread.front().image.size().width, imagesToSpread.front().image.size().height);

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << passStart.Day() << "/" << std::setw(2) << passStart.Month() << "/" << passStart.Year() << " " << std::setw(2) << passStart.Hour() << ":" << std::setw(2) << passStart.Minute() << ":"
            << std::setw(2) << passStart.Second() << " UTC";
        std::string dateStr = oss.str();


        ProjectImage rectifier(ProjectImage::Projection::Rectify, calc, mSettings.getProjectionScale());
        ProjectImage mercatorProjector(ProjectImage::Projection::Mercator, calc, mSettings.getProjectionScale());
        ProjectImage equdistantProjector(ProjectImage::Projection::Equidistant, calc, mSettings.getProjectionScale());

        if(mSettings.spreadImage()) {
            rectifier.calculateTransformation(imagesToSpread.front().image.size());
        }

        if(mSettings.mercatorProjection()) {
            std::cout << "Calculate Mercator TPS" << std::endl;
            mercatorProjector.calculateTransformation(imagesToSpread.front().image.size());
            std::cout << "Calculate Mercator TPS Done" << std::endl;
        }

        if(mSettings.equadistantProjection()) {
            std::cout << "Calculate Equidistant TPS" << std::endl;
            equdistantProjector.calculateTransformation(imagesToSpread.front().image.size());
            std::cout << "Calculate Equidistant Done" << std::endl;
        }
        std::list<ImageForSpread>::const_iterator it;
        for(const auto& img : imagesToSpread) {
            std::string fileName = img.fileNameBase + fileNameDate + "." + mSettings.getOutputFormat();

            if(mSettings.spreadImage()) {
                cv::Mat spreaded = rectifier.project(img.image);
                const std::string filePath = mSettings.getOutputPath() + std::string("spread_") + fileName;
                std::cout << "Saving " << filePath << std::endl;
                saveImage(filePath, spreaded);
            }

            if(mSettings.mercatorProjection()) {
                cv::Mat mercator = mercatorProjector.project(img.image);
                const std::string filePath = mSettings.getOutputPath() + std::string("mercator_") + fileName;
                std::cout << "Saving " << filePath << std::endl;
                saveImage(filePath, mercator);
            }

            if(mSettings.equadistantProjection()) {
                cv::Mat equidistant = equdistantProjector.project(img.image);
                const std::string filePath = mSettings.getOutputPath() + std::string("equidistant_") + fileName;
                std::cout << "Saving " << filePath << std::endl;
                saveImage(filePath, equidistant);
            }
        }

        std::cout << "Save images done" << std::endl;
    }

    std::cout << "Generate composite images" << std::endl;
    std::time_t now = std::time(nullptr);
    std::stringstream compositeFileNameDateSS;
    compositeFileNameDateSS << std::put_time(std::localtime(&now), "%Y-%m-%d-%H-%M-%S");
    std::list<ProjectImage> equidistantTransform;
    std::list<ProjectImage> mercatorTransform;
    ImageSearchResult images = searchForImages();
    if(images.geolocationCalculators.size() > 1) {
        if(mSettings.compositeEquadistantProjection()) {
            equidistantTransform = ProjectImage::createCompositeProjector(ProjectImage::Projection::Equidistant, images.geolocationCalculators, mSettings.getCompositeProjectionScale());
            auto imgSizeIt = images.imageSizes.begin();
            for(auto& transform : equidistantTransform) {
                std::cout << "Calculate Composite Equidistant TPS" << std::endl;
                transform.calculateTransformation(*imgSizeIt++);
                std::cout << "Calculate Composite Equidistant TPS done" << std::endl;
            }
        }
        if(mSettings.compositeMercatorProjection()) {
            mercatorTransform = ProjectImage::createCompositeProjector(ProjectImage::Projection::Mercator, images.geolocationCalculators, mSettings.getCompositeProjectionScale());
            auto imgSizeIt = images.imageSizes.begin();
            for(auto& transform : mercatorTransform) {
                std::cout << "Calculate Composite Mercator TPS" << std::endl;
                transform.calculateTransformation(*imgSizeIt++);
                std::cout << "Calculate Composite Mercator TPS done" << std::endl;
            }
        }
    }

    if(mSettings.generateComposite221()) {
        if(images.images221.size() > 1) {
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(auto& img : images.images221) {
                    img = ThreatImage::sharpen(img);
                }
            }

            if(mSettings.compositeEquadistantProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = equidistantTransform.begin();
                for(auto& img : images.images221) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_221_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }

            if(mSettings.compositeMercatorProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = mercatorTransform.begin();
                for(auto& img : images.images221) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_221_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }
        }
    }

    if(mSettings.generateComposite321()) {
        if(images.images321.size() > 1) {
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(auto& img : images.images321) {
                    img = ThreatImage::sharpen(img);
                }
            }

            if(mSettings.compositeEquadistantProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = equidistantTransform.begin();
                for(auto& img : images.images321) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_321_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }

            if(mSettings.compositeMercatorProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = mercatorTransform.begin();
                for(auto& img : images.images321) {

                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_321_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }
        }
    }

    if(mSettings.generateComposite125()) {
        if(images.images125.size() > 1) {
            if(mSettings.compositeEquadistantProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = equidistantTransform.begin();
                for(auto& img : images.images125) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_125_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }

            if(mSettings.compositeMercatorProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = mercatorTransform.begin();
                for(auto& img : images.images125) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_125_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }
        }
    }

    if(mSettings.generateComposite224()) {
        if(images.images224.size() > 1) {
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(auto& img : images.images224) {
                    img = ThreatImage::sharpen(img);
                }
            }

            if(mSettings.compositeEquadistantProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = equidistantTransform.begin();
                for(const auto& img : images.images224) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_224_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }

            if(mSettings.compositeMercatorProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = mercatorTransform.begin();
                for(auto& img : images.images224) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_224_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }
        }
    }


    if(mSettings.generateComposite68()) {
        if(images.images68.size() > 1) {
            std::list<cv::Mat> irImages;
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(const auto& img : images.images68) {
                    auto ir = ThreatImage::invertIR(img);
                    ir = ThreatImage::gamma(ir, 1.4);
                    ir = ThreatImage::contrast(ir, 1.3, -40);
                    ir = ThreatImage::sharpen(ir);
                    irImages.emplace_back(ir);
                }
            }

            if(mSettings.compositeEquadistantProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = equidistantTransform.begin();
                for(const auto& img : irImages) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_68_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }

            if(mSettings.compositeMercatorProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = mercatorTransform.begin();
                for(const auto& img : irImages) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_68_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }
        }

        if(images.images67.size() > 1) {
            std::list<cv::Mat> irImages;
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(const auto& img : images.images67) {
                    auto irImage = ThreatImage::equalize(img);
                    irImage = ThreatImage::invertIR(irImage);
                    irImages.emplace_back(irImage);
                }
            }

            if(mSettings.compositeEquadistantProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = equidistantTransform.begin();
                for(const auto& img : irImages) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_67_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }

            if(mSettings.compositeMercatorProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = mercatorTransform.begin();
                for(const auto& img : irImages) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_67_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }
        }
    }

    if(mSettings.generateCompositeThermal()) {
        cv::Mat thermalRef = cv::imread(mSettings.getResourcesPath() + "thermal_ref.bmp");
        if(images.images68.size() > 1) {
            std::list<cv::Mat> thermalImages;
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(const auto& img : images.images68) {
                    thermalImages.emplace_back(ThreatImage::irToTemperature(img, thermalRef));
                }
            }

            if(mSettings.compositeEquadistantProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = equidistantTransform.begin();
                for(const auto& img : thermalImages) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_68_thermal_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }

            if(mSettings.compositeMercatorProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = mercatorTransform.begin();
                for(const auto& img : thermalImages) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_68_thermal_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }
        }
        if(images.images67.size() > 1) {
            std::list<cv::Mat> thermalImages;
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(const auto& img : images.images67) {
                    thermalImages.emplace_back(ThreatImage::irToTemperature(ThreatImage::equalize(img), thermalRef));
                }
            }

            if(mSettings.compositeEquadistantProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = equidistantTransform.begin();
                for(const auto& img : thermalImages) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_67_thermal_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }

            if(mSettings.compositeMercatorProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = mercatorTransform.begin();
                for(const auto& img : thermalImages) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_67_thermal_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }
        }
    }

    if(mSettings.generateComposite68Rain()) {
        if(images.images68.size() > 1) {
            std::list<cv::Mat> irImages;
            cv::Mat rainRef = cv::imread(mSettings.getResourcesPath() + "rain.bmp");
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(const auto& img : images.images68) {
                    cv::Mat rainOverlay = ThreatImage::irToRain(img, rainRef);
                    cv::Mat ir = ThreatImage::invertIR(img);
                    ir = ThreatImage::gamma(ir, 1.4);
                    ir = ThreatImage::contrast(ir, 1.3, -40);
                    ir = ThreatImage::sharpen(ir);
                    ir = ThreatImage::addRainOverlay(ir, rainOverlay);
                    irImages.emplace_back(ir);
                }
            }

            if(mSettings.compositeEquadistantProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = equidistantTransform.begin();
                for(const auto& img : irImages) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_68_rain_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }

            if(mSettings.compositeMercatorProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = mercatorTransform.begin();
                for(const auto& img : irImages) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_68_rain_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }
        }

        if(images.images67.size() > 1) {
            std::list<cv::Mat> irImages;
            cv::Mat rainRef = cv::imread(mSettings.getResourcesPath() + "rain.bmp");
            if(mSettings.compositeEquadistantProjection() || mSettings.compositeMercatorProjection()) {
                for(const auto& img : images.images67) {
                    cv::Mat rainOverlay = ThreatImage::irToRain(img, rainRef);
                    irImages.emplace_back(ThreatImage::addRainOverlay(ThreatImage::invertIR(ThreatImage::equalize(img)), rainOverlay));
                }
            }

            if(mSettings.compositeEquadistantProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = equidistantTransform.begin();
                for(const auto& img : irImages) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "equidistant_" + compositeFileNameDateSS.str() + "_67_rain_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }

            if(mSettings.compositeMercatorProjection()) {
                std::list<cv::Mat> imagesToBlend;
                auto transformIt = mercatorTransform.begin();
                for(const auto& img : irImages) {
                    imagesToBlend.emplace_back(transformIt->project(img));
                    transformIt++;
                }
                cv::Mat composite = BlendImages::merge(imagesToBlend);
                const std::string filePath = mSettings.getOutputPath() + "mercator_" + compositeFileNameDateSS.str() + "_67_rain_composite.jpg";
                std::cout << "Saving composite: " << filePath << std::endl;
                saveImage(filePath, composite);
            }
        }
    }

    std::cout << "Generate composite images done" << std::endl;

    mThreadPool.stop();
    return 0;
}

// 221, 321, 125, 224, 68, 67
ImageSearchResult searchForImages() {
    ImageSearchResult result;
    std::time_t now = std::time(nullptr);
    std::map<std::time_t, std::tuple<std::string, std::string>> map;

    TleReader reader(mSettings.getTlePath());
    TleReader::TLE tle;
    reader.processFile();
    if(!reader.getTLE(mSettings.getSatNameInTLE(), tle)) {
        std::cout << "TLE data not found in TLE file, unable to create composite images..." << std::endl;
        return result;
    }

    std::map<time_t, fs::directory_entry> entriesSortByTime;
    for(const auto& entry : fs::directory_iterator(mSettings.getOutputPath())) {
        auto ftime = fs::last_write_time(entry);
        std::time_t cftime = std::chrono::system_clock::to_time_t((ftime));
        if(entry.path().extension() == ".dat") {
            entriesSortByTime[cftime] = entry;
            std::cout << "Enty" << entry << ", time=" << cftime << std::endl;
        }
    }

    for(const auto& [timeStamp, entry] : entriesSortByTime) {
        std::time_t fileCreatedSec = now - timeStamp;
        if(entry.path().extension() == ".dat" && fileCreatedSec < (mSettings.getCompositeMaxAgeHours() * 3600)) {
            std::string folder = entry.path().parent_path().generic_string();
            std::string gcpFileName = entry.path().filename().generic_string();
            std::string fileNameBase = gcpFileName.substr(0, gcpFileName.size() - 4);
            std::string pathBase = folder + "/" + fileNameBase;
            cv::Mat img;

            fs::path imagePath(pathBase + "_221.bmp");
            if(fs::exists(imagePath)) {
                img = cv::imread(imagePath.generic_string());
                result.images221.emplace_back(img);
            }
            imagePath = pathBase + "_321.bmp";
            if(fs::exists(imagePath)) {
                img = cv::imread(imagePath.generic_string());
                result.images321.emplace_back(img);
            }
            imagePath = pathBase + "_125.bmp";
            if(fs::exists(imagePath)) {
                img = cv::imread(imagePath.generic_string());
                result.images125.emplace_back(img);
            }
            imagePath = pathBase + "_224.bmp";
            if(fs::exists(imagePath)) {
                img = cv::imread(imagePath.generic_string());
                result.images224.emplace_back(img);
            }
            imagePath = pathBase + "_67.bmp";
            if(fs::exists(imagePath)) {
                img = cv::imread(imagePath.generic_string());
                result.images67.emplace_back(img);
            }
            imagePath = pathBase + "_68.bmp";
            if(fs::exists(imagePath)) {
                img = cv::imread(imagePath.generic_string());
                result.images68.emplace_back(img);
            }

            if(!img.empty()) {
                result.imageSizes.emplace_back(img.size());
                std::ifstream datFileStream(entry.path().generic_string());
                if(!datFileStream) {
                    continue;
                }
                std::string line1, line2;
                datFileStream >> line1;
                datFileStream >> line2;
                datFileStream.close();
                int64_t ticks = std::stoll(line1);
                DateTime passStart(ticks);
                ticks = std::stoll(line2);
                TimeSpan passLength(ticks);
                PixelGeolocationCalculator calc(tle, passStart, passLength, mSettings.getScanAngle(), mSettings.getRoll(), mSettings.getPitch(), mSettings.getYaw(), img.size().width, img.size().height);
                result.geolocationCalculators.emplace_back(calc);
            }
        }
    }
    return result;
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
