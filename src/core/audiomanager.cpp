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
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

AudioManager* AudioManager::instance() {
    static AudioManager _instance;
    return &_instance;
}

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
    Q_UNUSED(freqHz);
    Q_UNUSED(durationMs);
#endif
}

void AudioManager::openMediaFile(const QString &mediaUrl) {
    if (mediaUrl.isEmpty()) return;
    QString cleanPath = mediaUrl;
    if (cleanPath.startsWith("file:///")) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(cleanPath));
}

void AudioManager::setVolume(qreal vol) {
    vol = qBound(0.0, vol, 1.0);
    if (qFuzzyCompare(m_volume, vol)) return;
    m_volume = vol;
    m_isMuted = (m_volume == 0.0);
    m_volumeAtomic.store(m_isMuted ? 0.0 : m_volume);

#ifdef _WIN32
    if (m_isMciActive) {
        int mciVol = static_cast<int>(m_volume * 1000);
        std::wstring volCmd = L"setaudio avila_audio volume to " + std::to_wstring(mciVol);
        mciSendStringW(volCmd.c_str(), NULL, 0, NULL);
    }
#endif

    emit volumeChanged();
    emit isMutedChanged();
}

void AudioManager::toggleMute() {
    if (m_isMuted) {
        m_isMuted = false;
        m_volume = (m_preMuteVolume > 0.0 ? m_preMuteVolume : 1.0);
    } else {
        m_preMuteVolume = (m_volume > 0.0 ? m_volume : 1.0);
        m_isMuted = true;
        m_volume = 0.0;
    }
    m_volumeAtomic.store(m_isMuted ? 0.0 : m_volume);

#ifdef _WIN32
    if (m_isMciActive) {
        int mciVol = static_cast<int>(m_volume * 1000);
        std::wstring volCmd = L"setaudio avila_audio volume to " + std::to_wstring(mciVol);
        mciSendStringW(volCmd.c_str(), NULL, 0, NULL);
    }
#endif

    emit volumeChanged();
    emit isMutedChanged();
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

    std::wstring openCmd = L"open \"" + wpath + L"\" type mpegvideo alias avila_audio";
    MCIERROR err = mciSendStringW(openCmd.c_str(), NULL, 0, NULL);
    if (err != 0) {
        openCmd = L"open \"" + wpath + L"\" alias avila_audio";
        err = mciSendStringW(openCmd.c_str(), NULL, 0, NULL);
        if (err != 0) return false;
    }

    mciSendStringW(L"set avila_audio time format milliseconds", NULL, 0, NULL);

    int mciVol = static_cast<int>((m_isMuted ? 0.0 : m_volume) * 1000);
    std::wstring volCmd = L"setaudio avila_audio volume to " + std::to_wstring(mciVol);
    mciSendStringW(volCmd.c_str(), NULL, 0, NULL);

    wchar_t lenBuf[128];
    if (mciSendStringW(L"status avila_audio length", lenBuf, 128, NULL) == 0) {
        int len = QString::fromWCharArray(lenBuf).toInt();
        if (len > 0) {
            m_totalDuration = len;
            emit playbackDurationChanged();
        }
    }

    mciSendStringW(L"play avila_audio", NULL, 0, NULL);
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
        mciSendStringW(L"pause avila_audio", NULL, 0, NULL);
    }
#endif
}

void AudioManager::resumeMci() {
#ifdef _WIN32
    if (m_isMciActive) {
        mciSendStringW(L"resume avila_audio", NULL, 0, NULL);
    }
#endif
}

void AudioManager::seekMci(int positionMs) {
#ifdef _WIN32
    if (m_isMciActive) {
        std::wstring seekCmd = L"play avila_audio from " + std::to_wstring(positionMs);
        mciSendStringW(seekCmd.c_str(), NULL, 0, NULL);
    }
#endif
}

void AudioManager::stopMci() {
#ifdef _WIN32
    if (m_isMciActive) {
        mciSendStringW(L"stop avila_audio", NULL, 0, NULL);
        mciSendStringW(L"close avila_audio", NULL, 0, NULL);
        m_isMciActive = false;
    }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// WAV FILE I/O HELPERS
// ─────────────────────────────────────────────────────────────────────────────
void AudioManager::saveWavFile(const QString &filePath, const std::vector<int16_t> &pcmData, int sampleRate) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[AudioManager] Could not open file for writing:" << filePath;
        return;
    }

    int numSamples = static_cast<int>(pcmData.size());
    int dataBytes = numSamples * sizeof(int16_t);
    int totalBytes = 36 + dataBytes;

    QByteArray header;
    header.resize(44);
    char *hdr = header.data();

    // RIFF chunk descriptor
    memcpy(hdr + 0, "RIFF", 4);
    *reinterpret_cast<uint32_t*>(hdr + 4) = totalBytes;
    memcpy(hdr + 8, "WAVE", 4);

    // fmt sub-chunk
    memcpy(hdr + 12, "fmt ", 4);
    *reinterpret_cast<uint32_t*>(hdr + 16) = 16; // PCM
    *reinterpret_cast<uint16_t*>(hdr + 20) = 1;  // AudioFormat 1 = PCM
    *reinterpret_cast<uint16_t*>(hdr + 22) = 1;  // NumChannels 1 = Mono
    *reinterpret_cast<uint32_t*>(hdr + 24) = sampleRate; // SampleRate
    *reinterpret_cast<uint32_t*>(hdr + 28) = sampleRate * 2; // ByteRate
    *reinterpret_cast<uint16_t*>(hdr + 32) = 2;  // BlockAlign
    *reinterpret_cast<uint16_t*>(hdr + 34) = 16; // BitsPerSample

    // data sub-chunk
    memcpy(hdr + 36, "data", 4);
    *reinterpret_cast<uint32_t*>(hdr + 40) = dataBytes;

    file.write(header);
    if (!pcmData.empty()) {
        file.write(reinterpret_cast<const char*>(pcmData.data()), dataBytes);
    }
    file.close();
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
    m_recordThreadRunning = true;
    m_recordThread = std::thread(&AudioManager::microphoneCaptureWorker, this);
}

void AudioManager::stopMicrophoneThread() {
    m_recordThreadRunning = false;
    if (m_recordThread.joinable()) {
        m_recordThread.join();
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
        qDebug() << "[AudioManager] Could not open microphone device, falling back to synthesizer";
        return;
    }

    const int NUM_BUFFERS = 3;
    const int SAMPLES_PER_BUFFER = 800; // 50ms
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
            double level = qBound(0.1, rms * 5.0, 1.0);

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
#endif
}

void AudioManager::startRecording() {
    if (m_isRecording) return;

    if (m_isPlaying) {
        pauseAudio();
    }

    playUiBeep(880, 70); // Start notification

    m_isRecording = true;
    m_recordingDuration = 0;
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        m_recordedPcmAudio.clear();
        m_recordedAmplitudes.clear();
    }
    m_liveWaveform.clear();
    for (int i = 0; i < 24; ++i) {
        m_liveWaveform.append(0.15);
    }

    emit recordingStateChanged();
    emit recordingDurationChanged();
    emit liveWaveformChanged();

    startMicrophoneThread();
    m_recordTimer->start();
}

void AudioManager::onRecordTimerTick() {
    if (!m_isRecording) return;

    double level = 0.2;
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        if (!m_recordedAmplitudes.empty()) {
            level = m_recordedAmplitudes.back();
        } else {
            level = 0.2 + (QRandomGenerator::global()->bounded(100) / 100.0) * 0.4;
        }
    }

    m_liveWaveform.append(level);
    if (m_liveWaveform.size() > 24) {
        m_liveWaveform.removeFirst();
    }
    emit liveWaveformChanged();

    int elapsedSecs = static_cast<int>(m_recordedPcmAudio.size() / 16000);
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

    int finalSecs = std::max(1, m_recordingDuration);

    QVariantList sampledWaveform;
    int targetBars = 28;

    std::vector<double> amps;
    std::vector<int16_t> pcm;
    {
        std::lock_guard<std::mutex> lock(m_recordMutex);
        amps = m_recordedAmplitudes;
        pcm = m_recordedPcmAudio;
    }

    if (amps.empty()) {
        for (int i = 0; i < targetBars; ++i) {
            sampledWaveform.append(0.2 + (i % 3) * 0.2);
        }
    } else if (amps.size() <= targetBars) {
        for (double a : amps) sampledWaveform.append(a);
        while (sampledWaveform.size() < targetBars) {
            sampledWaveform.append(0.2);
        }
    } else {
        double step = static_cast<double>(amps.size()) / targetBars;
        for (int i = 0; i < targetBars; ++i) {
            int idx = static_cast<int>(i * step);
            idx = qBound(0, idx, static_cast<int>(amps.size() - 1));
            sampledWaveform.append(amps[idx]);
        }
    }

    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty()) {
        baseDir = QDir::currentPath() + "/cache";
    }
    QDir().mkpath(baseDir + "/voice_notes");

    QString fileName = "voice_note_" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".wav";
    QString fullPath = baseDir + "/voice_notes/" + fileName;

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
    if (m_audioThreadRunning.exchange(true)) return;
    if (m_audioThread.joinable()) {
        m_audioThread.join();
    }
    m_audioThread = std::thread(&AudioManager::audioOutputWorker, this);
}

void AudioManager::stopAudioPlaybackThread() {
    m_audioPlaying = false;
    m_audioThreadRunning = false;
    if (m_audioThread.joinable()) {
        m_audioThread.join();
    }
}

void AudioManager::audioOutputWorker() {
#ifdef _WIN32
    HWAVEOUT hWaveOut = NULL;
    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 16000;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        qDebug() << "[AudioManager] Could not open waveOut playback device";
        m_audioThreadRunning = false;
        return;
    }

    const int NUM_BUFFERS = 3;
    const int SAMPLES_PER_BUFFER = 1600; // 100ms
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
        { 261.63, 329.63, 392.00, 523.25 }, // C Major
        { 196.00, 246.94, 293.66, 392.00 }, // G Major
        { 220.00, 261.63, 329.63, 440.00 }, // A Minor
        { 174.61, 220.00, 261.63, 349.23 }  // F Major
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
            std::lock_guard<std::mutex> lock(m_audioMutex);
            size_t sampleIdx = m_playbackFileSampleIndex.load();
            size_t totalSamples = m_playbackFilePcm.size();

            for (int i = 0; i < SAMPLES_PER_BUFFER; ++i) {
                if (sampleIdx < totalSamples) {
                    double s = static_cast<double>(m_playbackFilePcm[sampleIdx]) * vol;
                    buf[i] = static_cast<int16_t>(qBound(-32767.0, s, 32767.0));
                    sampleIdx += (speed >= 1.9 ? 2 : 1);
                } else {
                    buf[i] = 0;
                }
            }
            m_playbackFileSampleIndex.store(sampleIdx);
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
        }

        waveHeaders[currentBuf].dwFlags &= ~WHDR_DONE;
        waveOutWrite(hWaveOut, &waveHeaders[currentBuf], sizeof(WAVEHDR));
        currentBuf = (currentBuf + 1) % NUM_BUFFERS;
    }

    waveOutReset(hWaveOut);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        waveOutUnprepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
    }
    waveOutClose(hWaveOut);
    m_audioThreadRunning = false;
#endif
}

void AudioManager::playAudio(const QString &messageId, const QString &audioUrl, int durationSecs) {
    if (messageId.isEmpty()) return;

    if (m_currentPlayingId != messageId) {
        stopMci();
        stopAudioPlaybackThread();

        m_currentPlayingId = messageId;
        m_currentAudioUrl = audioUrl;
        m_currentPosition = 0;
        m_totalDuration = (durationSecs > 0 ? durationSecs : 10) * 1000;
        m_playbackProgress = 0.0;
        m_playbackFileSampleIndex = 0;

        QString localPath = audioUrl;
        if (localPath.startsWith("file:///")) {
            localPath = QUrl(localPath).toLocalFile();
        }

        if (QFile::exists(localPath)) {
            if (playViaMci(localPath)) {
                m_audioType = 3;
            } else {
                std::vector<int16_t> loadedPcm;
                int sampleRate = 16000;
                if (loadWavFile(localPath, loadedPcm, sampleRate)) {
                    std::lock_guard<std::mutex> lock(m_audioMutex);
                    m_playbackFilePcm = std::move(loadedPcm);
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
        if (mciSendStringW(L"status avila_audio position", posBuf, 128, NULL) == 0) {
            int pos = QString::fromWCharArray(posBuf).toInt();
            if (pos >= 0) {
                m_currentPosition = pos;
            }
        }
#endif
    } else {
        int increment = static_cast<int>(50 * m_playbackSpeed);
        m_currentPosition += increment;
    }

    if (m_totalDuration > 0) {
        m_playbackProgress = static_cast<qreal>(m_currentPosition) / m_totalDuration;
    } else {
        m_playbackProgress = 0.0;
    }

    if (m_currentPosition >= m_totalDuration) {
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
        return;
    }

    emit playbackProgressChanged();
}
