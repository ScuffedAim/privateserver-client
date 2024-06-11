#include "Sound.h"

#include <sstream>

#include "Bancho.h"
#include "ConVar.h"
#include "Engine.h"
#include "File.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "SoundEngine.h"

ConVar snd_play_interp_duration(
    "snd_play_interp_duration", 0.75f, FCVAR_DEFAULT,
    "smooth over freshly started channel position jitter with engine time over this duration in seconds");
ConVar snd_play_interp_ratio("snd_play_interp_ratio", 0.50f, FCVAR_DEFAULT,
                             "percentage of snd_play_interp_duration to use 100% engine time over audio time (some "
                             "devices report 0 for very long)");

ConVar snd_wav_file_min_size("snd_wav_file_min_size", 51, FCVAR_DEFAULT,
                             "minimum file size in bytes for WAV files to be considered valid (everything below will "
                             "fail to load), this is a workaround for BASS crashes");

Sound::Sound(std::string filepath, bool stream, bool overlayable, bool loop, bool prescan) : Resource(filepath) {
    m_sample = 0;
    m_stream = 0;
    m_bStream = stream;
    m_bIsLooped = loop;
    m_bPrescan = prescan;
    m_bIsOverlayable = overlayable;
    m_fSpeed = 1.0f;
    m_fVolume = 1.0f;
}

std::vector<HCHANNEL> Sound::getActiveChannels() {
    std::vector<HCHANNEL> channels;

    if(m_bStream) {
        if(BASS_Mixer_ChannelGetMixer(m_stream) != 0) {
            channels.push_back(m_stream);
        }
    } else {
        for(auto chan : mixer_channels) {
            if(BASS_Mixer_ChannelGetMixer(chan) != 0) {
                channels.push_back(chan);
            }
        }

        // Only keep channels that are still playing
        mixer_channels = channels;
    }

    return channels;
}

HCHANNEL Sound::getChannel() {
    if(m_bStream) {
        return m_stream;
    } else {
        // If we want to be able to control samples after playing them, we
        // have to store them here, since bassmix only accepts DECODE streams.
        auto chan = BASS_SampleGetChannel(m_sample, BASS_SAMCHAN_STREAM | BASS_STREAM_DECODE);
        mixer_channels.push_back(chan);
        return chan;
    }
}

void Sound::init() {
    if(m_sFilePath.length() < 2 || !m_bAsyncReady) return;

    // HACKHACK: re-set some values to their defaults (only necessary because of the existence of rebuild())
    m_fSpeed = 1.0f;

    // error checking
    if(m_sample == 0 && m_stream == 0) {
        UString msg = "Couldn't load sound \"";
        msg.append(m_sFilePath.c_str());
        msg.append(UString::format("\", stream = %i, errorcode = %i", (int)m_bStream, BASS_ErrorGetCode()));
        msg.append(", file = ");
        msg.append(m_sFilePath.c_str());
        msg.append("\n");
        debugLog(0xffdd3333, "%s", msg.toUtf8());
    } else {
        m_bReady = true;
    }
}

void Sound::initAsync() {
    if(ResourceManager::debug_rm->getBool()) debugLog("Resource Manager: Loading %s\n", m_sFilePath.c_str());

    // HACKHACK: workaround for BASS crashes on malformed WAV files
    {
        const int minWavFileSize = snd_wav_file_min_size.getInt();
        if(minWavFileSize > 0) {
            auto fileExtensionLowerCase = UString(env->getFileExtensionFromFilePath(m_sFilePath).c_str());
            fileExtensionLowerCase.lowerCase();
            if(fileExtensionLowerCase == UString("wav")) {
                File wavFile(m_sFilePath);
                if(wavFile.getFileSize() < (size_t)minWavFileSize) {
                    printf("Sound: Ignoring malformed/corrupt WAV file (%i) %s\n", (int)wavFile.getFileSize(),
                           m_sFilePath.c_str());
                    return;
                }
            }
        }
    }

#ifdef _WIN32
    // On Windows, we need to convert the UTF-8 path to UTF-16, or paths with unicode characters will fail to open
    int size = MultiByteToWideChar(CP_UTF8, 0, m_sFilePath.c_str(), m_sFilePath.length(), NULL, 0);
    std::wstring file_path(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, m_sFilePath.c_str(), m_sFilePath.length(), (LPWSTR)file_path.c_str(), file_path.length());
#else
    std::string file_path = m_sFilePath;
#endif

    if(m_bStream) {
        auto flags = BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT;
        if(m_bPrescan) flags |= BASS_STREAM_PRESCAN;
        if(env->getOS() == Environment::OS::WINDOWS) flags |= BASS_UNICODE;

        m_stream = BASS_StreamCreateFile(false, file_path.c_str(), 0, 0, flags);
        if(!m_stream) {
            debugLog("BASS_StreamCreateFile() returned error %d on file %s\n", BASS_ErrorGetCode(),
                     m_sFilePath.c_str());
            return;
        }

        m_stream = BASS_FX_TempoCreate(m_stream, BASS_FX_FREESOURCE | BASS_STREAM_DECODE);
        if(!m_stream) {
            debugLog("BASS_FX_TempoCreate() returned error %d on file %s\n", BASS_ErrorGetCode(), m_sFilePath.c_str());
            return;
        }

        // Only compute the length once
        i64 length = BASS_ChannelGetLength(m_stream, BASS_POS_BYTE);
        f64 lengthInSeconds = BASS_ChannelBytes2Seconds(m_stream, length);
        f64 lengthInMilliSeconds = lengthInSeconds * 1000.0;
        m_length = (u32)lengthInMilliSeconds;
    } else {
        auto flags = BASS_SAMPLE_FLOAT;
        if(env->getOS() == Environment::OS::WINDOWS) flags |= BASS_UNICODE;

        m_sample = BASS_SampleLoad(false, file_path.c_str(), 0, 0, 1, flags);
        if(!m_sample) {
            debugLog("BASS_SampleLoad() returned error %d on file %s\n", BASS_ErrorGetCode(), m_sFilePath.c_str());
            return;
        }

        // Only compute the length once
        i64 length = BASS_ChannelGetLength(m_sample, BASS_POS_BYTE);
        f64 lengthInSeconds = BASS_ChannelBytes2Seconds(m_sample, length);
        f64 lengthInMilliSeconds = lengthInSeconds * 1000.0;
        m_length = (u32)lengthInMilliSeconds;
    }

    m_bAsyncReady = true;
}

void Sound::destroy() {
    if(!m_bReady) return;

    m_bStarted = false;
    m_bReady = false;
    m_bAsyncReady = false;
    m_fLastPlayTime = 0.0;
    m_bPaused = false;

    if(m_bStream) {
        BASS_Mixer_ChannelRemove(m_stream);
        BASS_ChannelStop(m_stream);
        BASS_StreamFree(m_stream);
        m_stream = 0;
    } else {
        for(auto chan : mixer_channels) {
            BASS_Mixer_ChannelRemove(chan);
            BASS_ChannelStop(chan);
            BASS_ChannelFree(chan);
        }
        mixer_channels.clear();

        BASS_SampleStop(m_sample);
        BASS_SampleFree(m_sample);
        m_sample = 0;
    }
}

void Sound::setPosition(double percent) {
    if(!m_bReady) return;
    if(!m_bStream) {
        engine->showMessageError("Programmer Error", "Called setPosition on a sample!");
        return;
    }

    percent = clamp<f64>(percent, 0.0, 1.0);

    f64 length = BASS_ChannelGetLength(m_stream, BASS_POS_BYTE);
    if(length < 0) {
        debugLog("Could not set stream position: error %d\n", BASS_ErrorGetCode());
        return;
    }

    f64 lengthInSeconds = BASS_ChannelBytes2Seconds(m_stream, length);
    if(!BASS_ChannelSetPosition(m_stream, (i64)(length * percent), BASS_POS_BYTE | BASS_POS_FLUSH)) {
        if(Osu::debug->getBool()) {
            debugLog("Sound::setPosition( %f ) BASS_ChannelSetPosition() error %i on file %s\n", percent,
                     BASS_ErrorGetCode(), m_sFilePath.c_str());
        }
    }

    if(m_bStarted) {
        m_fLastPlayTime = engine->getTime() - (lengthInSeconds * percent);
    }
}

void Sound::setPositionMS(unsigned long ms) {
    if(!m_bReady || ms > getLengthMS()) return;
    if(!m_bStream) {
        engine->showMessageError("Programmer Error", "Called setPositionMS on a sample!");
        return;
    }

    i64 position = BASS_ChannelSeconds2Bytes(m_stream, ms / 1000.0);
    if(position < 0) {
        debugLog("Could not set stream position: error %d\n", BASS_ErrorGetCode());
        return;
    }

    if(!BASS_ChannelSetPosition(m_stream, position, BASS_POS_BYTE | BASS_POS_FLUSH)) {
        if(Osu::debug->getBool()) {
            debugLog("Sound::setPositionMS( %lu ) BASS_ChannelSetPosition() error %i on file %s\n", ms,
                     BASS_ErrorGetCode(), m_sFilePath.c_str());
        }
    }

    if(m_bStarted) {
        m_fLastPlayTime = engine->getTime() - ((f64)ms / 1000.0);
    }
}

void Sound::setVolume(float volume) {
    if(!m_bReady) return;

    m_fVolume = clamp<float>(volume, 0.0f, 2.0f);

    for(auto channel : getActiveChannels()) {
        BASS_ChannelSetAttribute(channel, BASS_ATTRIB_VOL, m_fVolume);
    }
}

void Sound::setSpeed(float speed) {
    if(!m_bReady) return;
    if(!m_bStream) {
        engine->showMessageError("Programmer Error", "Called setSpeed on a sample!");
        return;
    }

    speed = clamp<float>(speed, 0.05f, 50.0f);

    float freq = convar->getConVarByName("snd_freq")->getFloat();
    BASS_ChannelGetAttribute(m_stream, BASS_ATTRIB_FREQ, &freq);

    BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_TEMPO, 1.0f);
    BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_TEMPO_FREQ, freq);

    bool nightcoring = osu->getModNC() || osu->getModDC();
    if(nightcoring) {
        BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_TEMPO_FREQ, speed * freq);
    } else {
        BASS_ChannelSetAttribute(m_stream, BASS_ATTRIB_TEMPO, (speed - 1.0f) * 100.0f);
    }

    m_fSpeed = speed;
}

void Sound::setFrequency(float frequency) {
    if(!m_bReady) return;

    frequency = (frequency > 99.0f ? clamp<float>(frequency, 100.0f, 100000.0f) : 0.0f);

    for(auto channel : getActiveChannels()) {
        BASS_ChannelSetAttribute(channel, BASS_ATTRIB_FREQ, frequency);
    }
}

void Sound::setPan(float pan) {
    if(!m_bReady) return;

    m_fPan = clamp<float>(pan, -1.0f, 1.0f);

    for(auto channel : getActiveChannels()) {
        BASS_ChannelSetAttribute(channel, BASS_ATTRIB_PAN, m_fPan);
    }
}

void Sound::setLoop(bool loop) {
    if(!m_bReady) return;
    if(!m_bStream) {
        engine->showMessageError("Programmer Error", "Called setLoop on a sample!");
        return;
    }

    m_bIsLooped = loop;
    BASS_ChannelFlags(m_stream, m_bIsLooped ? BASS_SAMPLE_LOOP : 0, BASS_SAMPLE_LOOP);
}

float Sound::getPosition() {
    if(!m_bReady) return 0.f;
    if(!m_bStream) {
        engine->showMessageError("Programmer Error", "Called getPosition on a sample!");
        return 0.f;
    }

    i64 lengthBytes = BASS_ChannelGetLength(m_stream, BASS_POS_BYTE);
    if(lengthBytes < 0) {
        // The stream ended and got freed by BASS_STREAM_AUTOFREE -> invalid handle!
        return 1.f;
    }

    i64 positionBytes = BASS_ChannelGetPosition(m_stream, BASS_POS_BYTE);
    const float position = (float)((double)(positionBytes) / (double)(lengthBytes));
    return position;
}

u32 Sound::getPositionMS() {
    if(!m_bReady) return 0;
    if(!m_bStream) {
        engine->showMessageError("Programmer Error", "Called getPositionMS on a sample!");
        return 0;
    }

    i64 position = BASS_ChannelGetPosition(m_stream, BASS_POS_BYTE);
    if(position < 0) {
        // The stream ended and got freed by BASS_STREAM_AUTOFREE -> invalid handle!
        return m_length;
    }

    f64 positionInSeconds = BASS_ChannelBytes2Seconds(m_stream, position);
    f64 positionInMilliSeconds = positionInSeconds * 1000.0;
    u32 positionMS = (u32)positionInMilliSeconds;
    if(!isPlaying()) {
        return positionMS;
    }

    // special case: a freshly started channel position jitters, lerp with engine time over a set duration to smooth
    // things over
    f64 interpDuration = snd_play_interp_duration.getFloat();
    u32 interpDurationMS = interpDuration * 1000;
    if(interpDuration <= 0.0 || positionMS >= interpDurationMS) return positionMS;

    f64 speedMultiplier = getSpeed();
    f64 delta = (engine->getTime() - m_fLastPlayTime) * speedMultiplier;
    f64 interp_ratio = snd_play_interp_ratio.getFloat();
    if(delta < interpDuration) {
        f64 lerpPercent = clamp<f64>(((delta / interpDuration) - interp_ratio) / (1.0 - interp_ratio), 0.0, 1.0);
        positionMS = (u32)lerp<f64>(delta * 1000.0, (f64)positionMS, lerpPercent);
    }

    return positionMS;
}

u32 Sound::getLengthMS() {
    if(!m_bReady) return 0;
    return m_length;
}

float Sound::getSpeed() { return m_fSpeed; }

float Sound::getFrequency() {
    auto default_freq = convar->getConVarByName("snd_freq")->getFloat();
    if(!m_bReady) return default_freq;
    if(!m_bStream) {
        engine->showMessageError("Programmer Error", "Called getFrequency on a sample!");
        return default_freq;
    }

    float frequency = default_freq;
    BASS_ChannelGetAttribute(m_stream, BASS_ATTRIB_FREQ, &frequency);
    return frequency;
}

bool Sound::isPlaying() {
    return m_bReady && m_bStarted && !m_bPaused && !getActiveChannels().empty();
}

bool Sound::isFinished() {
    return m_bReady && m_bStarted && !isPlaying();
}

void Sound::rebuild(std::string newFilePath) {
    m_sFilePath = newFilePath;
    reload();
}
