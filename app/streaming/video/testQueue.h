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
        // cluster based on ideas, add comments

        testQueue(); // rename to: playoutBuffer, jitterQueue, other ideas

        void enqueue(AVFrame *frame);
        int getQueueSize();
        static std::shared_ptr<testQueue> GetInstance(); // pointer of queue instance

        // make private
        std::mutex queue_mutex;
        std::mutex sleepTime_mutex;
        std::mutex queueMon_mutex;
        std::mutex offset_mutex;

        void IPolicyQueue(AVFrame *frame); // add verbs to these
        void EPolicyQueue(AVFrame *frame);
        microseconds getSleepTimeValue();
        AVFrame *dequeue();
        bool dequeueing();
        microseconds getFrameTimeMicrosecond();

        bool getQueueMonitor();
        void setQueueMonitor(bool qmIn);

        int getSleepOffVal();
        void adjustOffsetVal();

        void setQueueType(bool queueType);
        bool getQueueType();

private:
        // Organize/cluster based on connected Ideas
        // give class vars m_ or _
        // snake_case for vars

        std::queue<AVFrame *> myqueue;
        static std::shared_ptr<testQueue> queueInstance;
        microseconds averageInterFrameTimeMicro;
        microseconds avg; // avg Frame Time
        int counter;      // count of frames seen

        bool queueMon;      // whether queue monitoring is on/off
        int sleepOffsetVal; // current sleep offset from Queue Monitoring

        microseconds renderFrameTimeMicro; // sum of interframe times microseconds(probably should change)

        microseconds micro_start; // program start in microsecond

        microseconds lastFrameTimeMicro;

        bool queueTypeIsIPolicy;

        int queueState; // State 0 = queueing, State 1 = Dequeuing
        double alpha;
};
