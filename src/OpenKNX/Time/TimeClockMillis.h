#pragma once
#include "TimeClock.h"
namespace OpenKNX
{
    namespace Time
    {
        class TimeClockMillis : public TimeClock
        {
            unsigned long _timeSetMs = 0;
            time_t _offset = 0;
          public:
            inline void setup() override {}
            inline void loop() override {}
            inline void setTime(time_t epoch, unsigned long millisReceivedTimestamp) override
            {
                _offset = epoch;
                _timeSetMs = millisReceivedTimestamp;
            }
            inline time_t getTime() override
            {
                return _offset + (millis() - _timeSetMs) / 1000;
            }
            inline bool isRunning() override { return true; }
        };
    } // namespace Time
} // namespace OpenKNX