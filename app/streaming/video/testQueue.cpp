#include "testQueue.h"
#include "logger.h"

using namespace std::chrono;
using Clock = steady_clock;
using std::this_thread::sleep_for;

std::shared_ptr<testQueue> testQueue::queueInstance;

testQueue::testQueue(){

    counter = 0; //count of frames seen

    renderFrameTimeMicro = microseconds(0); //sum of interframe times microseconds(probably should change)

    micro_start = duration_cast<microseconds>(system_clock::now().time_since_epoch()); //program start in microsecond

    lastFrameTimeMicro = microseconds::zero(); //Time of last frame arrival in micro seconds

    averageInterFrameTimeMicro = microseconds(0); //Average interframe time of the stream

    avg = microseconds(16670); //Target frametime (should pick a different name)

    queueState = 0; // State 0 = queueing, State 1 = Dequeuing
    alpha = 0.9;

    std::queue<AVFrame *> myqueue; //actual queue
    std::mutex queue_mutex; //mutex lock

}


void testQueue::enqueue(AVFrame *frame)
{
    auto logger = Logger::GetInstance();
    queue_mutex.lock();
    myqueue.push(frame);
    logger->Log(("Queueing Frame" + std::to_string(frame->pts)), LogLevel::INFO);
    queue_mutex.unlock();
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

bool testQueue::EPolicyDequeuing(){
    if (getQueueSize() > 0)
    {
        return true;
    } else {
        return false;
    }
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

microseconds testQueue::getSleepTimeValue(){
    microseconds tempSleepTimeVal;
    sleepTime_mutex.lock();
    tempSleepTimeVal = avg;
    sleepTime_mutex.unlock();
    return tempSleepTimeVal;


}

void testQueue::IPolicyQueue(AVFrame *frame)
{
    auto logger = Logger::GetInstance();
    microseconds timeArrivedMicro = getFrameTimeMicrosecond();
    microseconds interFrameTimeMicro = timeArrivedMicro - lastFrameTimeMicro;

    if (counter >= 4)
    {
        averageInterFrameTimeMicro = renderFrameTimeMicro / counter;
    }

    if (interFrameTimeMicro > (avg + averageInterFrameTimeMicro) && frame->key_frame == 0 && queueState == 1)
    {
        logger->Log(("Frame arrived late deleting" + std::to_string(frame->pts)), LogLevel::INFO);
        av_frame_free(&frame);

        lastFrameTimeMicro = timeArrivedMicro;
        counter++; //count of frames

        if(counter > 1){
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
            renderFrameTimeMicro += interFrameTimeMicro;
            avg = duration_cast<microseconds>((avg * 0.95) + (1 - 0.95) * interFrameTimeMicro); //adjust the target frame time based on the current interframe time
        }

        lastFrameTimeMicro = timeArrivedMicro;

        queue_mutex.unlock(); //UNLOCKING SELF

        logger->LogGraph(std::to_string(getQueueSize()), "queueSize");

    }
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
    microseconds timeArrivedMicro = getFrameTimeMicrosecond();
    microseconds interFrameTimeMicro = timeArrivedMicro - lastFrameTimeMicro; //time between frames for our calculations

    queue_mutex.lock(); //LOCKING SELF
    myqueue.push(frame);
    queue_mutex.unlock();


    sleepTime_mutex.lock();

    if(counter == 1){
        avg = interFrameTimeMicro;
    }else if(counter > 2){
        avg = duration_cast<microseconds>((avg * 0.95) + (1 - 0.95) * interFrameTimeMicro); //adjust the target interframe time based on the current interframe time
    }


    sleepTime_mutex.unlock();
    logger->LogGraph(std::to_string(interFrameTimeMicro.count()), "interFrameTimeEnqueue");
    //setting previous frame time of arrival
    lastFrameTimeMicro = timeArrivedMicro;

    counter++;

    logger->LogGraph(std::to_string(getQueueSize()), "queueSize");

}
