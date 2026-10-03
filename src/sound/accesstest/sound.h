/******************************************************************************\
 * Copyright (c) 2026
 *
 * Author(s):
 *  JamulusAccess
 *
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
\******************************************************************************/

#pragma once

// Test-only sound interface (CONFIG+=accesstest): no audio device is opened. A timer thread
// calls the audio callback with a quiet tone, so a client can connect to a local server and
// show levels while the accessibility work is tested off screen. Never part of a release.

#include <chrono>
#include <cmath>
#include <thread>
#include "../../util.h"
#include "../../global.h"
#include "../soundbase.h"

class CSound : public CSoundBase
{
    Q_OBJECT

public:
    CSound ( void ( *fpNewCallback ) ( CVector<int16_t>& psData, void* arg ), void* arg, const bool, const QString& ) :
        CSoundBase ( "Access test (no device)", fpNewCallback, arg )
    {}

    virtual ~CSound() { Stop(); }

    virtual int Init ( const int iNewPrefMonoBufferSize )
    {
        iBufferSizeMono = iNewPrefMonoBufferSize;
        vecsBuffer.Init ( 2 * iBufferSizeMono );
        return iBufferSizeMono;
    }

    virtual void Start()
    {
        CSoundBase::Start();
        QThread::start();
    }

    virtual void Stop()
    {
        CSoundBase::Stop();
        wait();
    }

protected:
    virtual void run()
    {
        const auto period = std::chrono::microseconds ( 1000000LL * iBufferSizeMono / SYSTEM_SAMPLE_RATE_HZ );
        auto       next   = std::chrono::steady_clock::now();
        double     phase  = 0;

        while ( bRun )
        {
            for ( int i = 0; i < iBufferSizeMono; i++ )
            {
                const int16_t sample = static_cast<int16_t> ( 3000 * std::sin ( phase ) ); // about -20 dB
                phase += 2 * 3.14159265358979 * 440 / SYSTEM_SAMPLE_RATE_HZ;
                vecsBuffer[2 * i]     = sample;
                vecsBuffer[2 * i + 1] = sample;
            }
            ProcessCallback ( vecsBuffer );

            next += period;
            std::this_thread::sleep_until ( next );
        }
    }

    int              iBufferSizeMono = 128;
    CVector<int16_t> vecsBuffer;
};
