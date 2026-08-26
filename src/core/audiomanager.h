// src/core/audiomanager.h
#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QString>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

class AudioManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY recordingStateChanged)
    Q_PROPERTY(int recordingDuration READ recordingDuration NOTIFY recordingDurationChanged)
    Q_PROPERTY(QVariantList liveWaveform READ liveWaveform NOTIFY liveWaveformChanged)

    Q_PROPERTY(QString currentPlayingId READ currentPlayingId NOTIFY playbackStateChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackStateChanged)
    Q_PROPERTY(qreal playbackProgress READ playbackProgress NOTIFY playbackProgressChanged)
    Q_PROPERTY(int currentPosition READ currentPosition NOTIFY playbackProgressChanged)
    Q_PROPERTY(int totalDuration READ totalDuration NOTIFY playbackDurationChanged)
    Q_PROPERTY(qreal playbackSpeed READ playbackSpeed WRITE setPlaybackSpeed NOTIFY playbackSpeedChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool isMuted READ isMuted NOTIFY isMutedChanged)

public:
    static AudioManager* instance();
    explicit AudioManager(QObject *parent = nullptr);
    ~AudioManager() override;

    bool isRecording() const { return m_isRecording; }
    int recordingDuration() const { return m_recordingDuration; }
    QVariantList liveWaveform() const { return m_liveWaveform; }

    QString currentPlayingId() const { return m_currentPlayingId; }
    bool isPlaying() const { return m_isPlaying; }
    qreal playbackProgress() const { return m_playbackProgress; }
    int currentPosition() const { return m_currentPosition; }
    int totalDuration() const { return m_totalDuration; }
    qreal playbackSpeed() const { return m_playbackSpeed; }
    qreal volume() const { return m_volume; }
    bool isMuted() const { return m_isMuted; }

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE QVariantMap stopRecording();
    Q_INVOKABLE void cancelRecording();

    Q_INVOKABLE void playAudio(const QString &messageId, const QString &audioUrl, int duration = 0);
    Q_INVOKABLE void pauseAudio();
    Q_INVOKABLE void togglePlayPause(const QString &messageId, const QString &audioUrl, int duration = 0);
    Q_INVOKABLE void seek(const QString &messageId, qreal progress);
    Q_INVOKABLE void setPlaybackSpeed(qreal speed);
    Q_INVOKABLE void setVolume(qreal vol);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void openMediaFile(const QString &mediaUrl);

    // Audio Output helper
    void playUiBeep(int freqHz, int durationMs);

signals:
    void recordingStateChanged();
    void recordingDurationChanged();
    void liveWaveformChanged();

    void playbackStateChanged();
    void playbackProgressChanged();
    void playbackDurationChanged();
    void playbackSpeedChanged();
    void playbackFinished(const QString &messageId);
    void volumeChanged();
    void isMutedChanged();

private slots:
    void onRecordTimerTick();
    void onPlaybackTimerTick();

private:
    void startAudioPlaybackThread();
    void stopAudioPlaybackThread();
    void audioOutputWorker();

    void startMicrophoneThread();
    void stopMicrophoneThread();
    void microphoneCaptureWorker();

    void saveWavFile(const QString &filePath, const std::vector<int16_t> &pcmData, int sampleRate);
    bool loadWavFile(const QString &filePath, std::vector<int16_t> &outPcm, int &outSampleRate);

    bool playViaMci(const QString &filePath);
    void pauseMci();
    void resumeMci();
    void seekMci(int positionMs);
    void stopMci();

    // Recording state
    bool m_isRecording{false};
    int m_recordingDuration{0}; // seconds
    QTimer *m_recordTimer{nullptr};
    QVariantList m_liveWaveform;
    std::vector<double> m_recordedAmplitudes;
    std::vector<int16_t> m_recordedPcmAudio;
    std::thread m_recordThread;
    std::atomic<bool> m_recordThreadRunning{false};
    std::mutex m_recordMutex;

    // Playback state
    QString m_currentPlayingId;
    QString m_currentAudioUrl;
    bool m_isPlaying{false};
    bool m_isMciActive{false};
    int m_currentPosition{0}; // ms
    int m_totalDuration{0}; // ms
    qreal m_playbackProgress{0.0};
    qreal m_playbackSpeed{1.0};
    qreal m_volume{1.0};
    bool m_isMuted{false};
    qreal m_preMuteVolume{1.0};
    QTimer *m_playbackTimer{nullptr};

    // Playback Audio Output
    std::thread m_audioThread;
    std::atomic<bool> m_audioThreadRunning{false};
    std::atomic<bool> m_audioPlaying{false};
    std::atomic<int> m_audioType{0}; // 0: voice synth, 1: music synth, 2: file pcm playback, 3: mci playback
    std::atomic<double> m_playbackSpeedAtomic{1.0};
    std::atomic<double> m_volumeAtomic{1.0};
    std::vector<int16_t> m_playbackFilePcm;
    std::atomic<size_t> m_playbackFileSampleIndex{0};
    std::mutex m_audioMutex;
};
