#include "testQueue.h"
#include "logger.h"

using namespace std::chrono;
using Clock = steady_clock;
using std::this_thread::sleep_for;

int counter = 0; //count of frames seen

milliseconds renderFrameTime = milliseconds(0); //sum of interframe times (should change)
microseconds renderFrameTimeMicro = microseconds(0); //sum of interframe times microseconds(probably should change)

milliseconds start = duration_cast<milliseconds>(system_clock::now().time_since_epoch()); //program start
microseconds micro_start = duration_cast<microseconds>(system_clock::now().time_since_epoch()); //program start in microsecond

milliseconds lastFrameTime = milliseconds::zero(); //Time of last frame arrival
microseconds lastFrameTimeMicro = microseconds::zero(); //Time of last frame arrival in micro seconds

milliseconds averageInterFrameTime = milliseconds(0); //average interframe time of the stream
microseconds averageInterFrameTimeMicro = microseconds(0); //Average interframe time of the stream

microseconds avg = microseconds(16670); //Target frametime (should pick a different name)

std::shared_ptr<testQueue> testQueue::queueInstance; //singleton queue instance
std::queue<AVFrame *> myqueue; //actual queue
std::mutex queue_mutex; //mutex lock

int queueState = 0; // State 0 = queueing, State 1 = Dequeuing
double alpha = 0.9;

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
    milliseconds interFrameTime = timeArrived - lastFrameTime;
    microseconds interFrameTimeMicro = timeArrivedMicro - lastFrameTimeMicro;

    if (counter >= 4)
    {
        averageInterFrameTime = renderFrameTime / counter;
        averageInterFrameTimeMicro = renderFrameTimeMicro / counter;

    }

    logger->Log(("The last frame time is " + std::to_string(lastFrameTime.count()) + " Latency is " + std::to_string(interFrameTime.count())), LogLevel::INFO);
    logger->Log("current latency:" + std::to_string(averageInterFrameTime.count()), LogLevel::INFO);
    logger->Log("latency:" + std::to_string(interFrameTime.count()), LogLevel::INFO);

    if (interFrameTimeMicro > (avg + averageInterFrameTimeMicro) && frame->key_frame == 0 && queueState == 1)
    {
        logger->Log(("Frame arrived late deleting" + std::to_string(frame->pts)), LogLevel::INFO);
        av_frame_free(&frame);

        lastFrameTime = timeArrived;
        lastFrameTimeMicro = timeArrivedMicro;
        counter++; //count of frames

        if(counter > 1){
            renderFrameTime += interFrameTime;
            renderFrameTimeMicro += interFrameTimeMicro;
            avg = duration_cast<microseconds>((avg * 0.99) + (1 - 0.99) * interFrameTimeMicro);
        }
    }
    else
    {
        queue_mutex.lock(); //LOCKING SELF
        myqueue.push(frame);
        counter++; //count of frames

        if(counter > 1){
            renderFrameTime += interFrameTime;
            renderFrameTimeMicro += interFrameTimeMicro;
            avg = duration_cast<microseconds>((avg * 0.95) + (1 - 0.95) * interFrameTimeMicro); //adjust the target frame time based on the current interframe time 
        }

        lastFrameTime = timeArrived;
        lastFrameTimeMicro = timeArrivedMicro;
        
        queue_mutex.unlock(); //UNLOCKING SELF

        logger->LogGraph(std::to_string(getQueueSize()), "queueSize");
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


void testQueue::EPolicyQueue(AVFrame *frame)
{
    auto logger = Logger::GetInstance();
    milliseconds timeArrived = getFrameTime();
    microseconds timeArrivedMicro = getFrameTimeMicrosecond();
    milliseconds interFrameTime = timeArrived - lastFrameTime; //time between frames
    microseconds interFrameTimeMicro = timeArrivedMicro - lastFrameTimeMicro; //time between frames for our calculations

    queue_mutex.lock(); //LOCKING SELF
    myqueue.push(frame);
    counter++;

    if(counter > 1){
        renderFrameTime += interFrameTime;
        renderFrameTimeMicro += interFrameTimeMicro;
        avg = duration_cast<microseconds>((avg * 0.95) + (1 - 0.95) * interFrameTimeMicro); //adjust the target interframe time based on the current interframe time
    }

    logger->Log("The last frame time is " + std::to_string(lastFrameTime.count()), LogLevel::INFO); 
    logger->Log("Average interframe time:" + std::to_string(averageInterFrameTime.count()), LogLevel::INFO);
    lastFrameTime = timeArrived; //setting previous frame time of arrival
    lastFrameTimeMicro = timeArrivedMicro;

    queue_mutex.unlock(); //UNLOCKING SELF

    logger->Log("interFrameTime:" + std::to_string(interFrameTime.count()), LogLevel::INFO);
    logger->LogGraph(std::to_string(getQueueSize()), "queueSize");
    logger->Log(("Queueing Frame" + std::to_string(frame->pts)), LogLevel::INFO);
    logger->Log("Queue Size after queueing: " + std::to_string(getQueueSize()), LogLevel::INFO);

}