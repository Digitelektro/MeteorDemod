CONFIG += c++17 console
CONFIG -= app_bundle

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS LIQUID_BUILD_CPLUSPLUS
DEFINES +=_USE_MATH_DEFINES
DEFINES += _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING

QMAKE_LFLAGS += -lstdc++fs

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    DSP/meteorcostas.cpp \
    DSP/mm.cpp \
    DSP/phasecontrolloop.cpp \
    DSP/pll.cpp \
    DSP/polyphasebank.cpp \
    DSP/window.cpp \
    DSP/windowedsinc.cpp \
    GIS/dbfilereader.cpp \
    decoder/deinterleaver.cpp \
    decoder/viterbi.cpp \
    main.cpp \
    GIS/shapereader.cpp \
    GIS/shaperenderer.cpp \
    decoder/meteorimage.cpp \
    decoder/packetparser.cpp \
    decoder/reedsolomon.cpp \
    decoder/bitio.cpp \
    decoder/correlation.cpp \
    imageproc/spreadimage.cpp \
    imageproc/threatimage.cpp \
    common/settings.cpp \
    tools/databuffer.cpp \
    tools/iniparser.cpp \
    tools/pixelgeolocationcalculator.cpp \
    tools/threadpool.cpp \
    tools/tlereader.cpp \
    tools/matrix.cpp \
    tools/vector.cpp \
    tools/databuffer.cpp \
    DSP/agc.cpp \
    DSP/filter.cpp \
    DSP/iqsource.cpp \
    DSP/meteordemodulator.cpp \
    DSP/wavreader.cpp \
    DSP/mm.cpp


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    DSP/meteorcostas.h \
    DSP/mm.h \
    DSP/phasecontrolloop.h \
    DSP/pll.h \
    DSP/polyphasebank.h \
    DSP/window.h \
    DSP/windowedsinc.h \
    GIS/dbfilereader.h \
    GIS/shapereader.h \
    GIS/shaperenderer.h \
    common/global.h \
    decoder/deinterleaver.h \
    decoder/viterbi.h \
    decoder/meteorimage.h \
    decoder/packetparser.h \
    decoder/bitio.h \
    decoder/correlation.h \
    decoder/reedsolomon.h \
    imageproc/spreadimage.h \
    imageproc/threatimage.h \
    common/settings.h \
    common/version.h \
    tools/databuffer.h \
    tools/iniparser.h \
    tools/pixelgeolocationcalculator.h \
    tools/matrix.h \
    tools/threadpool.h \
    tools/tlereader.h \
    tools/vector.h \
    tools/databuffer.h \
    DSP/agc.h \
    DSP/filter.h \
    DSP/iqsource.h \
    DSP/meteordemodulator.h \
    DSP/wavreader.h \
    DSP/mm.h

INCLUDEPATH +=  ../../opencv/own_build_x86/install/include
INCLUDEPATH +=  ./decoder
INCLUDEPATH +=  ./common
INCLUDEPATH +=  ./tools
INCLUDEPATH +=  ./imageproc
INCLUDEPATH +=  $$PWD/external/libcorrect/include
INCLUDEPATH +=  $$PWD/external/sgp4/libsgp4

win32 {
    !contains(QMAKE_TARGET.arch, x86_64) {
        CONFIG(debug, debug|release) {
            message("x86 debug build")
            LIBS += $$PWD/external/libcorrect/build_x86/lib/Debug/correct.lib
            LIBS += $$PWD/external/sgp4/build_x86/libsgp4/Debug/sgp4.lib

            LIBS += -L$$PWD/../../opencv/own_build_x86/lib/Debug
            SHARED_LIB_FILES = $$files($$PWD/../../opencv/own_build_x86/lib/Debug/*.lib)
            for(FILE, SHARED_LIB_FILES) {
                BASENAME = $$basename(FILE)
                LIBS += -l$$replace(BASENAME,\.lib,)
            }
        } else {
            message("x86 release build")
            LIBS += $$PWD/external/libcorrect/build_x86/lib/Release/correct.lib
            LIBS += $$PWD/external/sgp4/build_x86/libsgp4/Release/sgp4.lib

            LIBS += -L$$PWD/../../opencv/own_build_x86/lib/Release
            SHARED_LIB_FILES = $$files($$PWD/../../opencv/own_build_x86/lib/Release/*.lib)
            for(FILE, SHARED_LIB_FILES) {
                BASENAME = $$basename(FILE)
                LIBS += -l$$replace(BASENAME,\.lib,)
            }
        }
    }
    else {
        CONFIG(debug, debug|release) {
            message("x64 debug build")
            LIBS += $$PWD/external/libcorrect/build/lib/Debug/correct.lib
            LIBS += $$PWD/external/sgp4/build/libsgp4/Debug/sgp4.lib

            LIBS += -L$$PWD/../../opencv/own_build_x64/lib/Debug
            SHARED_LIB_FILES = $$files($$PWD/../../opencv/own_build_x64/lib/Debug/*.lib)
            for(FILE, SHARED_LIB_FILES) {
                BASENAME = $$basename(FILE)
                LIBS += -l$$replace(BASENAME,\.lib,)
            }
        } else {
            message("x64 release build")
            LIBS += $$PWD/external/libcorrect/build/lib/Release/correct.lib
            LIBS += $$PWD/external/sgp4/build/libsgp4/Release/sgp4.lib

            LIBS += -L$$PWD/../../opencv/own_build_x64/lib/Release
            SHARED_LIB_FILES = $$files($$PWD/../../opencv/own_build_x64/lib/Release/*.lib)
            for(FILE, SHARED_LIB_FILES) {
                BASENAME = $$basename(FILE)
                LIBS += -l$$replace(BASENAME,\.lib,)
            }
        }
    }
}


