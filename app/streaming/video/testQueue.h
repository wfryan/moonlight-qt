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
#include <libavutil/frame.h>

using namespace std::chrono;

class testQueue
{

public:
        void enqueue(AVFrame *frame);
        int getQueueSize();
        static std::shared_ptr<testQueue> GetInstance(); // pointer of logger instance

        // should these be public or private?
        std::mutex queue_mutex;
        std::mutex sleepTime_mutex;
        std::mutex queueMon_mutex;
        std::mutex offset_mutex;

        void IPolicyQueue(AVFrame *frame);
        void EPolicyQueue(AVFrame *frame);
        microseconds getSleepTimeValue();
        AVFrame *dequeue();
        bool dequeueing();
        bool EPolicyDequeuing();
        microseconds getFrameTimeMicrosecond();
        testQueue();
        bool getQueueMonitor();
        void setQueueMonitor(bool qmIn);

        int getSleepOffVal();
        void adjustOffsetVal();

        void setQueueType(bool queueType);
        bool getQueueType();

private:
        std::queue<AVFrame *> myqueue;
        static std::shared_ptr<testQueue> queueInstance;
        microseconds averageInterFrameTimeMicro;
        microseconds avg;
        int counter; // count of frames seen

        bool queueMon;
        int sleepOffsetVal;

        microseconds renderFrameTimeMicro; // sum of interframe times microseconds(probably should change)

        microseconds micro_start; // program start in microsecond

        microseconds lastFrameTimeMicro;

        bool queueTypeIsIPolicy;

        int queueState; // State 0 = queueing, State 1 = Dequeuing
        double alpha;
};
