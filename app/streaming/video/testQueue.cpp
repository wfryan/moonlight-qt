#include "testQueue.h"
#include "logger.h"

using namespace std::chrono;

std::shared_ptr<testQueue> testQueue::queueInstance;
std::queue<AVFrame*> myqueue; 
std::mutex queue_mutex;
long lastFrameTime = 0;
// State 0 = queueing, State 1 = Dequeuing  
int queueState = 0;
long currentLatency = 0;

void testQueue::enqueue(AVFrame* frame){
    auto logger = Logger::GetInstance();
    queue_mutex.lock();
    myqueue.push(frame);
    logger->Log(("Queueing Frame" + std::to_string(frame->pts)), LogLevel::INFO);
    queue_mutex.unlock();
}

void testQueue::dequeue(){
    auto logger = Logger::GetInstance();
    if(myqueue.size() >= 30){
        myqueue.pop();
        logger->Log("Dequeue Frame", LogLevel::INFO);
    }else{
        return;
    }
}

void testQueue::queueSize(){
    auto logger = Logger::GetInstance();
    logger->Log(("Current queue size:" + std::to_string(myqueue.size())), LogLevel::INFO);
}

int testQueue::getQueueSize(){
    return myqueue.size();
}


std::shared_ptr<testQueue> testQueue::GetInstance(){
    if(queueInstance == nullptr){
        queueInstance = std::shared_ptr<testQueue>(new testQueue());
    }
    return queueInstance;
}

void testQueue::IPolicy(long minqueue, int displaylat, int frametime){
    long start_time = high_resolution_clock::now();
    auto logger = Logger::GetInstance();
    switch (queueState)
    {
    case 0  :
        if (myqueue.size() >= minqueue){
            Pacer::renderFrameDequeue(myqueue.pop());
            logger->Log("Dequeue Frame", LogLevel::INFO);
            queueState = 1;
            long end_time = high_resolution_clock::now();
            long run_time = duration_cast<milliseconds>(end_time - start_time);
            if(run_time > duration_cast<milliseconds>(16.67)){
                sleep(0);
            }else{
                sleep(duration_cast<milliseconds>(end_time - start_time));    
            }
        }
        break;
    case 1:
        Pacer::renderFrameDequeue(myqueue.pop());        
        logger->Log("Dequeue Frame", LogLevel::INFO);
        long end_time = high_resolution_clock::now();
        long run_time = duration_cast<milliseconds>(end_time - start_time);
        if(run_time > duration_cast<milliseconds>(16.67)){
            sleep(0);
        }else{
            sleep(duration_cast<milliseconds>(end_time - start_time));    
        }
        break;
    }
}

void testQueue::IPolicyQueue(AVFrame* frame, int maxlatency){
    long timeArrived= getFrameTime();
    long latency = timeArrived - lastFrameTime;
    auto logger = Logger::GetInstance();
    currentLatency = latency;
    logger->Log(("The last frame time is "+std::to_string(lastFrameTime) +" Latency is "+ std::to_string(latency)), LogLevel::INFO);
    if (latency > maxlatency)
    {
        av_frame_free(&frame);
        logger->Log(("Frame arrived late deleting" + std::to_string(frame->pts)), LogLevel::INFO);
        lastFrameTime = timeArrived;
    }else{
        myqueue.push(frame);
        lastFrameTime = timeArrived;
        logger->Log(("Queueing Frame" + std::to_string(frame->pts)), LogLevel::INFO);
    }  
}

long testQueue::getFrameTime(){
    using namespace std::chrono;
    milliseconds ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch());
    long output = ms.count();
    return output;
}


