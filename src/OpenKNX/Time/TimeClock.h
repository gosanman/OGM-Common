#pragma once
#include <Arduino.h>
#include <ctime>

namespace OpenKNX
{
    namespace Time
    {
        // Abstrakte Basis-Klasse für die Zeitverwaltung
        class TimeClock
        {
        public:
            virtual ~TimeClock() {}

            virtual void setup() = 0;
            virtual void loop() = 0;
            virtual void setTime(time_t epoch, unsigned long millisReceivedTimestamp) = 0;
            virtual time_t getTime() = 0;
            virtual bool isRunning() = 0;
        };

        // Globale Factory-Funktion, um die Zeitverwaltung zu setzen
        static TimeClock *currentClock = nullptr;

        inline void setCurrentTimeClock(TimeClock *clock)
        {
            currentClock = clock;
        }

        inline TimeClock *getCurrentTimeClock()
        {
            return currentClock;
        }
    } // namespace Time
} // namespace OpenKNXe OpenKNX