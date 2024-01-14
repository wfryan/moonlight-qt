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

AVFrame *testQueue::dequeue()
{
    auto logger = Logger::GetInstance();
    AVFrame *currentf;

    queue_mutex.lock();
    currentf = myqueue.front();
    myqueue.pop();
    logger->LogGraph(std::to_string(myqueue.size()), "queueSize");
    queue_mutex.unlock();

    logger->Log("Dequeue Frame", LogLevel::INFO);
    logger->Log("Queue Size after dequeueing: " + std::to_string(getQueueSize()), LogLevel::INFO);



    return currentf;
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

    logger->Log("current latency:" + std::to_string(currentLatency.count()), LogLevel::INFO);
    logger->Log("latency:" + std::to_string(latency.count()), LogLevel::INFO);
    if (latencyMicro > (microseconds(15000) + currentLatencyMicro) && frame->key_frame == 0 && queueState == 1)
    {
//        logger->Log("Counter:" + std::to_string(counter), LogLevel::INFO);
//        logger->Log("current latency:" + std::to_string(currentLatency.count()), LogLevel::INFO);
//        logger->Log("latency:" + std::to_string(latency.count()), LogLevel::INFO);
//        logger->LogGraph(std::to_string(latency.count()), "latency");
        logger->Log(("Frame arrived late deleting" + std::to_string(frame->pts)), LogLevel::INFO);
        av_frame_free(&frame);
        //logger->Log(("Frame arrived late deleting" + std::to_string(frame->pts)), LogLevel::INFO);
        lastFrameTime = timeArrived;
        lastFrameTimeMicro = timeArrivedMicro;
        counter++;
        if(counter > 1){
            renderFrameTime += latency;
            renderFrameTimeMicro += latencyMicro;
        }

        
    }
    else
    {

        queue_mutex.lock();
        myqueue.push(frame);
        counter++;
        if(counter > 1){
            renderFrameTime += latency;
            renderFrameTimeMicro += latencyMicro;
        }

        lastFrameTime = timeArrived;
        lastFrameTimeMicro = timeArrivedMicro;

        logger->LogGraph(std::to_string(myqueue.size()), "queueSize");
        
        queue_mutex.unlock();

        logger->Log(("Queueing Frame" + std::to_string(frame->pts)), LogLevel::INFO);
        logger->Log("Queue Size after queueing: " + std::to_string(getQueueSize()), LogLevel::INFO);

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
