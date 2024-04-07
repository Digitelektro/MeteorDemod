#pragma once

#include <array>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

#include "segment.h"
#include "threatimage.h"

namespace decoder {
namespace protocol {
namespace lrpt {
namespace msumr {

class Image {
    struct ac_table_rec {
        int run;
        int size;
        int len;
        uint32_t mask;
        uint32_t code;
    };


  public:
    enum APIDs { APID64 = 64, APID65, APID66, APID67, APID68, APID69 };

  public:
    Image();
    virtual ~Image();

    cv::Mat getRGBImage(APIDs redAPID, APIDs greenAPID, APIDs blueAPID, bool fillBlackLines = true, bool invertR = false, bool invertG = false, bool invertB = false);
    cv::Mat getChannelImage(APIDs APID, bool fillBlackLines = true);

  public:
    bool isChannel64Available() const {
        return mIsChannel64Available;
    }
    bool isChannel65Available() const {
        return mIsChannel65Available;
    }
    bool isChannel66Available() const {
        return mIsChannel66Available;
    }
    bool isChannel67Available() const {
        return mIsChannel67Available;
    }
    bool isChannel68Available() const {
        return mIsChannel68Available;
    }
    bool isChannel69Available() const {
        return mIsChannel69Available;
    }

  protected:
    void decode(uint16_t apid, uint16_t packetCount, const Segment& segment);
    void decMCUs(const uint8_t* packet, int len, int apd, int pck_cnt, int mcu_id, uint8_t q);

    int getLastY() const {
        return mLastY;
    }

    int getCurrentY() const {
        return mCurY;
    }

  private:
    void initHuffmanTable();
    void initCos();
    int getDcReal(uint16_t word);
    int getAcReal(uint16_t word);
    bool progressImage(int apd, int mcuID, int pckCnt);
    void fillDqtByQ(std::array<int, 64>& dqt, int q);
    int mapRange(int cat, int vl);
    void filtIdct8x8(std::array<float, 64>& res, std::array<float, 64>& inp);
    void fillPix(std::array<float, 64>& imgDct, int apd, int mcu_id, int m);

  private:
    bool mIsChannel64Available;
    bool mIsChannel65Available;
    bool mIsChannel66Available;
    bool mIsChannel67Available;
    bool mIsChannel68Available;
    bool mIsChannel69Available;

    std::vector<std::array<uint8_t, 6>> mChannels;
    int mLastMCU, mCurY, mLastY, mFirstPacket, mPrevPacket;
    std::array<int, 65536> mAcLookup{}, mDcLookup{};
    std::array<ac_table_rec, 162> mAcTable{};
    std::array<std::array<float, 8>, 8> mCosine{};
    std::array<float, 8> mAlpha;

  private:
    static constexpr uint16_t cMCUPerPacket = 14;
    static constexpr uint16_t cMCUPerLine = 196;
};


} // namespace msumr
} // namespace lrpt
} // namespace protocol
} // namespace decoder
