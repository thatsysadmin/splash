/*
 * Copyright (C) 2015 Emmanuel Durand
 *
 * This file is part of Splash.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Splash is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Splash.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * @listener.h
 * The Listener class, to input sound
 */

#ifndef SPLASH_LISTENER_H
#define SPLASH_LISTENER_H

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <portaudio.h>

#include "./core/constants.h"

#include "./core/attribute.h"
#include "./core/graph_object.h"
#include "./sound/sound_engine.h"

namespace Splash
{

/*************/
class Listener final : public GraphObject
{
  public:
    /**
     * Constructor
     */
    Listener();

    /**
     * Destructor
     */
    ~Listener() final;

    /**
     * Constructors/operators
     */
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener(Listener&&) = delete;
    Listener& operator=(Listener&&) = delete;

    /**
     * Safe bool idiom
     */
    explicit operator bool() const { return _ready; }

    /**
     * Add a buffer to the playing queue
     * \return Return false if there was an error
     */
    template <typename T>
    bool readFromQueue(std::vector<T>& buffer);

    /**
     * Set the audio parameters
     */
    void setParameters(uint32_t channels, uint32_t sampleRate, Sound_Engine::SampleFormat format, const std::string& deviceName = "");

  private:
    static constexpr uint32_t _ringbufferSize{4 * 1024 * 1024};
    Sound_Engine _engine;
    bool _ready{false};
    unsigned int _channels{2};
    unsigned int _sampleRate{0};
    Sound_Engine::SampleFormat _sampleFormat{Sound_Engine::SAMPLE_FMT_FLT};
    bool _planar{false};
    std::string _deviceName{""};
    size_t _sampleSize{2};
    bool _abortCallback{false};

    std::array<uint8_t, _ringbufferSize> _ringBuffer;
    std::atomic_int _ringWritePosition{0};
    std::atomic_int _ringReadPosition{0};
    std::atomic_int _ringUnusedSpace{0};

    /**
     * Free all PortAudio resources
     */
    void freeResources();

    /**
     * Initialize PortAudio resources
     */
    void initResources();

    /**
     * PortAudio callback
     */
    static int portAudioCallback(
        const void* in, void* out, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData);

    /**
     * Register new functors to modify attributes
     */
    void registerAttributes();
};

/*************/
template <typename T>
bool Listener::readFromQueue(std::vector<T>& buffer)
{
    if (buffer.size() == 0)
        return false;

    int readPosition = _ringReadPosition;
    int writePosition = _ringWritePosition;
    int unusedSpace = _ringUnusedSpace;

    // If the ring buffer is not filled enough, fill with zeros instead
    int delta = 0;
    if (writePosition >= readPosition)
        delta = writePosition - readPosition;
    else
        delta = _ringbufferSize - unusedSpace - readPosition + writePosition;

    int step = buffer.size() * sizeof(T);
    if (delta < step)
    {
        return false;
    }
    // Else, we copy the values and move the read position
    else
    {
        int effectiveSpace = _ringbufferSize - unusedSpace;
        int ringBufferEndLength = effectiveSpace - readPosition;

        if (step <= ringBufferEndLength)
        {
            std::copy(&_ringBuffer[readPosition], &_ringBuffer[readPosition] + step, buffer.data());
            readPosition = readPosition + step;
        }
        else
        {
            std::copy(&_ringBuffer[readPosition], &_ringBuffer[effectiveSpace], buffer.data());
            std::copy(&_ringBuffer[0], &_ringBuffer[step - ringBufferEndLength], reinterpret_cast<char*>(buffer.data()) + ringBufferEndLength);
            readPosition = step - ringBufferEndLength;
        }

        _ringReadPosition = readPosition;
    }

    return true;
}

} // namespace Splash

#endif
