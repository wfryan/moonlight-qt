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

class playoutBuffer
{

public:
        // class functions
        playoutBuffer();                                     // constructor
        static std::shared_ptr<playoutBuffer> GetInstance(); // pointer of queue instance

        // queue variables
        int getQueueSize();
        void enqueueIPolicy(AVFrame *frame); // enqueue of I policy
        void enqueueEPolicy(AVFrame *frame); // enqueue of E policy
        AVFrame *dequeue();                  // dequeue from queue
        bool dequeueing();                   // boolean check for if queue ready for dequeuing
        enum QueueState
        {
                Filling,
                Draining,
                justFreed
        }; // Whether the Queue is draining or not
        void readConfig(std::string input);
        // time-related
        microseconds getElapsedTime();

        // queueMonitor
        bool getQueueMonitor();
        void setQueueMonitor(bool qmIn, int target);
        int getSleepOffVal();
        void adjustOffsetVal();

        void incrementDeltaCount();
        void resetDeltaCount();
        int getDeltaCount();
        // PolicySpecific
        microseconds getAverageFrameTime();
        enum Policies
        {
                IPolicy,
                EPolicy
        }; // Queue policies. this simplifies identifying queue type
        void setQueueType(Policies queueType);
        Policies getQueueType();


private:
        // queue variables
        std::queue<AVFrame *> m_buffer_queue;
        static std::shared_ptr<playoutBuffer> queueInstance;
        int deltaFrameCount;

        // frametime/frame counter variables
        int m_frame_counter;           // count of frames seen
        microseconds m_frame_time_sum; // sum of interframe times microseconds

        // queue monitoring content
        bool m_queue_monitor_on;     // whether queue monitoring is on/off
        int m_constant_sleep_offset; // the constant that is modified by sleep offset
        int m_sleep_offset_val;      // current sleep offset from Queue Monitoring
        int m_queue_monitor_target;  // target value of frames in queue

        int m_correct_offset;
        int m_min_offset;
        int m_max_offset;


        // time tracking variables
        microseconds micro_start;               // program start in microsecond
        microseconds m_last_frame_arrived_time; // used to track when the previous frame arrived

        // Policy-specific variables
        microseconds m_average_frametime; // target frametime (old avg)
        Policies policy;
        QueueState m_queue_state;
        double m_alpha;    // alpha variable for average calculator
        int m_queue_limit; // amount before queue begins to drain normally

        // mutexes
        std::mutex queue_mutex;
        std::mutex sleepTime_mutex;
        std::mutex queueMon_mutex;
        std::mutex offset_mutex;
};
