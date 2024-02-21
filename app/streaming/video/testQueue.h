#include <string>
#include <iostream>
#include <memory>
#include <fstream>
#include <chrono>
#include <ctime>
#include <queue>
#include <mutex>
#include <thread>
#include "decoder.h"
#include "ffmpeg-renderers/renderer.h"
//#include "ffmpeg-renderers/pacer/pacer.h"
#include <libavutil/frame.h>
// #include <unistd.h>

using namespace std::chrono;



class testQueue{

    public:

        void enqueue(AVFrame* frame);
        void queueSize();
        int getQueueSize();
        static std::shared_ptr<testQueue> GetInstance(); //pointer of logger instance
        std::mutex queue_mutex;
        std::mutex sleepTime_mutex;
        std::mutex queueMon_mutex;
        std::mutex offset_mutex;
        bool queueMon;
        void IPolicyQueue(AVFrame* frame);
        void EPolicyQueue(AVFrame *frame);
        milliseconds getFrameTime();
        microseconds getSleepTimeValue();
        AVFrame* dequeue();
        bool dequeueing();
        bool EPolicyDequeuing();
        bool getQueueMonitor();
        void setQueueMonitor(bool qmIn);
        milliseconds averageInterFrameTime;
        microseconds getFrameTimeMicrosecond();
        microseconds averageInterFrameTimeMicro;
        microseconds avg;
        int sleepOffsetVal;
        int getSleepOffVal();
        void adjustOffsetVal();


    private:
        std::queue<AVFrame*> myqueue; 
        static std::shared_ptr<testQueue> queueInstance;
        

};
