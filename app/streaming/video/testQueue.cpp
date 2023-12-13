#include "testQueue.h"
#include "logger.h"

using namespace std::chrono;
using Clock = steady_clock;
using std::this_thread::sleep_for;

int counter = 0;
bool haveLatency = false;
milliseconds renderFrameTime = milliseconds(0);
microseconds renderFrameTimeMicro = microseconds(0);

milliseconds start = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
microseconds micro_start = duration_cast<microseconds>(system_clock::now().time_since_epoch());

std::shared_ptr<testQueue> testQueue::queueInstance;
std::queue<AVFrame *> myqueue;
std::mutex queue_mutex;
milliseconds lastFrameTime = milliseconds::zero();
microseconds lastFrameTimeMicro = microseconds::zero();
// State 0 = queueing, State 1 = Dequeuing
int queueState = 0;
milliseconds currentLatency = milliseconds(0);
microseconds currentLatencyMicro = microseconds(0);
milliseconds dequeue_latency = milliseconds(0);

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
}

int testQueue::getQueueSize()
{
    queue_mutex.lock();
    int size = myqueue.size();
    queue_mutex.unlock();
    return size;
}

bool testQueue::dequeueing()
{
    auto logger = Logger::GetInstance();
    switch (queueState)
    {
    case 0:
        // logger->Log("Case 0", LogLevel::INFO);

        // note: this actually sets the size of the queue
        if (getQueueSize() == 5)
        {
            queueState = 1;
            return true;
        }
        return false;
        break;
    case 1:
        // logger->Log("Case 1", LogLevel::INFO);
        if (getQueueSize() == 0)
        {
            return false;
        }

        return true;
        break;
    }
    return false;
}

std::shared_ptr<testQueue> testQueue::GetInstance()
{
    if (queueInstance == nullptr)
    {
        queueInstance = std::shared_ptr<testQueue>(new testQueue());
    }
    return queueInstance;
}

AVFrame *testQueue::IPolicy()
{
    // milliseconds start = getFrameTime();
    auto logger = Logger::GetInstance();
    // milliseconds fpms = milliseconds(17);
    AVFrame *currentf;
    switch (queueState)
    {
    case 0:

        queue_mutex.lock();
        // queue_Pacer->renderFrameDequeue(myqueue.front());
       currentf = myqueue.front();
        myqueue.pop();
        queue_mutex.unlock();
        logger->Log("Dequeue Frame", LogLevel::INFO);
        logger->Log(std::to_string(myqueue.size()), LogLevel::GRAPHING);

        queueState = 1;
        // milliseconds end = getFrameTime();
        // milliseconds run_time = (end - start);
        //  logger->Log("Run TIme " + std::to_string(run_time.count()) + " fpms "+std::to_string(fpms.count())+ " end "+std::to_string(end.count())+ " start "+std::to_string(start.count()), LogLevel::INFO);
        //  if (run_time < fpms)
        //  {
        //      logger->Log("Sleep ", LogLevel::INFO);
        //      sleep_for(fpms-run_time);
        //  }
        return currentf;

        break;
    case 1:
        queue_mutex.lock();
        // queue_Pacer->renderFrameDequeue(myqueue.front());
        currentf = myqueue.front();
        myqueue.pop();
        queue_mutex.unlock();
        logger->Log("Dequeue Frame", LogLevel::INFO);
        logger->Log("Queue Size after dequeue: " + std::to_string(getQueueSize()), LogLevel::INFO);
        // milliseconds end = getFrameTime();
        // milliseconds run_time = (end - start);
        // logger->Log("Run TIme " + std::to_string(run_time.count()) + " fpms "+std::to_string(fpms.count())+ " end "+std::to_string(end.count())+ " start "+std::to_string(start.count()), LogLevel::INFO);
        // logger->Log("fpms - run_time: " + std::to_string((fpms-run_time).count()), LogLevel::INFO);
        // if (run_time < fpms)
        // {
        //     logger->Log("Sleep ", LogLevel::INFO);
        //     sleep_for(fpms-run_time);
        // }
        return currentf;
        break;
    }
}

void testQueue::IPolicyQueue(AVFrame *frame)
{
    auto logger = Logger::GetInstance();
    milliseconds timeArrived = getFrameTime();
    microseconds timeArrivedMicro = getFrameTimeMicrosecond();

    milliseconds latency = timeArrived - lastFrameTime;
    microseconds latencyMicro = timeArrivedMicro - lastFrameTimeMicro;

    if (counter >= 4)
    {
        currentLatency = renderFrameTime / counter;
        currentLatencyMicro = renderFrameTimeMicro / counter;
        haveLatency = true;
    }

    logger->Log(("The last frame time is " + std::to_string(lastFrameTime.count()) + " Latency is " + std::to_string(latency.count())), LogLevel::INFO);
    if (latency > currentLatency && frame->key_frame == 0 && haveLatency)
    {
        logger->Log("Counter:" + std::to_string(counter), LogLevel::INFO);
        logger->Log("current latency:" + std::to_string(currentLatency.count()), LogLevel::INFO);
        logger->Log("latency:" + std::to_string(latency.count()), LogLevel::INFO);
        logger->Log(("Frame arrived late deleting" + std::to_string(frame->pts)), LogLevel::INFO);
        av_frame_free(&frame);
        //logger->Log(("Frame arrived late deleting" + std::to_string(frame->pts)), LogLevel::INFO);
        lastFrameTime = timeArrived;
        lastFrameTimeMicro = timeArrivedMicro
        counter++;
        renderFrameTime += latency;
        renderFrameTimeMicro += latencyMicro
    }
    else
    {

        queue_mutex.lock();
        myqueue.push(frame);
        counter++;
        renderFrameTime += latency;
        renderFrameTimeMicro += latencyMicro
        queue_mutex.unlock();
        lastFrameTime = timeArrived;
        lastFrameTimeMicro = timeArrivedMicro
        logger->Log(("Queueing Frame" + std::to_string(frame->pts)), LogLevel::INFO);
        logger->Log("Queue Size after queueing: " + std::to_string(getQueueSize()), LogLevel::INFO);
        logger->Log(std::to_string(myqueue.size()), LogLevel::GRAPHING);
    }
}

milliseconds testQueue::getFrameTime()
{
    using namespace std::chrono;
    milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
    milliseconds timems = ms - start;
    return timems;
}

microseconds testQueue::getFrameTimeMicrosecond()
{
    using namespace std::chrono;
    microseconds ms = duration_cast<microseconds>(system_clock::now().time_since_epoch());
    microseconds timems = ms - micro_start;
    return timems;
}
