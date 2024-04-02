#include "playoutBuffer.h"
#include "logger.h"
#include "iostream"
#include "fstream"
#include "string"
using namespace std::chrono;
using Clock = steady_clock;
using std::this_thread::sleep_for;

std::shared_ptr<playoutBuffer> playoutBuffer::queueInstance; // instance of this class that can be referred to elsewhere

auto logger = Logger::GetInstance(); // creates an instance of local logger

playoutBuffer::playoutBuffer()
{
    // queue variables
    std::queue<AVFrame *> m_buffer_queue; // buffer queue of frames

    // frametime/frame counter variables
    m_frame_counter = 0;                    // count of frames seen
    m_frame_time_sum = microseconds(0);     // sum of interframe times microseconds

    // queue monitoring content
    m_last_frame_arrived_time = microseconds::zero(); // Time of last frame arrival in microseconds
    m_constant_sleep_offset = 0;                   // constant sleep offset value for queue monitor
    m_sleep_offset_val = m_constant_sleep_offset;     // adjusting sleep offset value
    m_queue_monitor_target = 0;                       // Starting OffsetVal


    // time tracking variables
    micro_start = duration_cast<microseconds>(system_clock::now().time_since_epoch()); // program start in microseconds

    //Frame type counter
    deltaFrameCount = 0;
    // policy specific variables
    m_average_frametime = microseconds(8000); // Target frametime (old avg)
    m_alpha = 0.95;                             // alpha value in sleep time calculations
    m_queue_limit = 2;
    m_queue_monitor_on = false;
    m_queue_state = Filling;
    readConfig("C:/Users/claypool/Desktop/my_test/config.txt");
}

bool playoutBuffer::getQueueMonitor()
{
    queueMon_mutex.lock();
    bool qm = m_queue_monitor_on;
    queueMon_mutex.unlock();
    return qm;
}

void playoutBuffer::setQueueMonitor(bool qmIn, int target)
{
    queueMon_mutex.lock();
    m_queue_monitor_on = qmIn;
    m_queue_monitor_target = target;
    queueMon_mutex.unlock();
}


void playoutBuffer::incrementDeltaCount(){
    deltaFrameCount++;
}
void playoutBuffer::resetDeltaCount(){
    deltaFrameCount = 0;
}
int playoutBuffer::getDeltaCount(){
    int delta = deltaFrameCount;
    return delta;
}
playoutBuffer::playoutBuffer::Policies playoutBuffer::getQueueType()
{
    queue_mutex.lock();
    playoutBuffer::Policies qType = policy;
    queue_mutex.unlock();
    return qType;
}

void playoutBuffer::setQueueType(playoutBuffer::Policies queueType)
{
    queue_mutex.lock();
    policy = queueType;
    queue_mutex.unlock();
}

// Get the current sleep offset value
int playoutBuffer::getSleepOffVal()
{
    offset_mutex.lock(); // may need mutex because getSleepOffVal() and adjust OffsetVal() are called many times
    int tempVal = m_sleep_offset_val;
    offset_mutex.unlock();
    return tempVal;
}



// adjust the sleep offset value based on queue size.
void playoutBuffer::adjustOffsetVal()
{
    int queueLength = getQueueSize();


    offset_mutex.lock(); // may need mutex because getSleepOffVal() and adjust OffsetVal() are called many times
    if (queueLength > m_queue_monitor_target)
    {
        m_sleep_offset_val = m_max_offset; //3000
    }
    else if (queueLength < m_queue_monitor_target)
    {
        m_sleep_offset_val = m_min_offset; //1250
    } else {
        m_sleep_offset_val = m_correct_offset; //2500
    }

    offset_mutex.unlock();
}

// gets the current size of the queue
int playoutBuffer::getQueueSize()
{
    queue_mutex.lock();
    int size = (int)m_buffer_queue.size();
    queue_mutex.unlock();
    return size;
}

// finds whether the queue should be dequeueing or not
bool playoutBuffer::dequeueing()
{
    switch (policy)
    {
    case IPolicy:
    {
        switch (m_queue_state)
        {
        case Filling:
            if (getQueueSize() >= m_queue_limit) // queue has reached the fill limit
            {
                m_queue_state = Draining;
                return true;
            }

            return false;
            break;
        case Draining:
            if (getQueueSize() == 0) // empty queue
            {
                m_queue_state = Filling;
                return false;
            }

            return true;
            break;
        case justFreed:
            m_queue_state = Filling;
            return false;
        default:
            return false;
            break;
        }
        break;
    }
    case EPolicy:
    {
        if (getQueueSize() > 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    default:
    {
        return false;
    }
    }
}

std::shared_ptr<playoutBuffer> playoutBuffer::GetInstance()
{
    if (queueInstance == nullptr)
    {
        queueInstance = std::shared_ptr<playoutBuffer>(new playoutBuffer());
    }
    return queueInstance;
}

AVFrame *playoutBuffer::dequeue()
{
    AVFrame *currentf;

    queue_mutex.lock();
    currentf = m_buffer_queue.front();
    m_buffer_queue.pop();
    logger->LogGraph(std::to_string(m_buffer_queue.size()), "queueSize");
    queue_mutex.unlock();

    logger->Log("Dequeue Frame", LogLevel::INFO);
    logger->Log("Queue Size after dequeueing: " + std::to_string(getQueueSize()), LogLevel::INFO);

    return currentf;
}

microseconds playoutBuffer::getAverageFrameTime() // old getSleepTimeValue
{
    microseconds tempSleepTimeVal;
    sleepTime_mutex.lock();
    tempSleepTimeVal = m_average_frametime;
    sleepTime_mutex.unlock();
    return tempSleepTimeVal;
}

void playoutBuffer::enqueueIPolicy(AVFrame *frame)
{
    microseconds time_arrived = getElapsedTime();
    microseconds time_between_frames = time_arrived - m_last_frame_arrived_time;

    if (m_frame_counter > 0)
    {
        m_frame_time_sum += time_between_frames;
        m_average_frametime = duration_cast<microseconds>((m_average_frametime * m_alpha) + (1 - m_alpha) * time_between_frames); // adjust the target frame time based on the current interframe time
    }

    if (time_between_frames > (microseconds(33333)) && frame->key_frame == 0)
    {
        logger->Log(("Frame arrived late deleting" + std::to_string(frame->pts)), LogLevel::INFO);
        av_frame_free(&frame);
        //m_queue_state = justFreed;
    }
    else
    {
        queue_mutex.lock(); // LOCKING SELF
        m_buffer_queue.push(frame);

    }

    m_frame_counter++; // count of frames
    m_last_frame_arrived_time = time_arrived;

    queue_mutex.unlock(); // UNLOCKING SELF

    logger->LogGraph(std::to_string(getQueueSize()), "queueSize");
}

// gets the amount of time elapsed from the start of the program
microseconds playoutBuffer::getElapsedTime() // rename function, getElapsedTime, getTime, etc.
{
    using namespace std::chrono;
    microseconds current_microseconds = duration_cast<microseconds>(system_clock::now().time_since_epoch());
    microseconds time_ms = current_microseconds - micro_start;
    return time_ms;
}

void playoutBuffer::enqueueEPolicy(AVFrame *frame)
{
    microseconds time_arrived = getElapsedTime();
    microseconds time_between_frames = time_arrived - m_last_frame_arrived_time; // time between frames for our calculations (maybe remove 'inter')

    queue_mutex.lock(); // LOCKING SELF
    m_buffer_queue.push(frame);
    queue_mutex.unlock();

    sleepTime_mutex.lock();

    // can probably remove this m_frame_counter logic

    m_average_frametime = duration_cast<microseconds>((m_average_frametime * m_alpha) + (1 - m_alpha) * time_between_frames); // adjust the target interframe time based on the current interframe time

    sleepTime_mutex.unlock();
    logger->LogGraph(std::to_string(time_between_frames.count()), "interFrameTimeEnqueue");
    // setting previous frame time of arrival
    m_last_frame_arrived_time = time_arrived;

    m_frame_counter++; // may be needed for logging

    logger->LogGraph(std::to_string(getQueueSize()), "queueSize");
}

void playoutBuffer::readConfig(std::string input){

    setQueueType(EPolicy);
    m_queue_monitor_on = false;
    
    bool qmon;
    std::ifstream file;
    file.open(input, std::ios::in);
    std::string config;
    
    std::string configs[6];
    int count = 0;
    if (file.is_open())
    {
        for (std::string line; getline(file,config,',');)
        {
            configs[count]=config;
            count++;
        }

        if (configs[0]=="EPolicy")
        {
            setQueueType(EPolicy);
        }else{
            setQueueType(IPolicy);
        }

        if (configs[1]=="true")
        {
            qmon = true;
        }else{
            qmon = false;
        }
        m_correct_offset = std::stoi(configs[3]);
        m_min_offset = std::stoi(configs[4]);
        m_max_offset = std::stoi(configs[5]);


        setQueueMonitor(qmon,std::stoi(configs[2]));
    }
    file.close();
    
}
