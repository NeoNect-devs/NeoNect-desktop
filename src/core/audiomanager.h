/**
 * @file audiomanager.h
 * @brief Multithreaded audio recording, playback, waveform analysis, and media management controller.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Exposes a unified audio engine to the QML user interface. Manages microphone capture
 * in a dedicated background worker thread, real-time waveform normalization, WAV file
 * encoding/decoding, low-latency playback via Windows Multimedia (waveOut / MCI), seek operations,
 * and UI notification sound generation.
 *
 * @par Design Patterns:
 * - <b>Facade Pattern</b>: Presents a high-level API over low-level platform audio drivers (waveIn, waveOut, MCI).
 * - <b>Active Object / Multithreaded Worker</b>: Audio streaming and microphone capture run on independent `std::thread` instances with atomic flags.
 * - <b>Observer Pattern</b>: Notifies QML views of playback progress, duration changes, and live waveform telemetry.
 */

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

/**
 * @class AudioManager
 * @brief Full-featured audio subsystem for voice notes, file playback, and waveform visualization.
 */
class AudioManager : public QObject {
    Q_OBJECT

    /** @brief True when the microphone is actively recording voice input. */
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY recordingStateChanged)
    /** @brief Elapsed recording time in seconds. */
    Q_PROPERTY(int recordingDuration READ recordingDuration NOTIFY recordingDurationChanged)
    /** @brief Live normalized amplitude samples for real-time recording visualization. */
    Q_PROPERTY(QVariantList liveWaveform READ liveWaveform NOTIFY liveWaveformChanged)

    /** @brief Message UUID of the currently playing audio track. */
    Q_PROPERTY(QString currentPlayingId READ currentPlayingId NOTIFY playbackStateChanged)
    /** @brief True if audio is actively playing through the output device. */
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackStateChanged)
    /** @brief Playback progress normalized from 0.0 to 1.0. */
    Q_PROPERTY(qreal playbackProgress READ playbackProgress NOTIFY playbackProgressChanged)
    /** @brief Current playback position in milliseconds. */
    Q_PROPERTY(int currentPosition READ currentPosition NOTIFY playbackProgressChanged)
    /** @brief Total duration of current track in milliseconds. */
    Q_PROPERTY(int totalDuration READ totalDuration NOTIFY playbackDurationChanged)
    /** @brief Playback speed multiplier (e.g. 1.0, 1.5, 2.0). */
    Q_PROPERTY(qreal playbackSpeed READ playbackSpeed WRITE setPlaybackSpeed NOTIFY playbackSpeedChanged)
    /** @brief Audio output volume level from 0.0 to 1.0. */
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    /** @brief Output mute state. */
    Q_PROPERTY(bool isMuted READ isMuted WRITE setMuted NOTIFY isMutedChanged)

public:
    /**
     * @brief Constructs the audio manager and initializes timer telemetry.
     * @param parent Optional parent QObject for Qt tree ownership.
     */
    explicit AudioManager(QObject *parent = nullptr);

    /**
     * @brief Destructor. Gracefully terminates audio capture and playback threads.
     */
    ~AudioManager() override;

    /** @brief Checks if voice recording is active. */
    bool isRecording() const { return m_isRecording; }
    /** @brief Returns elapsed recording duration in seconds. */
    int recordingDuration() const { return m_recordingDuration; }
    /** @brief Returns live waveform sample amplitudes. */
    QVariantList liveWaveform() const { return m_liveWaveform; }

    /** @brief Returns UUID of current playing track. */
    QString currentPlayingId() const { return m_currentPlayingId; }
    /** @brief Checks if playback is running. */
    bool isPlaying() const { return m_isPlaying; }
    /** @brief Returns normalized progress [0.0 - 1.0]. */
    qreal playbackProgress() const { return m_playbackProgress; }
    /** @brief Returns position in milliseconds. */
    int currentPosition() const { return m_currentPosition; }
    /** @brief Returns total duration in milliseconds. */
    int totalDuration() const { return m_totalDuration; }
    /** @brief Returns playback speed factor. */
    qreal playbackSpeed() const { return m_playbackSpeed; }
    /** @brief Returns volume level [0.0 - 1.0]. */
    qreal volume() const { return m_volume; }
    /** @brief Returns true if muted. */
    bool isMuted() const { return m_isMuted; }

    /**
     * @brief Starts microphone capture on a background thread and begins recording.
     */
    Q_INVOKABLE void startRecording();

    /**
     * @brief Stops microphone capture, writes encoded WAV to disk, and returns metadata.
     * @return QVariantMap containing `filePath`, `duration`, and `waveform`.
     */
    Q_INVOKABLE QVariantMap stopRecording();

    /**
     * @brief Aborts active recording and discards audio buffers without saving.
     */
    Q_INVOKABLE void cancelRecording();

    /**
     * @brief Starts or resumes playback of an audio message or external audio file.
     * @param messageId Unique message ID associated with track.
     * @param audioUrl Local file URL or path to audio source.
     * @param duration Optional hint duration in milliseconds.
     */
    Q_INVOKABLE void playAudio(const QString &messageId, const QString &audioUrl, int duration = 0);

    /**
     * @brief Pauses active audio playback.
     */
    Q_INVOKABLE void pauseAudio();

    /**
     * @brief Toggles between play and pause states for a given message track.
     * @param messageId Unique message identifier.
     * @param audioUrl Path to audio file.
     * @param duration Hint duration.
     */
    Q_INVOKABLE void togglePlayPause(const QString &messageId, const QString &audioUrl, int duration = 0);

    /**
     * @brief Seeks to a relative position within the current track.
     * @param messageId Target message ID.
     * @param progress Desired target progress [0.0 - 1.0].
     */
    Q_INVOKABLE void seek(const QString &messageId, qreal progress);

    /**
     * @brief Adjusts playback speed multiplier.
     * @param speed Speed multiplier (typically 1.0 to 2.0).
     */
    Q_INVOKABLE void setPlaybackSpeed(qreal speed);

    /**
     * @brief Sets master audio output volume.
     * @param vol Volume level [0.0 - 1.0].
     */
    Q_INVOKABLE void setVolume(qreal vol);

    /**
     * @brief Sets or clears the audio mute state.
     * @param muted Mute flag.
     */
    Q_INVOKABLE void setMuted(bool muted);

    /**
     * @brief Toggles mute on/off.
     */
    Q_INVOKABLE void toggleMute();

    /**
     * @brief Opens a local media file using the host OS default media player via shell execute.
     * @param mediaUrl Local file URL.
     */
    Q_INVOKABLE void openMediaFile(const QString &mediaUrl);

    /**
     * @brief Queries file size on disk in bytes.
     * @param fileUrl Local file path or URL.
     * @return Size in bytes.
     */
    Q_INVOKABLE qint64 getFileSize(const QString &fileUrl);

    /**
     * @brief Formats raw bytes into human-readable strings (e.g. `"14.2 MB"`).
     * @param bytes File size in bytes.
     * @return Formatted size string.
     */
    Q_INVOKABLE static QString formatFileSize(qint64 bytes);

    /**
     * @brief Generates a low-latency UI acoustic beep tone.
     * @param freqHz Tone frequency in Hertz.
     * @param durationMs Tone duration in milliseconds.
     */
    void playUiBeep(int freqHz, int durationMs);

signals:
    /** @brief Emitted when recording starts or stops. */
    void recordingStateChanged();
    /** @brief Emitted as recording duration updates. */
    void recordingDurationChanged();
    /** @brief Emitted when new amplitude samples are calculated for recording visualization. */
    void liveWaveformChanged();

    /** @brief Emitted when playback starts, pauses, or finishes. */
    void playbackStateChanged();
    /** @brief Emitted on playback position update. */
    void playbackProgressChanged();
    /** @brief Emitted when total duration is determined. */
    void playbackDurationChanged();
    /** @brief Emitted when playback speed changes. */
    void playbackSpeedChanged();
    /** @brief Emitted when an audio track finishes playing to the end. */
    void playbackFinished(const QString &messageId);
    /** @brief Emitted when volume changes. */
    void volumeChanged();
    /** @brief Emitted when mute state changes. */
    void isMutedChanged();

private slots:
    /** @brief Tick handler updating recording duration and waveform samples. */
    void onRecordTimerTick();
    /** @brief Tick handler updating playback progress and position. */
    void onPlaybackTimerTick();

private:
    /** @brief Starts the background PCM playback thread. */
    void startAudioPlaybackThread();
    /** @brief Stops the background PCM playback thread. */
    void stopAudioPlaybackThread();
    /** @brief Worker loop writing synthesized/PCM audio buffers to waveOut. */
    void audioOutputWorker();

    /** @brief Starts the background microphone capture thread. */
    void startMicrophoneThread();
    /** @brief Stops the background microphone capture thread. */
    void stopMicrophoneThread();
    /** @brief Worker loop reading microphone input from waveIn. */
    void microphoneCaptureWorker();

    /** @brief Writes 16-bit mono PCM samples to a RIFF/WAV audio file. */
    void saveWavFile(const QString &filePath, const std::vector<int16_t> &pcmData, int sampleRate);
    /** @brief Parses a RIFF/WAV audio file into raw 16-bit PCM samples. */
    bool loadWavFile(const QString &filePath, std::vector<int16_t> &outPcm, int &outSampleRate);

    /** @brief Starts Windows MCI playback for complex codecs (MP3/OGG). */
    bool playViaMci(const QString &filePath);
    /** @brief Pauses Windows MCI playback. */
    void pauseMci();
    /** @brief Resumes Windows MCI playback. */
    void resumeMci();
    /** @brief Seeks Windows MCI playback to a specific millisecond. */
    void seekMci(int positionMs);
    /** @brief Stops Windows MCI playback and releases device. */
    void stopMci();

    // Recording state
    bool m_isRecording{false};
    int m_recordingDuration{0};
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
    int m_currentPosition{0};
    int m_totalDuration{0};
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
    std::atomic<int> m_audioType{0};
    std::atomic<double> m_playbackSpeedAtomic{1.0};
    std::atomic<double> m_volumeAtomic{1.0};
    std::vector<int16_t> m_playbackFilePcm;
    std::atomic<size_t> m_playbackFileSampleIndex{0};
    std::mutex m_audioMutex;
};
