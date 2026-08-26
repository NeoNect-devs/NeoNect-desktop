// src/main.cpp
#include "core/application.h"
#include <QtCore/QByteArray>

int main(int argc, char *argv[]) {
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
#ifdef _WIN32
    qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", "none");
    qputenv("QT_FFMPEG_ENCODING_HW_DEVICE_TYPES", "none");
    qputenv("QT_DISABLE_HW_TEXTURES_CONVERSION", "1");
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");
#endif

    Avila::Application app(argc, argv);
    return app.run();
}
