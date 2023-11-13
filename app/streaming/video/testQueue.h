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
#include "ffmpeg-renderers/pacer/pacer.h"
#include <libavutil/frame.h>
#include <unistd.h>

// enum class LogLevel {
//     INFO = 0,
// 	DEBUG = 1,
// 	WARN = 3,
// 	ERROR = 4,
// 	yaDunFuckedUp = 5,
// };


class testQueue{

    public:
        //void SetPrefs(std::string logFileName, LogLevel level);
        //void Log(std::string input, LogLevel level);
        void enqueue(AVFrame* frame);
        void dequeue();
        void queueSize();
        int getQueueSize();
        static std::shared_ptr<testQueue> GetInstance(); //pointer of logger instance
        std::mutex queue_mutex;
        void IPolicyQueue(AVFrame* frame, int maxlatency);
        long getFrameTime();
        void IPolicy(long minqueue, int displaylat, int frametime);
        //void run();


    private:
        std::queue<AVFrame*> myqueue; 
        static std::shared_ptr<testQueue> queueInstance;
        

};