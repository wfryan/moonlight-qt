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
        //void SetPrefs(std::string logFileName, LogLevel level);
        //void Log(std::string input, LogLevel level);
        void enqueue(AVFrame* frame);
        void queueSize();
        int getQueueSize();
        static std::shared_ptr<testQueue> GetInstance(); //pointer of logger instance
        std::mutex queue_mutex;
        void IPolicyQueue(AVFrame* frame);
        void EPolicyQueue(AVFrame *frame);
        milliseconds getFrameTime();
        AVFrame* dequeue();
        bool dequeueing();
        milliseconds currentLatency;
        microseconds getFrameTimeMicrosecond();
        microseconds currentLatencyMicro;
        microseconds avg;
        //void run();


    private:
        std::queue<AVFrame*> myqueue; 
        static std::shared_ptr<testQueue> queueInstance;
        

};
