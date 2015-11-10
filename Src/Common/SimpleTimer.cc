/******************************************************************************
 * Copyright (c) 2014-2015, Beijing HengShengDongYang Technology Ltd. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#include "Common/SimpleTimer.h"

SimpleTimer::SimpleTimer()
    : timerThread(boost::thread(boost::bind(&SimpleTimer::TimerThreadFunc, this))),
    interval(0), timerCB(NULL), timerCBPara(NULL)
{
}

SimpleTimer::~SimpleTimer()
{
    Stop();
}

void SimpleTimer::Start(unsigned int intvl, TimerCallBack cb, void* cbPara)
{
    interval = intvl;
    timerCB = cb;
    timerCBPara = cbPara;
    condStart.notify_one();
}

void SimpleTimer::Stop()
{
    cond.notify_one();
    timerThread.try_join_for(boost::chrono::milliseconds(interval));
    interval = 0;
    timerCB = NULL;
    timerCBPara = NULL;
}

void SimpleTimer::TimerThreadFunc()
{
    while (1) {
        if (interval == 0) {// the timer has not been initiated
            boost::unique_lock<boost::mutex> lock(mtxStart);
            condStart.wait(lock); // wait until the timer is started
            continue;
        } else {
            boost::unique_lock<boost::mutex> lock(mtx);
            if (!cond.timed_wait(lock, boost::posix_time::milliseconds(interval))) {
//             if (boost::cv_status::timeout == cond.wait_for(lock, boost::chrono::milliseconds(interval))) {
                // if timed out, then execute the Timer CallBack
                if (timerCB) {
                    timerCB(timerCBPara);
                }
            } else {
                // if some signal is sent to this condition, then stop the thread
                break;
            }
        }
    }
}
