#include "testQueue.h"
#include "logger.h"

std::shared_ptr<testQueue> testQueue::queueInstance;
std::queue<AVFrame*> myqueue; 
std::mutex queue_mutex;

void testQueue::enqueue(AVFrame* frame){
    auto logger = Logger::GetInstance();
    queue_mutex.lock();
    myqueue.push(frame);
    logger->Log(("Queueing Frame" + std::to_string(frame->pts)), LogLevel::INFO);
    queue_mutex.unlock();
}

void testQueue::dequeue(){
    logger->Log("Dequeue Frame", LogLevel::INFO);
    myqueue.pop();
    // auto logger = Logger::GetInstance();
    // if(myqueue.size() >= 30){
    //     myqueue.pop();
    //     logger->Log("Dequeue Frame", LogLevel::INFO);
    // }else{
    //     return;
    // }
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


