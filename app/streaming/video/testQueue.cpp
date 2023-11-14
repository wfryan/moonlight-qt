#include "testQueue.h"
#include "logger.h"

using namespace std::chrono;
using Clock = steady_clock;
using std::this_thread::sleep_for;

milliseconds start = duration_cast< milliseconds >(system_clock::now().time_since_epoch());

std::shared_ptr<testQueue> testQueue::queueInstance;
std::queue<AVFrame *> myqueue;
std::mutex queue_mutex;
milliseconds lastFrameTime = milliseconds::zero();
// State 0 = queueing, State 1 = Dequeuing
int queueState = 0;
milliseconds currentLatency;
Pacer *queue_Pacer;

void testQueue::enqueue(AVFrame *frame)
{
    auto logger = Logger::GetInstance();
    queue_mutex.lock();
    myqueue.push(frame);
    logger->Log(("Queueing Frame" + std::to_string(frame->pts)), LogLevel::INFO);
    queue_mutex.unlock();
}

void testQueue::dequeue()
{
    auto logger = Logger::GetInstance();
    if (myqueue.size() >= 30)
    {
        myqueue.pop();
        logger->Log("Dequeue Frame", LogLevel::INFO);
    }
    else
    {
        return;
    }
}

void testQueue::queueSize()
{
    auto logger = Logger::GetInstance();
    logger->Log(("Current queue size:" + std::to_string(myqueue.size())), LogLevel::INFO);
}

int testQueue::getQueueSize()
{
    return myqueue.size();
}

std::shared_ptr<testQueue> testQueue::GetInstance()
{
    if (queueInstance == nullptr)
    {
        queueInstance = std::shared_ptr<testQueue>(new testQueue());
    }
    return queueInstance;
}

void testQueue::IPolicy(long unsigned int minqueue)
{
    time_point<Clock> start = Clock::now();
    auto logger = Logger::GetInstance();
    milliseconds fpms = milliseconds(17);
    switch (queueState)
    {
    case 0:
        if (myqueue.size() >= minqueue)
        {
            queue_mutex.lock();
            queue_Pacer->renderFrameDequeue(myqueue.front());
            myqueue.pop();
            queue_mutex.unlock();
            logger->Log("Dequeue Frame", LogLevel::INFO);
            queueState = 1;
            time_point<Clock> end = Clock::now();
            milliseconds run_time = duration_cast<milliseconds>(end - start);
            if (!(run_time > fpms))
            {
                sleep_for(run_time);
            }
        }
        break;
    case 1:
        queue_mutex.lock();
        queue_Pacer->renderFrameDequeue(myqueue.front());
        myqueue.pop();
        queue_mutex.unlock();
        logger->Log("Dequeue Frame", LogLevel::INFO);
        time_point<Clock> end = Clock::now();
        milliseconds run_time = duration_cast<milliseconds>(end - start);
        if (!(run_time > fpms))
        {
            sleep_for(run_time);
        }
        break;
    }
}

void testQueue::IPolicyQueue(AVFrame *frame)
{
    auto logger = Logger::GetInstance();
    milliseconds timeArrived = getFrameTime();
    milliseconds latency = timeArrived - lastFrameTime;
    currentLatency = latency;
    logger->Log(("The last frame time is " + std::to_string(lastFrameTime.count()) + " Latency is " + std::to_string(latency.count())), LogLevel::INFO);
    if (latency > currentLatency * 2)
    {
        av_frame_free(&frame);
        logger->Log(("Frame arrived late deleting" + std::to_string(frame->pts)), LogLevel::INFO);
        lastFrameTime = timeArrived;
    }
    else
    {
        queue_mutex.lock();
        myqueue.push(frame);
        queue_mutex.unlock();
        lastFrameTime = timeArrived;
        logger->Log(("Queueing Frame" + std::to_string(frame->pts)), LogLevel::INFO);
    }
}

milliseconds testQueue::getFrameTime()
{
    using namespace std::chrono;
    milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
    milliseconds timems = ms - start; 
    return timems;
}
