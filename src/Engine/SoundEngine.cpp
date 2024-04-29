//================ Copyright (c) 2014, PG, All rights reserved. =================//
//
// Purpose:		handles sounds, bass library wrapper atm
//
// $NoKeywords: $snd
//===============================================================================//

// TODO: async audio implementation still needs changes in Sound class playing-state handling
// TODO: async audio thread needs proper delay timing
// TODO: finish dynamic audio device updating, but can only do async due to potential lag, disabled by default

#include "SoundEngine.h"

#include "Bancho.h"
#include "CBaseUILabel.h"
#include "CBaseUISlider.h"
#include "ConVar.h"
#include "Engine.h"
#include "Environment.h"
#include "Osu.h"
#include "OsuBeatmap.h"
#include "OsuOptionsMenu.h"
#include "OsuSkin.h"
#include "Sound.h"
#include "Thread.h"
#include "WinEnvironment.h"

// removed from latest headers. not sure if it's handled at all
#ifndef BASS_CONFIG_MP3_OLDGAPS
#define BASS_CONFIG_MP3_OLDGAPS 68
#endif

void _volume(UString oldValue, UString newValue) {
    (void)oldValue;
    engine->getSound()->setVolume(newValue.toFloat());
}

ConVar _volume_("volume", 1.0f, FCVAR_NONE, _volume);

ConVar snd_output_device("snd_output_device", "Default", FCVAR_NONE);
ConVar snd_restart("snd_restart");

ConVar snd_freq("snd_freq", 44100, FCVAR_NONE, "output sampling rate in Hz");
ConVar snd_updateperiod("snd_updateperiod", 10, FCVAR_NONE, "BASS_CONFIG_UPDATEPERIOD length in milliseconds");
ConVar snd_dev_period("snd_dev_period", 10, FCVAR_NONE,
                      "BASS_CONFIG_DEV_PERIOD length in milliseconds, or if negative then in samples");
ConVar snd_dev_buffer("snd_dev_buffer", 30, FCVAR_NONE, "BASS_CONFIG_DEV_BUFFER length in milliseconds");
ConVar snd_chunk_size("snd_chunk_size", 256, FCVAR_NONE, "only used in horizon builds with sdl mixer audio");

ConVar snd_restrict_play_frame(
    "snd_restrict_play_frame", true, FCVAR_NONE,
    "only allow one new channel per frame for overlayable sounds (prevents lag and earrape)");
ConVar snd_change_check_interval("snd_change_check_interval", 0.0f, FCVAR_NONE,
                                 "check for output device changes every this many seconds. 0 = disabled (default)");

ConVar win_snd_wasapi_buffer_size(
    "win_snd_wasapi_buffer_size", 0.011f, FCVAR_NONE,
    "buffer size/length in seconds (e.g. 0.011 = 11 ms), directly responsible for audio delay and crackling");
ConVar win_snd_wasapi_period_size(
    "win_snd_wasapi_period_size", 0.0f, FCVAR_NONE,
    "interval between OutputWasapiProc calls in seconds (e.g. 0.016 = 16 ms) (0 = use default)");

ConVar asio_buffer_size("asio_buffer_size", -1, FCVAR_NONE,
                        "buffer size in samples (usually 44100 samples per second)");

ConVar osu_universal_offset_hardcoded("osu_universal_offset_hardcoded", 0.0f, FCVAR_NONE);

DWORD CALLBACK OutputWasapiProc(void *buffer, DWORD length, void *user) {
    if(engine->getSound()->g_bassOutputMixer == 0) return 0;
    const int c = BASS_ChannelGetData(engine->getSound()->g_bassOutputMixer, buffer, length);
    return c < 0 ? 0 : c;
}

void _RESTART_SOUND_ENGINE_ON_CHANGE(UString oldValue, UString newValue) {
    const int oldValueMS = std::round(oldValue.toFloat() * 1000.0f);
    const int newValueMS = std::round(newValue.toFloat() * 1000.0f);

    if(oldValueMS != newValueMS) engine->getSound()->restart();
}

DWORD ASIO_clamp(BASS_ASIO_INFO info, DWORD buflen) {
    if(buflen == -1) return info.bufpref;
    if(buflen < info.bufmin) return info.bufmin;
    if(buflen > info.bufmax) return info.bufmax;
    if(info.bufgran == 0) return buflen;

    if(info.bufgran == -1) {
        // Buffer lengths are only allowed in powers of 2
        for(int oksize = info.bufmin; oksize <= info.bufmax; oksize *= 2) {
            if(oksize == buflen) {
                return buflen;
            } else if(oksize > buflen) {
                oksize /= 2;
                return oksize;
            }
        }

        // Unreachable
        return info.bufpref;
    } else {
        // Buffer lengths are only allowed in multiples of info.bufgran
        buflen -= info.bufmin;
        buflen = (buflen / info.bufgran) * info.bufgran;
        buflen += info.bufmin;
        return buflen;
    }
}

SoundEngine::SoundEngine() {
    auto bass_version = BASS_GetVersion();
    debugLog("SoundEngine: BASS version = 0x%08x\n", bass_version);
    if(HIWORD(bass_version) != BASSVERSION) {
        engine->showMessageErrorFatal("Fatal Sound Error", "An incorrect version of the BASS library file was loaded!");
        engine->shutdown();
        return;
    }

    auto mixer_version = BASS_Mixer_GetVersion();
    debugLog("SoundEngine: BASSMIX version = 0x%08x\n", mixer_version);
    if(HIWORD(mixer_version) != BASSVERSION) {
        engine->showMessageErrorFatal("Fatal Sound Error",
                                      "An incorrect version of the BASSMIX library file was loaded!");
        engine->shutdown();
        return;
    }

    auto loud_version = BASS_Loudness_GetVersion();
    debugLog("SoundEngine: BASSloud version = 0x%08x\n", loud_version);
    if(HIWORD(loud_version) != BASSVERSION) {
        engine->showMessageErrorFatal("Fatal Sound Error",
                                      "An incorrect version of the BASSloud library file was loaded!");
        engine->shutdown();
        return;
    }

#ifdef _WIN32
    auto asio_version = BASS_ASIO_GetVersion();
    debugLog("SoundEngine: BASSASIO version = 0x%08x\n", asio_version);
    if(HIWORD(asio_version) != BASSASIOVERSION) {
        engine->showMessageErrorFatal("Fatal Sound Error",
                                      "An incorrect version of the BASSASIO library file was loaded!");
        engine->shutdown();
        return;
    }

    auto wasapi_version = BASS_WASAPI_GetVersion();
    debugLog("SoundEngine: BASSWASAPI version = 0x%08x\n", wasapi_version);
    if(HIWORD(wasapi_version) != BASSVERSION) {
        engine->showMessageErrorFatal("Fatal Sound Error",
                                      "An incorrect version of the BASSWASAPI library file was loaded!");
        engine->shutdown();
        return;
    }
#endif

    BASS_SetConfig(BASS_CONFIG_BUFFER, 100);

    // all beatmaps timed to non-iTunesSMPB + 529 sample deletion offsets on old dlls pre 2015
    BASS_SetConfig(BASS_CONFIG_MP3_OLDGAPS, 1);

    // avoids lag/jitter in BASS_ChannelGetPosition() shortly after a BASS_ChannelPlay() after loading/silence
    BASS_SetConfig(BASS_CONFIG_DEV_NONSTOP, 1);

    // if set to 1, increases sample playback latency by 10 ms
    BASS_SetConfig(BASS_CONFIG_VISTA_TRUEPOS, 0);

    m_currentOutputDevice = {
        .id = 0,
        .enabled = true,
        .isDefault = true,
        .name = "No sound",
        .driver = OutputDriver::NONE,
    };
}

OUTPUT_DEVICE SoundEngine::getWantedDevice() {
    auto wanted_name = snd_output_device.getString();
    for(auto device : m_outputDevices) {
        if(device.enabled && device.name == wanted_name) {
            return device;
        }
    }

    debugLog("Could not find sound device '%s', initializing default one instead.\n", wanted_name.toUtf8());
    return getDefaultDevice();
}

OUTPUT_DEVICE SoundEngine::getDefaultDevice() {
    for(auto device : m_outputDevices) {
        if(device.enabled && device.isDefault) {
            return device;
        }
    }

    debugLog("Could not find a working sound device!\n");
    return {
        .id = 0,
        .enabled = true,
        .isDefault = true,
        .name = "No sound",
        .driver = OutputDriver::NONE,
    };
}

void SoundEngine::updateOutputDevices(bool printInfo) {
    m_outputDevices.clear();

    BASS_DEVICEINFO deviceInfo;
    for(int d = 0; (BASS_GetDeviceInfo(d, &deviceInfo) == true); d++) {
        const bool isEnabled = (deviceInfo.flags & BASS_DEVICE_ENABLED);
        const bool isDefault = (deviceInfo.flags & BASS_DEVICE_DEFAULT);

        OUTPUT_DEVICE soundDevice;
        soundDevice.id = d;
        soundDevice.name = deviceInfo.name;
        soundDevice.enabled = isEnabled;
        soundDevice.isDefault = isDefault;

        // avoid duplicate names
        int duplicateNameCounter = 2;
        while(true) {
            bool foundDuplicateName = false;
            for(size_t i = 0; i < m_outputDevices.size(); i++) {
                if(m_outputDevices[i].name == soundDevice.name) {
                    foundDuplicateName = true;

                    soundDevice.name = deviceInfo.name;
                    soundDevice.name.append(UString::format(" (%i)", duplicateNameCounter));

                    duplicateNameCounter++;

                    break;
                }
            }

            if(!foundDuplicateName) break;
        }

        soundDevice.driver = OutputDriver::BASS;
        m_outputDevices.push_back(soundDevice);

        debugLog("DSOUND: Device %i = \"%s\", enabled = %i, default = %i\n", d, deviceInfo.name, (int)isEnabled,
                 (int)isDefault);
    }

#ifdef _WIN32
    BASS_ASIO_DEVICEINFO asioDeviceInfo;
    for(int d = 0; (BASS_ASIO_GetDeviceInfo(d, &asioDeviceInfo) == true); d++) {
        OUTPUT_DEVICE soundDevice;
        soundDevice.id = d;
        soundDevice.name = asioDeviceInfo.name;
        soundDevice.enabled = true;
        soundDevice.isDefault = false;

        // avoid duplicate names
        int duplicateNameCounter = 2;
        while(true) {
            bool foundDuplicateName = false;
            for(size_t i = 0; i < m_outputDevices.size(); i++) {
                if(m_outputDevices[i].name == soundDevice.name) {
                    foundDuplicateName = true;

                    soundDevice.name = deviceInfo.name;
                    soundDevice.name.append(UString::format(" (%i)", duplicateNameCounter));

                    duplicateNameCounter++;

                    break;
                }
            }

            if(!foundDuplicateName) break;
        }

        soundDevice.driver = OutputDriver::BASS_ASIO;
        soundDevice.name.append(" [ASIO]");
        m_outputDevices.push_back(soundDevice);

        debugLog("ASIO: Device %i = \"%s\"\n", d, asioDeviceInfo.name);
    }

    BASS_WASAPI_DEVICEINFO wasapiDeviceInfo;
    for(int d = 0; (BASS_WASAPI_GetDeviceInfo(d, &wasapiDeviceInfo) == true); d++) {
        const bool isEnabled = (wasapiDeviceInfo.flags & BASS_DEVICE_ENABLED);
        const bool isDefault = (wasapiDeviceInfo.flags & BASS_DEVICE_DEFAULT);
        const bool isInput = (wasapiDeviceInfo.flags & BASS_DEVICE_INPUT);
        if(isInput) continue;

        OUTPUT_DEVICE soundDevice;
        soundDevice.id = d;
        soundDevice.name = wasapiDeviceInfo.name;
        soundDevice.enabled = isEnabled;
        soundDevice.isDefault = isDefault;

        // avoid duplicate names
        int duplicateNameCounter = 2;
        while(true) {
            bool foundDuplicateName = false;
            for(size_t i = 0; i < m_outputDevices.size(); i++) {
                if(m_outputDevices[i].name == soundDevice.name) {
                    foundDuplicateName = true;

                    soundDevice.name = wasapiDeviceInfo.name;
                    soundDevice.name.append(UString::format(" (%i)", duplicateNameCounter));

                    duplicateNameCounter++;

                    break;
                }
            }

            if(!foundDuplicateName) break;
        }

        soundDevice.driver = OutputDriver::BASS_WASAPI;
        soundDevice.name.append(" [WASAPI]");
        m_outputDevices.push_back(soundDevice);

        debugLog("WASAPI: Device %i = \"%s\", enabled = %i, default = %i\n", d, wasapiDeviceInfo.name, (int)isEnabled,
                 (int)isDefault);
    }
#endif
}

bool SoundEngine::initializeOutputDevice(OUTPUT_DEVICE device) {
    debugLog("SoundEngine: initializeOutputDevice( %s ) ...\n", device.name.toUtf8());

    if(m_currentOutputDevice.driver == OutputDriver::BASS) {
        BASS_SetDevice(0);
        BASS_Free();
        BASS_SetDevice(m_currentOutputDevice.id);

        BASS_Free();
    } else if(m_currentOutputDevice.driver == OutputDriver::BASS_ASIO) {
#ifdef _WIN32
        g_bassOutputMixer = 0;
        BASS_ASIO_Free();
#endif
        BASS_Free();
    } else if(m_currentOutputDevice.driver == OutputDriver::BASS_WASAPI) {
#ifdef _WIN32
        g_bassOutputMixer = 0;
        BASS_WASAPI_Free();
#endif
        BASS_Free();
    }

    if(device.driver == OutputDriver::NONE) {
        m_bReady = true;
        m_currentOutputDevice = device;
        snd_output_device.setValue(m_currentOutputDevice.name);
        debugLog("SoundEngine: Output Device = \"%s\"\n", m_currentOutputDevice.name.toUtf8());

        if(bancho.osu && bancho.osu->m_optionsMenu) {
            bancho.osu->m_optionsMenu->updateLayout();
        }

        return true;
    }

    if(device.driver == OutputDriver::BASS) {
        // NOTE: only used by osu atm (new osu uses 5 instead of 10, but not tested enough for offset problems)
        BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 10);
        BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 1);
    } else if(device.driver == OutputDriver::BASS_ASIO || device.driver == OutputDriver::BASS_WASAPI) {
        BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 0);
        BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);
    }

    // allow users to override some defaults (but which may cause beatmap desyncs)
    // we only want to set these if their values have been explicitly modified (to avoid sideeffects in the default
    // case, and for my sanity)
    {
        if(snd_updateperiod.getFloat() != snd_updateperiod.getDefaultFloat())
            BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, snd_updateperiod.getInt());

        if(snd_dev_buffer.getFloat() != snd_dev_buffer.getDefaultFloat())
            BASS_SetConfig(BASS_CONFIG_DEV_BUFFER, snd_dev_buffer.getInt());

        if(snd_dev_period.getFloat() != snd_dev_period.getDefaultFloat())
            BASS_SetConfig(BASS_CONFIG_DEV_PERIOD, snd_dev_period.getInt());
    }

    const int freq = snd_freq.getInt();
    HWND hwnd = NULL;
#ifdef _WIN32
    const WinEnvironment *winEnv = dynamic_cast<WinEnvironment *>(env);
    hwnd = winEnv->getHwnd();
#endif

    int bass_device_id = device.id;
    unsigned int runtimeFlags = BASS_DEVICE_STEREO | BASS_DEVICE_FREQ;
    if(device.driver == OutputDriver::BASS) {
        // Regular BASS: we still want a "No sound" device to check for loudness
        if(!BASS_Init(0, freq, runtimeFlags | BASS_DEVICE_NOSPEAKER, hwnd, NULL)) {
            m_bReady = false;
            engine->showMessageError("Sound Error", UString::format("BASS_Init(0) failed (%i)!", BASS_ErrorGetCode()));
            return false;
        }
    } else if(device.driver == OutputDriver::BASS_ASIO || device.driver == OutputDriver::BASS_WASAPI) {
        // ASIO and WASAPI: Initialize BASS on "No sound" device
        runtimeFlags |= BASS_DEVICE_NOSPEAKER;
        bass_device_id = 0;
    }

    if(!BASS_Init(bass_device_id, freq, runtimeFlags, hwnd, NULL)) {
        m_bReady = false;
        engine->showMessageError("Sound Error",
                                 UString::format("BASS_Init(%d) failed (%i)!", bass_device_id, BASS_ErrorGetCode()));
        return false;
    }

    // Starting with bass 2020 2.4.15.2 which has all offset problems
    // fixed, this is the non-dsound backend compensation.
    // Gets overwritten later if ASIO or WASAPI driver is used.
    // Depends on BASS_CONFIG_UPDATEPERIOD/BASS_CONFIG_DEV_BUFFER.
    osu_universal_offset_hardcoded.setValue(15.0f);

    if(device.driver == OutputDriver::BASS_ASIO) {
#ifdef _WIN32
        if(!BASS_ASIO_Init(device.id, 0)) {
            m_bReady = false;
            engine->showMessageError("Sound Error",
                                     UString::format("BASS_ASIO_Init() failed (%i)!", BASS_ASIO_ErrorGetCode()));
            return false;
        }

        double sample_rate = BASS_ASIO_GetRate();
        if(sample_rate == 0.0) {
            sample_rate = snd_freq.getFloat();
            debugLog("ASIO: BASS_ASIO_GetRate() returned 0, using %f instead!\n", sample_rate);
        }

        BASS_ASIO_INFO info = {0};
        BASS_ASIO_GetInfo(&info);
        auto bufsize = asio_buffer_size.getInt();
        bufsize = ASIO_clamp(info, bufsize);

        if(bancho.osu && bancho.osu->m_optionsMenu) {
            auto slider = bancho.osu->m_optionsMenu->m_asioBufferSizeSlider;
            slider->setBounds(info.bufmin, info.bufmax);
            slider->setKeyDelta(info.bufgran == -1 ? info.bufmin : info.bufgran);
        }

        auto mixer_flags = BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE | BASS_MIXER_NONSTOP;
        g_bassOutputMixer = BASS_Mixer_StreamCreate(sample_rate, 2, mixer_flags);
        if(g_bassOutputMixer == 0) {
            m_bReady = false;
            engine->showMessageError("Sound Error",
                                     UString::format("BASS_Mixer_StreamCreate() failed (%i)!", BASS_ErrorGetCode()));
            return false;
        }

        if(!BASS_ASIO_ChannelEnableBASS(false, 0, g_bassOutputMixer, true)) {
            m_bReady = false;
            engine->showMessageError("Sound Error", UString::format("BASS_ASIO_ChannelEnableBASS() failed (code %i)!",
                                                                    BASS_ASIO_ErrorGetCode()));
            return false;
        }

        if(!BASS_ASIO_Start(bufsize, 0)) {
            m_bReady = false;
            engine->showMessageError("Sound Error",
                                     UString::format("BASS_ASIO_Start() failed (code %i)!", BASS_ASIO_ErrorGetCode()));
            return false;
        }

        double wanted_latency = 1000.0 * asio_buffer_size.getFloat() / sample_rate;
        double actual_latency = 1000.0 * (double)BASS_ASIO_GetLatency(false) / sample_rate;
        osu_universal_offset_hardcoded.setValue(-(actual_latency + 25.0f));
        debugLog("ASIO: wanted %f ms, got %f ms latency. Sample rate: %f Hz\n", wanted_latency, actual_latency,
                 sample_rate);
#endif
    }

    if(device.driver == OutputDriver::BASS_WASAPI) {
#ifdef _WIN32
        const float bufferSize = std::round(win_snd_wasapi_buffer_size.getFloat() * 1000.0f) / 1000.0f;    // in seconds
        const float updatePeriod = std::round(win_snd_wasapi_period_size.getFloat() * 1000.0f) / 1000.0f;  // in seconds

        // BASS_WASAPI_RAW ignores sound "enhancements" that some sound cards offer (adds latency)
        // BASS_MIXER_NONSTOP prevents some sound cards from going to sleep when there is no output
        auto flags = BASS_WASAPI_RAW | BASS_MIXER_NONSTOP | BASS_WASAPI_EXCLUSIVE;

        debugLog("WASAPI bufferSize = %f, updatePeriod = %f\n", bufferSize, updatePeriod);
        if(!BASS_WASAPI_Init(device.id, 0, 0, flags, bufferSize, updatePeriod, OutputWasapiProc, NULL)) {
            const int errorCode = BASS_ErrorGetCode();
            if(errorCode == BASS_ERROR_WASAPI_BUFFER) {
                debugLog("Sound Error: BASS_WASAPI_Init() failed with BASS_ERROR_WASAPI_BUFFER!");
            } else {
                engine->showMessageError("Sound Error", UString::format("BASS_WASAPI_Init() failed (%i)!", errorCode));
            }

            m_bReady = false;
            return false;
        }

        BASS_WASAPI_INFO wasapiInfo;
        BASS_WASAPI_GetInfo(&wasapiInfo);
        auto mixer_flags = BASS_SAMPLE_FLOAT | BASS_STREAM_DECODE | BASS_MIXER_NONSTOP;
        g_bassOutputMixer = BASS_Mixer_StreamCreate(wasapiInfo.freq, wasapiInfo.chans, mixer_flags);
        if(g_bassOutputMixer == 0) {
            m_bReady = false;
            engine->showMessageError("Sound Error",
                                     UString::format("BASS_Mixer_StreamCreate() failed (%i)!", BASS_ErrorGetCode()));
            return false;
        }

        if(!BASS_WASAPI_Start()) {
            m_bReady = false;
            engine->showMessageError("Sound Error",
                                     UString::format("BASS_WASAPI_Start() failed (%i)!", BASS_ErrorGetCode()));
            return false;
        }

        // since we use the newer bass/fx dlls for wasapi builds anyway (which have different time handling)
        osu_universal_offset_hardcoded.setValue(-25.0f);
#endif
    }

    m_bReady = true;
    m_currentOutputDevice = device;
    snd_output_device.setValue(m_currentOutputDevice.name);
    debugLog("SoundEngine: Output Device = \"%s\"\n", m_currentOutputDevice.name.toUtf8());

    if(bancho.osu && bancho.osu->m_optionsMenu) {
        bancho.osu->m_optionsMenu->updateLayout();
    }

    return true;
}

SoundEngine::~SoundEngine() {
    if(m_currentOutputDevice.driver == OutputDriver::BASS) {
        BASS_Free();
    } else if(m_currentOutputDevice.driver == OutputDriver::BASS_ASIO) {
#ifdef _WIN32
        BASS_ASIO_Free();
#endif
        BASS_Free();
    } else if(m_currentOutputDevice.driver == OutputDriver::BASS_WASAPI) {
#ifdef _WIN32
        BASS_WASAPI_Free();
#endif
        BASS_Free();
    }
}

void SoundEngine::restart() { setOutputDevice(m_currentOutputDevice); }

void SoundEngine::update() {}

bool SoundEngine::play(Sound *snd, float pan, float pitch) {
    if(!m_bReady || snd == NULL || !snd->isReady()) return false;

    if(!snd->isOverlayable() && snd->isPlaying()) {
        return false;
    }

    if(snd->isOverlayable() && snd_restrict_play_frame.getBool()) {
        if(engine->getTime() <= snd->getLastPlayTime()) {
            return false;
        }
    }

    HCHANNEL channel = snd->getChannel();
    if(channel == 0) {
        debugLog("SoundEngine::play() failed to get channel, errorcode %i\n", BASS_ErrorGetCode());
        return false;
    }

    pan = clamp<float>(pan, -1.0f, 1.0f);
    pitch = clamp<float>(pitch, 0.0f, 2.0f);
    BASS_ChannelSetAttribute(channel, BASS_ATTRIB_VOL, snd->m_fVolume);
    BASS_ChannelSetAttribute(channel, BASS_ATTRIB_PAN, pan);
    BASS_ChannelSetAttribute(channel, BASS_ATTRIB_NORAMP, snd->isStream() ? 0 : 1);
    if(pitch != 1.0f) {
        const float semitonesShift = lerp<float>(-60.0f, 60.0f, pitch / 2.0f);
        float freq = snd_freq.getFloat();
        BASS_ChannelGetAttribute(channel, BASS_ATTRIB_FREQ, &freq);
        BASS_ChannelSetAttribute(channel, BASS_ATTRIB_FREQ, std::pow(2.0f, (semitonesShift / 12.0f)) * freq);
    }

    BASS_ChannelFlags(channel, snd->isLooped() ? BASS_SAMPLE_LOOP : 0, BASS_SAMPLE_LOOP);

    if(isMixing()) {
        if(BASS_Mixer_ChannelGetMixer(channel) != 0) return false;

        auto flags = BASS_MIXER_DOWNMIX | BASS_MIXER_NORAMPIN;
        if(snd->isStream()) {
            flags |= BASS_STREAM_AUTOFREE;
        }

        if(!BASS_Mixer_StreamAddChannel(g_bassOutputMixer, channel, flags)) {
            debugLog("BASS_Mixer_StreamAddChannel() failed (%i)!\n", BASS_ErrorGetCode());
            return false;
        }
    } else {
        if(BASS_ChannelIsActive(channel) == BASS_ACTIVE_PLAYING) return false;

        if(!BASS_ChannelPlay(channel, false)) {
            debugLog("SoundEngine::play() couldn't BASS_ChannelPlay(), errorcode %i\n", BASS_ErrorGetCode());
            return false;
        }
    }

    snd->m_bPaused = false;
    snd->setLastPlayTime(engine->getTime());
    return true;
}

bool SoundEngine::play3d(Sound *snd, Vector3 pos) {
    if(!m_bReady || snd == NULL || !snd->isReady() || !snd->is3d()) return false;
    if(snd_restrict_play_frame.getBool() && engine->getTime() <= snd->getLastPlayTime()) return false;

    HCHANNEL channel = snd->getChannel();
    if(channel == 0) {
        debugLog("SoundEngine::play3d() failed to get channel, errorcode %i\n", BASS_ErrorGetCode());
        return false;
    }

    BASS_3DVECTOR bassPos = BASS_3DVECTOR(pos.x, pos.y, pos.z);
    if(!BASS_ChannelSet3DPosition(channel, &bassPos, NULL, NULL)) {
        debugLog("SoundEngine::play3d() couldn't BASS_ChannelSet3DPosition(), errorcode %i\n", BASS_ErrorGetCode());
        return false;
    }

    BASS_Apply3D();
    if(BASS_ChannelPlay(channel, false)) {
        snd->setLastPlayTime(engine->getTime());
        return true;
    } else {
        debugLog("SoundEngine::play3d() couldn't BASS_ChannelPlay(), errorcode %i\n", BASS_ErrorGetCode());
        return false;
    }
}

void SoundEngine::pause(Sound *snd) {
    if(!m_bReady || snd == NULL || !snd->isReady()) return;
    if(!snd->isStream()) {
        engine->showMessageError("Programmer Error", "Called pause on a sample!");
        return;
    }
    if(!snd->isPlaying()) return;

    auto pan = snd->getPan();
    auto pos = snd->getPositionMS();
    auto loop = snd->isLooped();
    auto speed = snd->getSpeed();

    // Calling BASS_Mixer_ChannelRemove automatically frees the stream due
    // to BASS_STREAM_AUTOFREE. We need to reinitialize it.
    snd->reload();

    snd->setPositionMS(pos);
    snd->setSpeed(speed);
    snd->setPan(pan);
    snd->setLoop(loop);
    snd->m_bPaused = true;
}

void SoundEngine::stop(Sound *snd) {
    if(!m_bReady || snd == NULL || !snd->isReady()) return;

    // This will stop all samples, then re-init to be ready for a play()
    snd->reload();
}

bool SoundEngine::setOutputDevice(OUTPUT_DEVICE device) {
    bool success = true;

    unsigned long prevMusicPositionMS = 0;
    if(bancho.osu != nullptr) {
        if(bancho.osu->isInPlayMode()) {
            // Kick the player out of play mode, since restarting SoundEngine during gameplay is not supported.
            // XXX: Make OsuBeatmap work without a running SoundEngine
            bancho.osu->getSelectedBeatmap()->fail();
            bancho.osu->getSelectedBeatmap()->stop(true);
        } else if(bancho.osu->getSelectedBeatmap()->getMusic() != NULL) {
            prevMusicPositionMS = bancho.osu->getSelectedBeatmap()->getMusic()->getPositionMS();
        }
    }

    auto previous = m_currentOutputDevice;
    if(!initializeOutputDevice(device)) {
        success = false;
        initializeOutputDevice(previous);
    }

    if(bancho.osu != nullptr) {
        bancho.osu->m_optionsMenu->m_outputDeviceLabel->setText(getOutputDeviceName());
        bancho.osu->getSkin()->reloadSounds();
        bancho.osu->m_optionsMenu->onOutputDeviceResetUpdate();

        // start playing music again after audio device changed
        if(!bancho.osu->isInPlayMode() && bancho.osu->getSelectedBeatmap()->getMusic() != NULL) {
            bancho.osu->getSelectedBeatmap()->unloadMusic();
            bancho.osu->getSelectedBeatmap()->select();  // (triggers preview music play)
            bancho.osu->getSelectedBeatmap()->getMusic()->setPositionMS(prevMusicPositionMS);
        }
    }

    return success;
}

void SoundEngine::setVolume(float volume) {
    if(!m_bReady) return;

    m_fVolume = clamp<float>(volume, 0.0f, 1.0f);
    if(m_currentOutputDevice.driver == OutputDriver::BASS_ASIO) {
#ifdef _WIN32
        BASS_ASIO_ChannelSetVolume(false, 0, m_fVolume);
        BASS_ASIO_ChannelSetVolume(false, 1, m_fVolume);
#endif
    } else if(m_currentOutputDevice.driver == OutputDriver::BASS_WASAPI) {
#ifdef _WIN32
        BASS_WASAPI_SetVolume(BASS_WASAPI_CURVE_WINDOWS | BASS_WASAPI_VOL_SESSION, m_fVolume);
#endif
    } else {
        // 0 (silent) - 10000 (full).
        BASS_SetConfig(BASS_CONFIG_GVOL_SAMPLE, (DWORD)(m_fVolume * 10000));
        BASS_SetConfig(BASS_CONFIG_GVOL_STREAM, (DWORD)(m_fVolume * 10000));
        BASS_SetConfig(BASS_CONFIG_GVOL_MUSIC, (DWORD)(m_fVolume * 10000));
    }
}

void SoundEngine::set3dPosition(Vector3 headPos, Vector3 viewDir, Vector3 viewUp) {
    if(!m_bReady) return;

    BASS_3DVECTOR bassHeadPos = BASS_3DVECTOR(headPos.x, headPos.y, headPos.z);
    BASS_3DVECTOR bassViewDir = BASS_3DVECTOR(viewDir.x, viewDir.y, viewDir.z);
    BASS_3DVECTOR bassViewUp = BASS_3DVECTOR(viewUp.x, viewUp.y, viewUp.z);

    if(!BASS_Set3DPosition(&bassHeadPos, NULL, &bassViewDir, &bassViewUp))
        debugLog("SoundEngine::set3dPosition() couldn't BASS_Set3DPosition(), errorcode %i\n", BASS_ErrorGetCode());
    else
        BASS_Apply3D();  // apply the changes
}

void SoundEngine::onFreqChanged(UString oldValue, UString newValue) {
    (void)oldValue;
    (void)newValue;
    restart();
}

std::vector<OUTPUT_DEVICE> SoundEngine::getOutputDevices() {
    std::vector<OUTPUT_DEVICE> outputDevices;

    for(size_t i = 0; i < m_outputDevices.size(); i++) {
        if(m_outputDevices[i].enabled) {
            outputDevices.push_back(m_outputDevices[i]);
        }
    }

    return outputDevices;
}
