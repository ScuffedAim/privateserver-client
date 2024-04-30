//================ Copyright (c) 2015, PG, All rights reserved. =================//
//
// Purpose:		fps timer
//
// $NoKeywords: $linuxtime $os
//===============================================================================//

#ifdef __linux__

#ifndef LINUXTIMER_H
#define LINUXTIMER_H

#include <time.h>

#include "Timer.h"

class LinuxTimer : public BaseTimer {
   public:
    LinuxTimer();
    virtual ~LinuxTimer() { ; }

    virtual void start() override;
    virtual void update() override;

    virtual inline double getDelta() const override { return m_delta; }
    virtual inline double getElapsedTime() const override { return m_elapsedTime; }
    virtual inline u64 getElapsedTimeMS() const override { return m_elapsedTimeMS; }

   private:
    timespec m_startTime;
    timespec m_currentTime;

    double m_delta;
    double m_elapsedTime;
    u64 m_elapsedTimeMS;
};

#endif

#endif
