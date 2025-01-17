#pragma once

#include "rtaudio/RtAudio.h"
#include "common/audio/audio_sink.h"
#include <mutex>
#include <vector>

class RtAudioSink : public audio::AudioSink
{
private:
    int d_samplerate;

    std::mutex audio_mtx;
    std::vector<int16_t> audio_buff;

    RtAudio rt_dac;
    RtAudio::StreamParameters rt_parameters;

    static int audio_callback(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                              double streamTime, RtAudioStreamStatus status, void *userData);

public:
    RtAudioSink();
    ~RtAudioSink();
    void set_samplerate(int samplerate);
    void start();
    void stop();

    void push_samples(int16_t *samples, int nsamples);

public:
    static std::string getID() { return "rtaudio"; }
    static std::shared_ptr<audio::AudioSink> getInstance() { return std::make_shared<RtAudioSink>(); }
};