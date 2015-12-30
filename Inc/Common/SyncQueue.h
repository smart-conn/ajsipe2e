/******************************************************************************
 * Copyright AllSeen Alliance. All rights reserved.
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
#ifndef SYNCQUEUE_H_
#define SYNCQUEUE_H_

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class SyncQueue
{
public:

    SyncQueue()
    {
        RequestToEnd = false;  
        EnqueueData = true;
    }
    void Enqueue(const T& data)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if(EnqueueData)
        {
            m_queue.push(data);
            m_cond.notify_one();
        }

    } 


    bool TryDequeue(T& result)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        while (m_queue.empty() && (! RequestToEnd)) 
        { 
            m_cond.wait(lock);
        }

        if( RequestToEnd )
        {
            DoEndActions();
            return false;
        }

        result= m_queue.front(); m_queue.pop();

        return true;
    }

    void StopQueue()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        RequestToEnd =  true;
        m_cond.notify_one();       
    }

    int Size()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();

    }

private:

    void DoEndActions()
    {
        EnqueueData = false;

        while (!m_queue.empty())  
        {
            m_queue.pop();
        }
    }



    std::queue<T> m_queue;              // Use STL queue to store data
    std::mutex m_mutex;               // The mutex to synchronise on
    std::condition_variable m_cond;            // The condition to wait for

    bool RequestToEnd;
    bool EnqueueData;
};


#endif
