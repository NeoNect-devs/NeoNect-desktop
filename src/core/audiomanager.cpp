// src/core/audiomanager.cpp
#include "audiomanager.h"
#include <QRandomGenerator>
#include <QUuid>
#include <QtMath>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QDesktopServices>
#include <QFileInfo>
#include <QSettings>
#include <cstring>
#include "../storage/StoragePathResolver.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#else
#include <QLibrary>
#include <cmath>

namespace {

struct pa_sample_spec {
    int format;
    uint32_t rate;
    uint8_t channels;
};

struct pa_buffer_attr {
    uint32_t maxlength;
    uint32_t tlength;
    uint32_t prebuf;
    uint32_t minreq;
    uint32_t fragsize;
};

using pa_simple = void;

using fn_pa_simple_new = pa_simple* (*)(const char *server, const char *name, int dir, const char *dev, const char *stream_name, const pa_sample_spec *ss, const void *map, const pa_buffer_attr *attr, int *error);
using fn_pa_simple_free = void (*)(pa_simple *s);
using fn_pa_simple_read = int (*)(pa_simple *s, void *data, size_t bytes, int *error);
using fn_pa_simple_write = int (*)(pa_simple *s, const void *data, size_t bytes, int *error);
using fn_pa_simple_drain = int (*)(pa_simple *s, int *error);

class PulseAudioHelper {
public:
    static PulseAudioHelper& instance() {
        static PulseAudioHelper s_instance;
        return s_instance;
    }

    bool isAvailable() const { return m_available; }

    pa_simple* createStream(int dir, int sampleRate, int channels, const pa_buffer_attr *attr = nullptr) {
        if (!m_available || !m_fnNew) return nullptr;
        pa_sample_spec ss;
        ss.format = 3;
        ss.rate = static_cast<uint32_t>(sampleRate > 0 ? sampleRate : 16000);
        ss.channels = static_cast<uint8_t>(channels > 0 ? channels : 1);
        int error = 0;
        return m_fnNew(nullptr, "NeoNect", dir, nullptr, dir == 2 ? "voice-record" : "voice-playback", &ss, nullptr, attr, &error);
    }

    void freeStream(pa_simple* s) {
        if (s && m_fnFree) m_fnFree(s);
    }

    int read(pa_simple* s, void* data, size_t bytes) {
        if (!s || !m_fnRead) return -1;
        int error = 0;
        return m_fnRead(s, data, bytes, &error);
    }

    int write(pa_simple* s, const void* data, size_t bytes) {
        if (!s || !m_fnWrite) return -1;
        int error = 0;
        return m_fnWrite(s, data, bytes, &error);
    }

    int drain(pa_simple* s) {
        if (!s || !m_fnDrain) return -1;
        int error = 0;
        return m_fnDrain(s, &error);
    }

private:
    PulseAudioHelper() {
        m_lib.setFileName("libpulse-simple.so.0");
        if (!m_lib.load()) {
            m_lib.setFileName("pulse-simple");
            m_lib.load();
        }
        if (m_lib.isLoaded()) {
            m_fnNew = reinterpret_cast<fn_pa_simple_new>(m_lib.resolve("pa_simple_new"));
            m_fnFree = reinterpret_cast<fn_pa_simple_free>(m_lib.resolve("pa_simple_free"));
            m_fnRead = reinterpret_cast<fn_pa_simple_read>(m_lib.resolve("pa_simple_read"));
            m_fnWrite = reinterpret_cast<fn_pa_simple_write>(m_lib.resolve("pa_simple_write"));
            m_fnDrain = reinterpret_cast<fn_pa_simple_drain>(m_lib.resolve("pa_simple_drain"));
            m_available = (m_fnNew && m_fnFree && m_fnRead && m_fnWrite && m_fnDrain);
        }
    }

    QLibrary m_lib;
    bool m_available{false};
    fn_pa_simple_new m_fnNew{nullptr};
    fn_pa_simple_free m_fnFree{nullptr};
    fn_pa_simple_read m_fnRead{nullptr};
    fn_pa_simple_write m_fnWrite{nullptr};
    fn_pa_simple_drain m_fnDrain{nullptr};
};

}
#endif


AudioManager::AudioManager(QObject *parent)
    : QObject(parent),
      m_recordTimer(new QTimer(this)),
      m_playbackTimer(new QTimer(this)) {

    m_recordTimer->setInterval(100); // 10 ticks per second
    connect(m_recordTimer, &QTimer::timeout, this, &AudioManager::onRecordTimerTick);

    m_playbackTimer->setInterval(50); // 20 FPS playback tick
    connect(m_playbackTimer, &QTimer::timeout, this, &AudioManager::onPlaybackTimerTick);

    // Seed live waveform with initial placeholder bars
    for (int i = 0; i < 24; ++i) {
        m_liveWaveform.append(0.2);
    }

    QSettings settings("NeoNect", "NeoNect");
    m_volume = settings.value("audio/media_volume", 1.0).toDouble();
    if (m_volume < 0.0) m_volume = 0.0;
    if (m_volume > 1.0) m_volume = 1.0;
    m_isMuted = settings.value("audio/media_muted", false).toBool();
    m_volumeAtomic.store(m_isMuted ? 0.0 : m_volume);
}

AudioManager::~AudioManager() {
    stopMci();
    stopMicrophoneThread();
    stopAudioPlaybackThread();
}

void AudioManager::playUiBeep(int freqHz, int durationMs) {
#ifdef _WIN32
    Q_UNUSED(freqHz);
    Q_UNUSED(durationMs);
    MessageBeep(MB_OK);
#else
    auto &pulse = PulseAudioHelper::instance();
    if (!pulse.isAvailable()) return;
    int sampleRate = 16000;
    pa_simple *s = pulse.createStream(1, sampleRate, 1);
    if (!s) return;
    int samples = std::clamp(durationMs, 10, 500) * sampleRate / 1000;
    std::vector<int16_t> buf(samples);
    double freq = std::clamp(freqHz, 100, 5000);
    double vol = m_volumeAtomic.load();
    for (int i = 0; i < samples; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double sVal = std::sin(2.0 * M_PI * freq * t) * 12000.0 * vol;
        buf[i] = static_cast<int16_t>(qBound(-32000.0, sVal, 32000.0));
    }
    pulse.write(s, buf.data(), buf.size() * sizeof(int16_t));
    pulse.drain(s);
    pulse.freeStream(s);
#endif
}

void AudioManager::setStorageBoundary(const QString& serverUrl, const QString& username) {
    std::lock_guard<std::mutex> lock(m_audioMutex);
    m_serverUrl = serverUrl;
    m_username = username;
}

void AudioManager::cleanupLocalFile(const QString& url) {
    if (url.isEmpty()) return;
    
    QString sUrl, sUser;
    {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        sUrl = m_serverUrl;
        sUser = m_username;
    }

    if (sUrl.isEmpty() || sUser.isEmpty()) return;

    QString localPath = url;
    if (localPath.startsWith("file:///")) {
        localPath = QUrl(localPath).toLocalFile();
    }

    QString baseDir = NeoNect::Storage::StoragePathResolver::cacheDirectory(sUrl, sUser);
    QString legacyRoot = QDir(baseDir + "/voice_notes").canonicalPath();
    QString mediaVoiceDir = NeoNect::Storage::StoragePathResolver::mediaVoiceDirectory(sUrl, sUser);
    QString expectedRoot = QDir(mediaVoiceDir).canonicalPath();

    QFileInfo fi(localPath);
    if (!fi.exists()) return;

    QString canonPath = fi.canonicalFilePath();
    if ((!expectedRoot.isEmpty() && (canonPath.startsWith(expectedRoot + "/") || canonPath == expectedRoot)) ||
        (!legacyRoot.isEmpty() && (canonPath.startsWith(legacyRoot + "/") || canonPath == legacyRoot))) {
        QFile::remove(canonPath);
    }
}

void AudioManager::openMediaFile(const QString &mediaUrl) {
    if (mediaUrl.isEmpty()) return;
    QString cleanPath = mediaUrl;
    if (cleanPath.startsWith("file:///")) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(cleanPath));
}

qint64 AudioManager::getFileSize(const QString &fileUrl) {
    if (fileUrl.isEmpty()) return 0;
    QString cleanPath = fileUrl;
    QUrl url(fileUrl);
    if (url.isLocalFile()) {
        cleanPath = url.toLocalFile();
    } else if (cleanPath.startsWith("file:///")) {
        cleanPath = cleanPath.mid(8);
    } else if (cleanPath.startsWith("file://")) {
        cleanPath = cleanPath.mid(7);
    }
#ifdef _WIN32
    if (cleanPath.startsWith("/") && cleanPath.length() >= 3 && cleanPath.at(2) == ':') {
        cleanPath = cleanPath.mid(1);
    }
#endif
    QFileInfo fi(cleanPath);
    return (fi.exists() && fi.isFile()) ? fi.size() : 0;
}

QString AudioManager::formatFileSize(qint64 bytes) {
    if (bytes <= 0) return "0 B";
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    }
    if (bytes < 1024LL * 1024 * 1024) {
        return QString("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 1));
    }
    return QString("%1 GB").arg(QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2));
}

void AudioManager::setVolume(qreal vol) {
    qreal clamped = std::max(0.0, std::min(1.0, vol));
    if (qFuzzyCompare(m_volume, clamped)) return;
    m_volume = clamped;
    if (m_volume > 0.0 && m_isMuted) {
        m_isMuted = false;
        emit isMutedChanged();
    }
    m_volumeAtomic.store(m_isMuted ? 0.0 : m_volume);

#ifdef _WIN32
    if (m_isMciActive) {
        int mciVol = static_cast<int>((m_isMuted ? 0.0 : m_volume) * 1000);
        std::wstring volCmd = L"setaudio neonect_audio volume to " + std::to_wstring(mciVol);
        mciSendStringW(volCmd.c_str(), NULL, 0, NULL);
    }
#endif

    QSettings settings("NeoNect", "NeoNect");
    settings.setValue("audio/media_volume", m_volume);
    settings.setValue("audio/media_muted", m_isMuted);
    settings.sync();

    emit volumeChanged();
}

void AudioManager::setMuted(bool muted) {
    if (m_isMuted == muted) return;
    m_isMuted = muted;
    m_volumeAtomic.store(m_isMuted ? 0.0 : m_volume);

#ifdef _WIN32
    if (m_isMciActive) {
        int mciVol = static_cast<int>((m_isMuted ? 0.0 : m_volume) * 1000);
        std::wstring volCmd = L"setaudio neonect_audio volume to " + std::to_wstring(mciVol);
        mciSendStringW(volCmd.c_str(), NULL, 0, NULL);
    }
#endif

    QSettings settings("NeoNect", "NeoNect");
    settings.setValue("audio/media_muted", m_isMuted);
    settings.sync();

    emit isMutedChanged();
}

void AudioManager::toggleMute() {
    setMuted(!m_isMuted);
}

// ─────────────────────────────────────────────────────────────────────────────
// MCI MEDIA PLAYBACK (MP3, WAV, FLAC, AAC, OGG)
// ─────────────────────────────────────────────────────────────────────────────
bool AudioManager::playViaMci(const QString &filePath) {
#ifdef _WIN32
    stopMci();

    QString cleanPath = filePath;
    if (cleanPath.startsWith("file:///")) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }
    cleanPath = QDir::toNativeSeparators(cleanPath);

    if (!QFile::exists(cleanPath)) return false;

    // Use short path name to avoid spaces and special character problems in MCI
    std::wstring wpath = cleanPath.toStdWString();
    wchar_t shortPath[MAX_PATH];
    if (GetShortPathNameW(wpath.c_str(), shortPath, MAX_PATH) > 0) {
        wpath = shortPath;
    }

    std::wstring openCmd = L"open \"" + wpath + L"\" type mpegvideo alias neonect_audio";
    MCIERROR err = mciSendStringW(openCmd.c_str(), NULL, 0, NULL);
    if (err != 0) {
        openCmd = L"open \"" + wpath + L"\" alias neonect_audio";
        err = mciSendStringW(openCmd.c_str(), NULL, 0, NULL);
        if (err != 0) return false;
    }

    mciSendStringW(L"set neonect_audio time format milliseconds", NULL, 0, NULL);

    int mciVol = static_cast<int>((m_isMuted ? 0.0 : m_volume) * 1000);
    std::wstring volCmd = L"setaudio neonect_audio volume to " + std::to_wstring(mciVol);
    mciSendStringW(volCmd.c_str(), NULL, 0, NULL);

    wchar_t lenBuf[128];
    if (mciSendStringW(L"status neonect_audio length", lenBuf, 128, NULL) == 0) {
        int len = QString::fromWCharArray(lenBuf).toInt();
        if (len > 0) {
            m_totalDuration = len;
            emit playbackDurationChanged();
        }
    }

    mciSendStringW(L"play neonect_audio", NULL, 0, NULL);
    m_isMciActive = true;
    return true;
#else
    Q_UNUSED(filePath);
    return false;
#endif
}

void AudioManager::pauseMci() {
#ifdef _WIN32
    if (m_isMciActive) {
        mciSendStringW(L"pause neonect_audio", NULL, 0, NULL);
    }
#endif
}

void AudioManager::resumeMci() {
#ifdef _WIN32
    if (m_isMciActive) {
        mciSendStringW(L"resume neonect_audio", NULL, 0, NULL);
    }
#endif
}

void AudioManager::seekMci(int positionMs) {
#ifdef _WIN32
    if (m_isMciActive) {
        std::wstring seekCmd = L"play neonect_audio from " + std::to_wstring(positionMs);
        mciSendStringW(seekCmd.c_str(), NULL, 0, NULL);
    }
#endif
}

void AudioManager::stopMci() {
#ifdef _WIN32
    if (m_isMciActive) {
        mciSendStringW(L"stop neonect_audio", NULL, 0, NULL);
        mciSendStringW(L"close neonect_audio", NULL, 0, NULL);
        m_isMciActive = false;
    }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// WAV FILE I/O HELPERS
// ─────────────────────────────────────────────────────────────────────────────
void AudioManager::saveWavFile(const QString &filePath, const std::vector<int16_t> &pcmData, int sampleRate) {
    QString tmpPath = filePath + ".tmp." + QString::number(QRandomGenerator::global()->generate());
    QFile file(tmpPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[AudioManager] Could not open file for writing";
        return;
    }

    int numSamples = static_cast<int>(pcmData.size());
    int dataBytes = numSamples * sizeof(int16_t);
    int totalBytes = 36 + dataBytes;

    QByteArray header;
    header.resize(44);
    char *hdr = header.data();

    memcpy(hdr + 0, "RIFF", 4);
    *reinterpret_cast<uint32_t*>(hdr + 4) = totalBytes;
    memcpy(hdr + 8, "WAVE", 4);

    memcpy(hdr + 12, "fmt ", 4);
    *reinterpret_cast<uint32_t*>(hdr + 16) = 16;
    *reinterpret_cast<uint16_t*>(hdr + 20) = 1;
    *reinterpret_cast<uint16_t*>(hdr + 22) = 1;
    *reinterpret_cast<uint32_t*>(hdr + 24) = sampleRate;
    *reinterpret_cast<uint32_t*>(hdr + 28) = sampleRate * 2;
    *reinterpret_cast<uint16_t*>(hdr + 32) = 2;
    *reinterpret_cast<uint16_t*>(hdr + 34) = 16;

    memcpy(hdr + 36, "data", 4);
    *reinterpret_cast<uint32_t*>(hdr + 40) = dataBytes;

    file.write(header);
    if (!pcmData.empty()) {
        file.write(reinterpret_cast<const char*>(pcmData.data()), dataBytes);
    }
    file.flush();
    file.close();

    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }
    if (!file.rename(filePath)) {
        QFile::remove(tmpPath);
    }
}

bool AudioManager::loadWavFile(const QString &filePath, std::vector<int16_t> &outPcm, int &outSampleRate) {
    QString cleanPath = filePath;
    if (cleanPath.startsWith("file:///")) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }

    QFile file(cleanPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.size() < 44) return false;
    const char *ptr = data.constData();
    if (memcmp(ptr, "RIFF", 4) != 0 || memcmp(ptr + 8, "WAVE", 4) != 0) {
        return false;
    }

    outSampleRate = *reinterpret_cast<const uint32_t*>(ptr + 24);
    uint16_t channels = *reinterpret_cast<const uint16_t*>(ptr + 22);
    uint16_t bitsPerSample = *reinterpret_cast<const uint16_t*>(ptr + 34);

    // Find "data" chunk
    int dataOffset = 12;
    int dataLength = 0;
    while (dataOffset + 8 <= data.size()) {
        if (memcmp(ptr + dataOffset, "data", 4) == 0) {
            dataLength = *reinterpret_cast<const uint32_t*>(ptr + dataOffset + 4);
            dataOffset += 8;
            break;
        }
        uint32_t chunkSize = *reinterpret_cast<const uint32_t*>(ptr + dataOffset + 4);
        dataOffset += 8 + chunkSize;
    }

    if (dataOffset >= data.size() || dataLength <= 0) return false;
    int availableBytes = std::min(dataLength, static_cast<int>(data.size() - dataOffset));

    if (bitsPerSample == 16) {
        int sampleCount = availableBytes / sizeof(int16_t);
        const int16_t *srcSamples = reinterpret_cast<const int16_t*>(ptr + dataOffset);
        outPcm.clear();
        if (channels == 1) {
            outPcm.assign(srcSamples, srcSamples + sampleCount);
        } else {
            outPcm.reserve(sampleCount / channels);
            for (int i = 0; i < sampleCount; i += channels) {
                outPcm.push_back(srcSamples[i]);
            }
        }
        return true;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// MICROPHONE RECORDING THREAD
// ─────────────────────────────────────────────────────────────────────────────
void AudioManager::startMicrophoneThread() {
    stopMicrophoneThread();
    m_recordThreadRunning = true;
    m_recordThread = std::thread(&AudioManager::microphoneCaptureWorker, this);
}

void AudioManager::stopMicrophoneThread() {
    m_recordThreadRunning = false;
    if (m_recordThread.joinable()) {
        if (m_recordThread.get_id() != std::this_thread::get_id()) {
            m_recordThread.join();
        }
    }
}

void AudioManager::microphoneCaptureWorker() {
#ifdef _WIN32
    HWAVEIN hWaveIn = NULL;
    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 16000;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    if (waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        qDebug() << "[AudioManager] Could not open microphone device";
        m_recordThreadRunning = false;
        QMetaObject::invokeMethod(this, &AudioManager::cancelRecording, Qt::QueuedConnection);
        return;
    }

    const int NUM_BUFFERS = 3;
    const int SAMPLES_PER_BUFFER = 800;
    const int BUFFER_SIZE = SAMPLES_PER_BUFFER * sizeof(int16_t);

    std::vector<int16_t> pcmBuffers[NUM_BUFFERS];
    WAVEHDR waveHeaders[NUM_BUFFERS];

    for (int i = 0; i < NUM_BUFFERS; ++i) {
        pcmBuffers[i].resize(SAMPLES_PER_BUFFER, 0);
        memset(&waveHeaders[i], 0, sizeof(WAVEHDR));
        waveHeaders[i].lpData = reinterpret_cast<LPSTR>(pcmBuffers[i].data());
        waveHeaders[i].dwBufferLength = BUFFER_SIZE;
        waveInPrepareHeader(hWaveIn, &waveHeaders[i], sizeof(WAVEHDR));
        waveInAddBuffer(hWaveIn, &waveHeaders[i], sizeof(WAVEHDR));
    }

    waveInStart(hWaveIn);

    int currentBuf = 0;
    while (m_recordThreadRunning && m_isRecording) {
        if (waveHeaders[currentBuf].dwFlags & WHDR_DONE) {
            const auto &buf = pcmBuffers[currentBuf];

            double sumSq = 0.0;
            for (int16_t s : buf) {
                sumSq += static_cast<double>(s) * s;
            }
            double rms = std::sqrt(sumSq / buf.size()) / 32768.0;
            double level = qBound(0.05, rms * 5.0, 1.0);

            {
                std::lock_guard<std::mutex> lock(m_recordMutex);
                m_recordedPcmAudio.insert(m_recordedPcmAudio.end(), buf.begin(), buf.end());
                m_recordedAmplitudes.push_back(level);
            }

            waveHeaders[currentBuf].dwFlags &= ~WHDR_DONE;
            waveInAddBuffer(hWaveIn, &waveHeaders[currentBuf], sizeof(WAVEHDR));
            currentBuf = (currentBuf + 1) % NUM_BUFFERS;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    waveInStop(hWaveIn);
    waveInReset(hWaveIn);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        waveInUnprepareHeader(hWaveIn, &waveHeaders[i], sizeof(WAVEHDR));
    }
    waveInClose(hWaveIn);
    m_recordThreadRunning = false;
#else
    auto &pulse = PulseAudioHelper::instance();
    pa_buffer_attr ba;
    ba.maxlength = static_cast<uint32_t>(-1);
    ba.tlength = static_cast<uint32_t>(-1);
    ba.prebuf = static_cast<uint32_t>(-1);
    ba.minreq = static_cast<uint32_t>(-1);
    ba.fragsize = 1600;

    pa_simple *s = pulse.createStream(2, 16000, 1, &ba);
    if (!s) {
        qDebug() << "[AudioManager] Could not open microphone device, PulseAudio stream creation failed";
        m_recordThreadRunning = false;
        QMetaObject::invokeMethod(this, &AudioManager::cancelRecording, Qt::QueuedConnection);
        return;
    }

    const size_t SAMPLES_PER_BUFFER = 800;
    std::vector<int16_t> buf(SAMPLES_PER_BUFFER, 0);

    while (m_recordThreadRunning && m_isRecording) {
        int r = pulse.read(s, buf.data(), buf.size() * sizeof(int16_t));
        if (r == 0) {
            double sumSq = 0.0;
            for (int16_t val : buf) {
                sumSq += static_cast<double>(val) * val;
            }
            double rms = std::sqrt(sumSq / buf.size()) / 32768.0;
            double level = qBound(0.05, rms * 5.0, 1.0);

            {
                std::lock_guard<std::mutex> lock(m_recordMutex);
                m_recordedPcmAudio.insert(m_recordedPcmAudio.end(), buf.begin(), buf.end());
                m_recordedAmplitudes.push_back(level);
            }
        } else {
            break;
        }
    }

    pulse.freeStream(s);
    m_recordThreadRunning = false;
#endif
}

void AudioManager::startRecording() {
    if (m_isRecording) return;

    if (m_isPlaying) {
        pauseAudio();
    }

    m_isRecording = true;
    m_recordingDuration = 0;
    m_recordElapsedTimer.restart();
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        m_recordedPcmAudio.clear();
        m_recordedAmplitudes.clear();
    }
    m_liveWaveform.clear();
    for (int i = 0; i < 24; ++i) {
        m_liveWaveform.append(0.05);
    }

    emit recordingStateChanged();
    emit recordingDurationChanged();
    emit liveWaveformChanged();

    startMicrophoneThread();
    m_recordTimer->start();
}

void AudioManager::onRecordTimerTick() {
    if (!m_isRecording) return;

    double level = 0.05;
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        if (!m_recordedAmplitudes.empty()) {
            level = m_recordedAmplitudes.back();
        }
    }

    m_liveWaveform.append(level);
    if (m_liveWaveform.size() > 24) {
        m_liveWaveform.removeFirst();
    }
    emit liveWaveformChanged();

    int pcmSecs = 0;
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        pcmSecs = static_cast<int>(m_recordedPcmAudio.size() / 16000);
    }
    int elapsedSecs = std::max(pcmSecs, static_cast<int>(m_recordElapsedTimer.elapsed() / 1000));
    if (elapsedSecs > m_recordingDuration) {
        m_recordingDuration = elapsedSecs;
        emit recordingDurationChanged();
    }
}

QVariantMap AudioManager::stopRecording() {
    if (!m_isRecording) return {};

    m_recordTimer->stop();
    m_isRecording = false;
    stopMicrophoneThread();
    emit recordingStateChanged();

    playUiBeep(660, 60);

    std::vector<double> amps;
    std::vector<int16_t> pcm;
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        amps = m_recordedAmplitudes;
        pcm = m_recordedPcmAudio;
    }

    if (pcm.empty()) {
        m_recordingDuration = 0;
        {
            std::lock_guard<std::mutex> lock(m_recordMutex);
            m_recordedAmplitudes.clear();
            m_recordedPcmAudio.clear();
        }
        emit recordingDurationChanged();
        return {};
    }

    double sampleSecs = static_cast<double>(pcm.size()) / 16000.0;
    int finalSecs = std::max(1, static_cast<int>(std::round(sampleSecs)));

    QVariantList sampledWaveform;
    int targetBars = 28;

    if (amps.empty()) {
        for (int i = 0; i < targetBars; ++i) {
            sampledWaveform.append(0.05);
        }
    } else if (amps.size() <= targetBars) {
        for (double a : amps) sampledWaveform.append(a);
        while (sampledWaveform.size() < targetBars) {
            sampledWaveform.append(0.05);
        }
    } else {
        double step = static_cast<double>(amps.size()) / targetBars;
        for (int i = 0; i < targetBars; ++i) {
            int startIdx = static_cast<int>(i * step);
            int endIdx = static_cast<int>((i + 1) * step);
            endIdx = std::clamp(endIdx, startIdx + 1, static_cast<int>(amps.size()));
            double peak = 0.05;
            for (int j = startIdx; j < endIdx; ++j) {
                if (amps[j] > peak) peak = amps[j];
            }
            sampledWaveform.append(peak);
        }
    }

    QString sUrl, sUser;
    {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        sUrl = m_serverUrl;
        sUser = m_username;
    }

    if (sUrl.isEmpty() || sUser.isEmpty()) {
        qWarning() << "[AudioManager] Cannot save voice note: No authenticated context.";
        m_recordingDuration = 0;
        {
            std::lock_guard<std::mutex> lock(m_recordMutex);
            m_recordedAmplitudes.clear();
            m_recordedPcmAudio.clear();
        }
        emit recordingDurationChanged();
        return {};
    }

    QString voiceDir = NeoNect::Storage::StoragePathResolver::mediaVoiceDirectory(sUrl, sUser);
    QDir().mkpath(voiceDir);

    QString fileName = "voice_note_" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".wav";
    QString fullPath = voiceDir + "/" + fileName;

    saveWavFile(fullPath, pcm, 16000);

    QVariantMap result;
    result["audioUrl"] = QUrl::fromLocalFile(fullPath).toString();
    result["duration"] = finalSecs;
    result["waveform"] = sampledWaveform;
    result["fileSize"] = static_cast<int>(pcm.size() * sizeof(int16_t) + 44);

    m_recordingDuration = 0;
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        m_recordedAmplitudes.clear();
        m_recordedPcmAudio.clear();
    }
    emit recordingDurationChanged();

    return result;
}

void AudioManager::cancelRecording() {
    if (!m_isRecording) return;

    m_recordTimer->stop();
    m_isRecording = false;
    stopMicrophoneThread();
    m_recordingDuration = 0;
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        m_recordedAmplitudes.clear();
        m_recordedPcmAudio.clear();
    }
    emit recordingStateChanged();
    emit recordingDurationChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// AUDIO PLAYBACK THREAD
// ─────────────────────────────────────────────────────────────────────────────
void AudioManager::startAudioPlaybackThread() {
    stopAudioPlaybackThread();
    m_audioPlaying = true;
    m_audioThreadRunning = true;
    m_audioThread = std::thread(&AudioManager::audioOutputWorker, this);
}

void AudioManager::stopAudioPlaybackThread() {
    m_audioPlaying = false;
    m_audioThreadRunning = false;
    if (m_audioThread.joinable()) {
        if (m_audioThread.get_id() != std::this_thread::get_id()) {
            m_audioThread.join();
        }
    }
}

void AudioManager::audioOutputWorker() {
#ifdef _WIN32
    int sampleRate = m_playbackSampleRate.load();
    if (sampleRate <= 0) sampleRate = 16000;

    HWAVEOUT hWaveOut = NULL;
    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = sampleRate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        qDebug() << "[AudioManager] Could not open waveOut playback device";
        m_audioThreadRunning = false;
        m_audioPlaying = false;
        QMetaObject::invokeMethod(this, &AudioManager::handlePlaybackFinished, Qt::QueuedConnection);
        return;
    }

    const int NUM_BUFFERS = 3;
    const int SAMPLES_PER_BUFFER = 1600;
    const int BUFFER_SIZE = SAMPLES_PER_BUFFER * sizeof(int16_t);

    std::vector<int16_t> pcmBuffers[NUM_BUFFERS];
    WAVEHDR waveHeaders[NUM_BUFFERS];

    for (int i = 0; i < NUM_BUFFERS; ++i) {
        pcmBuffers[i].resize(SAMPLES_PER_BUFFER, 0);
        memset(&waveHeaders[i], 0, sizeof(WAVEHDR));
        waveHeaders[i].lpData = reinterpret_cast<LPSTR>(pcmBuffers[i].data());
        waveHeaders[i].dwBufferLength = BUFFER_SIZE;
        waveOutPrepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
        waveHeaders[i].dwFlags |= WHDR_DONE;
    }

    int currentBuf = 0;
    double samplePhase1 = 0.0;
    double samplePhase2 = 0.0;
    double samplePhase3 = 0.0;
    int songTick = 0;

    const double chordProgression[4][4] = {
        { 261.63, 329.63, 392.00, 523.25 },
        { 196.00, 246.94, 293.66, 392.00 },
        { 220.00, 261.63, 329.63, 440.00 },
        { 174.61, 220.00, 261.63, 349.23 }
    };

    while (m_audioThreadRunning && m_audioPlaying) {
        int waitCount = 0;
        while (!(waveHeaders[currentBuf].dwFlags & WHDR_DONE) && waitCount++ < 25) {
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
            if (!m_audioPlaying || !m_audioThreadRunning) break;
        }

        if (!m_audioPlaying || !m_audioThreadRunning) break;

        int type = m_audioType.load();
        double speed = m_playbackSpeedAtomic.load();
        double vol = m_volumeAtomic.load();
        auto &buf = pcmBuffers[currentBuf];

        if (type == 2) {
            size_t sampleIdx = 0;
            size_t totalSamples = 0;
            {
                std::lock_guard<std::mutex> lock(m_audioMutex);
                sampleIdx = m_playbackFileSampleIndex.load();
                totalSamples = m_playbackFilePcm.size();

                for (int i = 0; i < SAMPLES_PER_BUFFER; ++i) {
                    if (sampleIdx < totalSamples) {
                        double s = static_cast<double>(m_playbackFilePcm[sampleIdx]) * vol;
                        buf[i] = static_cast<int16_t>(qBound(-32767.0, s, 32767.0));
                        sampleIdx += (speed >= 1.9 ? 2 : (speed >= 1.4 ? (i % 2 == 0 ? 2 : 1) : 1));
                    } else {
                        buf[i] = 0;
                    }
                }
                m_playbackFileSampleIndex.store(sampleIdx);
            }

            waveHeaders[currentBuf].dwFlags &= ~WHDR_DONE;
            waveOutWrite(hWaveOut, &waveHeaders[currentBuf], sizeof(WAVEHDR));
            currentBuf = (currentBuf + 1) % NUM_BUFFERS;

            if (sampleIdx >= totalSamples && totalSamples > 0) {
                for (int i = 0; i < NUM_BUFFERS; ++i) {
                    int drainWait = 0;
                    while (!(waveHeaders[i].dwFlags & WHDR_DONE) && drainWait++ < 50) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    }
                }
                m_audioPlaying = false;
                QMetaObject::invokeMethod(this, &AudioManager::handlePlaybackFinished, Qt::QueuedConnection);
                break;
            }
        } else if (type == 0) {
            for (int i = 0; i < SAMPLES_PER_BUFFER; ++i) {
                songTick++;
                double t = static_cast<double>(songTick) / wfx.nSamplesPerSec;

                double syllable = std::max(0.0, std::sin(t * 3.5 * 2.0 * M_PI * speed));
                double vocalTremolo = 0.85 + 0.15 * std::sin(t * 7.0 * 2.0 * M_PI);

                double pitchInflection = 220.0 + 35.0 * std::sin(t * 1.8 * 2.0 * M_PI * speed);
                double freq1 = pitchInflection * speed;
                double freq2 = pitchInflection * 2.0 * speed;
                double freq3 = pitchInflection * 3.0 * speed;

                samplePhase1 += (freq1 * 2.0 * M_PI) / wfx.nSamplesPerSec;
                samplePhase2 += (freq2 * 2.0 * M_PI) / wfx.nSamplesPerSec;
                samplePhase3 += (freq3 * 2.0 * M_PI) / wfx.nSamplesPerSec;

                double v1 = std::sin(samplePhase1) * 0.55;
                double v2 = std::sin(samplePhase2) * 0.30;
                double v3 = std::sin(samplePhase3) * 0.15;

                double sampleVal = (v1 + v2 + v3) * syllable * vocalTremolo * 18000.0 * vol;
                buf[i] = static_cast<int16_t>(qBound(-32000.0, sampleVal, 32000.0));
            }
            waveHeaders[currentBuf].dwFlags &= ~WHDR_DONE;
            waveOutWrite(hWaveOut, &waveHeaders[currentBuf], sizeof(WAVEHDR));
            currentBuf = (currentBuf + 1) % NUM_BUFFERS;
        } else if (type == 1) {
            for (int i = 0; i < SAMPLES_PER_BUFFER; ++i) {
                songTick++;
                double t = static_cast<double>(songTick) / wfx.nSamplesPerSec;

                int chordIndex = static_cast<int>(t * speed / 2.0) % 4;
                int noteIndex = static_cast<int>(t * speed * 8.0) % 4;

                double noteFreq = chordProgression[chordIndex][noteIndex] * speed;
                double bassFreq = (chordProgression[chordIndex][0] / 2.0) * speed;

                samplePhase1 += (noteFreq * 2.0 * M_PI) / wfx.nSamplesPerSec;
                samplePhase2 += (bassFreq * 2.0 * M_PI) / wfx.nSamplesPerSec;
                samplePhase3 += (noteFreq * 1.5 * 2.0 * M_PI) / wfx.nSamplesPerSec;

                double pluck = std::pow(1.0 - std::fmod(t * speed * 8.0, 1.0), 1.6);
                double lead = std::sin(samplePhase1) * 0.5 + std::sin(samplePhase3) * 0.25;
                double bass = std::sin(samplePhase2) * 0.45;

                double sampleVal = (lead * pluck + bass) * 16000.0 * vol;
                buf[i] = static_cast<int16_t>(qBound(-32000.0, sampleVal, 32000.0));
            }
            waveHeaders[currentBuf].dwFlags &= ~WHDR_DONE;
            waveOutWrite(hWaveOut, &waveHeaders[currentBuf], sizeof(WAVEHDR));
            currentBuf = (currentBuf + 1) % NUM_BUFFERS;
        }
    }

    waveOutReset(hWaveOut);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        waveOutUnprepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
    }
    waveOutClose(hWaveOut);
    m_audioThreadRunning = false;
#else
    int sampleRate = m_playbackSampleRate.load();
    if (sampleRate <= 0) sampleRate = 16000;

    auto &pulse = PulseAudioHelper::instance();
    pa_buffer_attr ba;
    ba.maxlength = static_cast<uint32_t>(-1);
    uint32_t bytesPerSec = static_cast<uint32_t>(sampleRate) * sizeof(int16_t);
    ba.tlength = bytesPerSec / 10;
    ba.prebuf = static_cast<uint32_t>(-1);
    ba.minreq = bytesPerSec / 20;
    ba.fragsize = static_cast<uint32_t>(-1);

    pa_simple *s = pulse.createStream(1, sampleRate, 1, &ba);
    if (!s) {
        qDebug() << "[AudioManager] Could not open PulseAudio playback stream";
        m_audioThreadRunning = false;
        m_audioPlaying = false;
        QMetaObject::invokeMethod(this, &AudioManager::handlePlaybackFinished, Qt::QueuedConnection);
        return;
    }

    const int SAMPLES_PER_BUFFER = 800;
    std::vector<int16_t> buf(SAMPLES_PER_BUFFER, 0);

    while (m_audioThreadRunning && m_audioPlaying) {
        int type = m_audioType.load();
        double speed = m_playbackSpeedAtomic.load();
        double vol = m_volumeAtomic.load();

        if (type == 2) {
            size_t sampleIdx = 0;
            size_t totalSamples = 0;
            {
                std::lock_guard<std::mutex> lock(m_audioMutex);
                sampleIdx = m_playbackFileSampleIndex.load();
                totalSamples = m_playbackFilePcm.size();
                for (int i = 0; i < SAMPLES_PER_BUFFER; ++i) {
                    if (sampleIdx < totalSamples) {
                        double val = static_cast<double>(m_playbackFilePcm[sampleIdx]) * vol;
                        buf[i] = static_cast<int16_t>(qBound(-32767.0, val, 32767.0));
                        sampleIdx += (speed >= 1.9 ? 2 : (speed >= 1.4 ? (i % 2 == 0 ? 2 : 1) : 1));
                    } else {
                        buf[i] = 0;
                    }
                }
                m_playbackFileSampleIndex.store(sampleIdx);
            }

            if (pulse.write(s, buf.data(), buf.size() * sizeof(int16_t)) < 0) {
                break;
            }

            if (sampleIdx >= totalSamples && totalSamples > 0) {
                pulse.drain(s);
                m_audioPlaying = false;
                QMetaObject::invokeMethod(this, &AudioManager::handlePlaybackFinished, Qt::QueuedConnection);
                break;
            }
        } else {
            for (int i = 0; i < SAMPLES_PER_BUFFER; ++i) {
                buf[i] = 0;
            }
            if (pulse.write(s, buf.data(), buf.size() * sizeof(int16_t)) < 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    pulse.drain(s);
    pulse.freeStream(s);
    m_audioThreadRunning = false;
#endif
}

void AudioManager::playAudio(const QString &messageId, const QString &audioUrl, int durationSecs) {
    if (messageId.isEmpty()) return;

    QString sUrl, sUser;
    {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        sUrl = m_serverUrl;
        sUser = m_username;
    }

    if (sUrl.isEmpty() || sUser.isEmpty()) {
        qWarning() << "[AudioManager] Cannot play audio: No authenticated context.";
        return;
    }

    QString localPath = audioUrl;
    if (localPath.startsWith("file:///")) {
        localPath = QUrl(localPath).toLocalFile();
    } else if (localPath.startsWith("file://")) {
        localPath = localPath.mid(7);
    }

    if (m_currentPlayingId != messageId) {
        QString mediaDir = NeoNect::Storage::StoragePathResolver::mediaDirectory(sUrl, sUser);
        QString canonMediaDir = QDir(mediaDir).canonicalPath();
        QString baseDir = NeoNect::Storage::StoragePathResolver::cacheDirectory(sUrl, sUser);
        QString expectedRoot = QDir(baseDir + "/voice_notes").canonicalPath();
        QString accountDir = NeoNect::Storage::StoragePathResolver::accountDirectory(sUrl, sUser);
        QString canonAccountDir = QDir(accountDir).canonicalPath();
        QString spoolDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/spool";
        QString canonSpoolDir = QDir(spoolDir).canonicalPath();

        QFileInfo fi(localPath);
        if (!fi.exists()) {
            qWarning() << "[AudioManager] Audio file does not exist";
            return;
        }

        QString canonPath = fi.canonicalFilePath();
        bool isAllowed = false;
        if (!canonMediaDir.isEmpty() && (canonPath.startsWith(canonMediaDir + "/") || canonPath == canonMediaDir)) {
            isAllowed = true;
        } else if (!expectedRoot.isEmpty() && (canonPath.startsWith(expectedRoot + "/") || canonPath == expectedRoot)) {
            isAllowed = true;
        } else if (!canonAccountDir.isEmpty() && (canonPath.startsWith(canonAccountDir + "/") || canonPath == canonAccountDir)) {
            isAllowed = true;
        } else if (!canonSpoolDir.isEmpty() && (canonPath.startsWith(canonSpoolDir + "/") || canonPath == canonSpoolDir)) {
            isAllowed = true;
        }

        if (!isAllowed) {
            qWarning() << "[AudioManager] Cannot play audio outside of authenticated storage boundary.";
            return;
        }

        stopMci();
        stopAudioPlaybackThread();

        m_currentPlayingId = messageId;
        m_currentAudioUrl = audioUrl;
        m_currentPosition = 0;
        m_totalDuration = (durationSecs > 0 ? durationSecs : 10) * 1000;
        m_playbackProgress = 0.0;
        m_playbackFileSampleIndex = 0;

        if (QFile::exists(localPath)) {
            if (playViaMci(localPath)) {
                m_audioType = 3;
            } else {
                std::vector<int16_t> loadedPcm;
                int sampleRate = 16000;
                if (loadWavFile(localPath, loadedPcm, sampleRate)) {
                    std::lock_guard<std::mutex> lock(m_audioMutex);
                    m_playbackFilePcm = std::move(loadedPcm);
                    m_playbackSampleRate.store(sampleRate > 0 ? sampleRate : 16000);
                    m_audioType = 2;
                    if (!m_playbackFilePcm.empty() && sampleRate > 0) {
                        m_totalDuration = static_cast<int>((m_playbackFilePcm.size() * 1000) / sampleRate);
                    }
                    startAudioPlaybackThread();
                } else {
                    bool isMusic = (audioUrl.contains(".mp3") || audioUrl.contains(".ogg") || audioUrl.contains(".flac") || durationSecs > 30);
                    m_audioType = isMusic ? 1 : 0;
                    startAudioPlaybackThread();
                }
            }
        } else {
            bool isMusic = (audioUrl.contains(".mp3") || audioUrl.contains(".ogg") || audioUrl.contains(".flac") || durationSecs > 30);
            m_audioType = isMusic ? 1 : 0;
            startAudioPlaybackThread();
        }

        emit playbackDurationChanged();
        emit playbackProgressChanged();
    } else {
        if (m_isMciActive) {
            resumeMci();
        } else {
            m_audioPlaying = true;
            startAudioPlaybackThread();
        }
    }

    m_playbackSpeedAtomic = m_playbackSpeed;
    m_volumeAtomic = m_isMuted ? 0.0 : m_volume;
    m_isPlaying = true;
    emit playbackStateChanged();
    m_playbackTimer->start();
}

void AudioManager::pauseAudio() {
    if (!m_isPlaying) return;

    if (m_isMciActive) {
        pauseMci();
    }

    m_audioPlaying = false;
    m_isPlaying = false;
    m_playbackTimer->stop();
    emit playbackStateChanged();
    stopAudioPlaybackThread();
}

void AudioManager::togglePlayPause(const QString &messageId, const QString &audioUrl, int durationSecs) {
    if (m_currentPlayingId == messageId && m_isPlaying) {
        pauseAudio();
    } else {
        playAudio(messageId, audioUrl, durationSecs);
    }
}

void AudioManager::seek(const QString &messageId, qreal progress) {
    qreal clamped = qBound(0.0, progress, 1.0);
    if (m_currentPlayingId == messageId) {
        m_playbackProgress = clamped;
        m_currentPosition = static_cast<int>(m_totalDuration * clamped);
        if (m_isMciActive) {
            seekMci(m_currentPosition);
        } else if (m_audioType == 2) {
            std::lock_guard<std::mutex> lock(m_audioMutex);
            m_playbackFileSampleIndex = static_cast<size_t>(m_playbackFilePcm.size() * clamped);
        }
        emit playbackProgressChanged();
    }
}

void AudioManager::setPlaybackSpeed(qreal speed) {
    if (qFuzzyCompare(m_playbackSpeed, speed)) return;

    if (speed <= 1.0) m_playbackSpeed = 1.0;
    else if (speed <= 1.5) m_playbackSpeed = 1.5;
    else m_playbackSpeed = 2.0;

    m_playbackSpeedAtomic = m_playbackSpeed;
    emit playbackSpeedChanged();
}

void AudioManager::onPlaybackTimerTick() {
    if (!m_isPlaying) return;

    if (m_isMciActive) {
#ifdef _WIN32
        wchar_t posBuf[128];
        if (mciSendStringW(L"status neonect_audio position", posBuf, 128, NULL) == 0) {
            int pos = QString::fromWCharArray(posBuf).toInt();
            if (pos >= 0) {
                m_currentPosition = pos;
            }
        }
#endif
    } else {
        if (m_audioType == 2) {
            int rate = m_playbackSampleRate.load();
            if (rate > 0) {
                m_currentPosition = static_cast<int>((m_playbackFileSampleIndex.load() * 1000) / rate);
            } else {
                int increment = static_cast<int>(50 * m_playbackSpeed);
                m_currentPosition += increment;
            }
        } else {
            int increment = static_cast<int>(50 * m_playbackSpeed);
            m_currentPosition += increment;
        }
    }

    if (m_totalDuration > 0) {
        m_playbackProgress = static_cast<qreal>(m_currentPosition) / m_totalDuration;
    } else {
        m_playbackProgress = 0.0;
    }

    if (m_currentPosition >= m_totalDuration) {
        if (m_audioType != 2) {
            handlePlaybackFinished();
            return;
        } else {
            m_currentPosition = m_totalDuration;
            m_playbackProgress = 1.0;
            if (!m_audioThreadRunning.load()) {
                handlePlaybackFinished();
                return;
            }
        }
    }

    emit playbackProgressChanged();
}

void AudioManager::handlePlaybackFinished() {
    if (!m_isPlaying && m_currentPlayingId.isEmpty()) return;
    m_currentPosition = m_totalDuration;
    m_playbackProgress = 1.0;
    m_isPlaying = false;
    m_audioPlaying = false;
    m_playbackTimer->stop();
    stopMci();
    stopAudioPlaybackThread();
    QString finishedId = m_currentPlayingId;
    m_currentPlayingId.clear();
    emit playbackProgressChanged();
    emit playbackStateChanged();
    emit playbackFinished(finishedId);
}
