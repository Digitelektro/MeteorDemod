#include "viterbi.h"

Viterbi::Viterbi(int k, uint8_t polynomA, uint8_t polynomB)
    : mpConvolutional(nullptr) {
    mPolynomials[0] = polynomA;
    mPolynomials[1] = polynomB;

    mpConvolutional = correct_convolutional_create(2, k, mPolynomials);
}

Viterbi::~Viterbi() {
    if(mpConvolutional) {
        correct_convolutional_destroy(mpConvolutional);
    }
}

size_t Viterbi::decodeSoft(const uint8_t* data, uint8_t* result, size_t blockSize) {
    if(mpConvolutional == nullptr) {
        return -1;
    }

    size_t reencodedSize = blockSize / 8;
    if(mReencodedBuffer.size() != reencodedSize) {
        mReencodedBuffer.resize(reencodedSize);
    }

    size_t decodedBytes = correct_convolutional_decode_soft(mpConvolutional, data, blockSize, result);
    if(decodedBytes != -1) {
        correct_convolutional_encode(mpConvolutional, result, decodedBytes, mReencodedBuffer.data());
        calculateBer(data, mReencodedBuffer.data(), blockSize);
    }
    return decodedBytes;
}

void Viterbi::calculateBer(const uint8_t* original, const uint8_t* reEncoded, ssize_t blockSize) {
    float errors = 0, total = 0;
    for(int i = 0; i < blockSize / 8; i++) {
        for(int bit = 7; bit >= 0; bit--) {
            if(*original != 128) {
                errors += (*original > 128) != ((reEncoded[i] >> bit) & 0x01);
                total++;
            }
            original++;
        }
    }

    if(total == 0) {
        mLastBER = 0.0f;
    } else {
        mLastBER = (errors / total);
    }
}
