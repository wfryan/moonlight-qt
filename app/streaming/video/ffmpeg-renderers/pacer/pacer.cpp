#include "pacer.h"
#include "streaming/streamutils.h"

#include "streaming/video/playoutBuffer.h"
#include "streaming/video/logger.h"

using std::this_thread::sleep_for;

#ifdef Q_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <VersionHelpers.h>
#include "dxvsyncsource.h"
#endif

#ifdef HAS_WAYLAND
#include "waylandvsyncsource.h"
#endif

#include <SDL_syswm.h>

// Limit the number of queued frames to prevent excessive memory consumption
// if the V-Sync source or renderer is blocked for a while. It's important
// that the sum of all queued frames between both pacing and rendering queues
// must not exceed the number buffer pool size to avoid running the decoder
// out of available decoding surfaces.
#define MAX_QUEUED_FRAMES 4

// We may be woken up slightly late so don't go all the way
// up to the next V-sync since we may accidentally step into
// the next V-sync period. It also takes some amount of time
// to do the render itself, so we can't render right before
// V-sync happens.
#define TIMER_SLACK_MS 3

bool queueThreadMade = false;

Pacer::Pacer(IFFmpegRenderer *renderer, PVIDEO_STATS videoStats) : m_RenderThread(nullptr),
                                                                   m_VsyncThread(nullptr),
                                                                   m_Stopping(false),
                                                                   m_VsyncSource(nullptr),
                                                                   m_VsyncRenderer(renderer),
                                                                   m_MaxVideoFps(0),
                                                                   m_DisplayFps(0),
                                                                   m_VideoStats(videoStats),
                                                                   m_DequeueThread(nullptr)
{
}

microseconds lastFrameTimeDequeueMicro = microseconds::zero(); // Time of last frame arrival in micro seconds

Pacer::~Pacer()
{
    m_Stopping = true;

    // Stop the V-sync thread
    if (m_VsyncThread != nullptr)
    {
        m_PacingQueueNotEmpty.wakeAll();
        m_VsyncSignalled.wakeAll();
        SDL_WaitThread(m_VsyncThread, nullptr);
    }

    // Stop V-sync callbacks
    delete m_VsyncSource;
    m_VsyncSource = nullptr;

    // Stop the render thread
    if (m_RenderThread != nullptr)
    {
        m_RenderQueueNotEmpty.wakeAll();
        SDL_WaitThread(m_RenderThread, nullptr);
    }
    else
    {
        // Notify the renderer that it is being destroyed soon
        // NB: This must happen on the same thread that calls renderFrame().
        m_VsyncRenderer->cleanupRenderContext();
    }

    // Delete any remaining unconsumed frames
    while (!m_RenderQueue.isEmpty())
    {
        AVFrame *frame = m_RenderQueue.dequeue();
        av_frame_free(&frame);
    }
    while (!m_PacingQueue.isEmpty())
    {
        AVFrame *frame = m_PacingQueue.dequeue();
        av_frame_free(&frame);
    }
}

void Pacer::renderOnMainThread()
{
    // Ignore this call for renderers that work on a dedicated render thread
    if (m_RenderThread != nullptr)
    {
        return;
    }

    m_FrameQueueLock.lock();

    if (!m_RenderQueue.isEmpty())
    {
        AVFrame *frame = m_RenderQueue.dequeue();
        m_FrameQueueLock.unlock();

        renderFrame(frame);
    }
    else
    {
        m_FrameQueueLock.unlock();
    }
}

int Pacer::vsyncThread(void *context)
{
    Pacer *me = reinterpret_cast<Pacer *>(context);

#if SDL_VERSION_ATLEAST(2, 0, 9)
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
#else
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
#endif

    bool async = me->m_VsyncSource->isAsync();
    while (!me->m_Stopping)
    {
        if (async)
        {
            // Wait for the VSync source to invoke signalVsync() or 100ms to elapse
            me->m_FrameQueueLock.lock();
            me->m_VsyncSignalled.wait(&me->m_FrameQueueLock, 100);
            me->m_FrameQueueLock.unlock();
        }
        else
        {
            // Let the VSync source wait in the context of our thread
            me->m_VsyncSource->waitForVsync();
        }

        if (me->m_Stopping)
        {
            break;
        }

        me->handleVsync(1000 / me->m_DisplayFps);
    }

    return 0;
}

int Pacer::renderThread(void *context)
{
    Pacer *me = reinterpret_cast<Pacer *>(context);

    // auto logger = Logger::GetInstance();
    // logger->Log("Before making dequeue thread", LogLevel::INFO);
    // me->m_DequeueThread = SDL_CreateThread(Pacer::renderFrameDequeueThreadProc, "dequeue thread", me);
    // logger->Log("after making dequeue thread", LogLevel::INFO);

    if (SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH) < 0)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Unable to set render thread to high priority: %s",
                    SDL_GetError());
    }

    while (!me->m_Stopping)
    {
        // Wait for the renderer to be ready for the next frame
        me->m_VsyncRenderer->waitToRender();

        // Acquire the frame queue lock to protect the queue and
        // the not empty condition
        me->m_FrameQueueLock.lock();

        // Wait for a frame to be ready to render
        while (!me->m_Stopping && me->m_RenderQueue.isEmpty())
        {
            me->m_RenderQueueNotEmpty.wait(&me->m_FrameQueueLock);
        }

        if (me->m_Stopping)
        {
            // Exit this thread
            me->m_FrameQueueLock.unlock();
            break;
        }

        AVFrame *frame = me->m_RenderQueue.dequeue();
        me->m_FrameQueueLock.unlock();

        me->renderFrame(frame);
    }

    // Notify the renderer that it is being destroyed soon
    // NB: This must happen on the same thread that calls renderFrame().
    me->m_VsyncRenderer->cleanupRenderContext();

    return 0;
}

void Pacer::enqueueFrameForRenderingAndUnlock(AVFrame *frame)
{
    dropFrameForEnqueue(m_RenderQueue);
    m_RenderQueue.enqueue(frame);

    m_FrameQueueLock.unlock();

    if (m_RenderThread != nullptr)
    {
        m_RenderQueueNotEmpty.wakeOne();
    }
    else
    {
        SDL_Event event;

        // For main thread rendering, we'll push an event to trigger a callback
        event.type = SDL_USEREVENT;
        event.user.code = SDL_CODE_FRAME_READY;
        SDL_PushEvent(&event);
    }
}

// Called in an arbitrary thread by the IVsyncSource on V-sync
// or an event synchronized with V-sync
void Pacer::handleVsync(int timeUntilNextVsyncMillis)
{
    // Make sure initialize() has been called
    SDL_assert(m_MaxVideoFps != 0);

    m_FrameQueueLock.lock();

    // If the queue length history entries are large, be strict
    // about dropping excess frames.
    int frameDropTarget = 1;

    // If we may get more frames per second than we can display, use
    // frame history to drop frames only if consistently above the
    // one queued frame mark.
    if (m_MaxVideoFps >= m_DisplayFps)
    {
        for (int queueHistoryEntry : m_PacingQueueHistory)
        {
            if (queueHistoryEntry <= 1)
            {
                // Be lenient as long as the queue length
                // resolves before the end of frame history
                frameDropTarget = 3;
                break;
            }
        }

        // Keep a rolling 500 ms window of pacing queue history
        if (m_PacingQueueHistory.count() == m_DisplayFps / 2)
        {
            m_PacingQueueHistory.dequeue();
        }

        m_PacingQueueHistory.enqueue(m_PacingQueue.count());
    }

    // Catch up if we're several frames ahead
    while (m_PacingQueue.count() > frameDropTarget)
    {
        AVFrame *frame = m_PacingQueue.dequeue();

        // Drop the lock while we call av_frame_free()
        m_FrameQueueLock.unlock();
        m_VideoStats->pacerDroppedFrames++;
        av_frame_free(&frame);
        m_FrameQueueLock.lock();
    }

    if (m_PacingQueue.isEmpty())
    {
        // Wait for a frame to arrive or our V-sync timeout to expire
        if (!m_PacingQueueNotEmpty.wait(&m_FrameQueueLock, SDL_max(timeUntilNextVsyncMillis, TIMER_SLACK_MS) - TIMER_SLACK_MS))
        {
            // Wait timed out - unlock and bail
            m_FrameQueueLock.unlock();
            return;
        }

        if (m_Stopping)
        {
            m_FrameQueueLock.unlock();
            return;
        }
    }

    // Place the first frame on the render queue
    enqueueFrameForRenderingAndUnlock(m_PacingQueue.dequeue());
}

bool Pacer::initialize(SDL_Window *window, int maxVideoFps, bool enablePacing)
{
    m_MaxVideoFps = maxVideoFps;
    m_DisplayFps = StreamUtils::getDisplayRefreshRate(window);
    m_RendererAttributes = m_VsyncRenderer->getRendererAttributes();

    if (enablePacing)
    {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Frame pacing: target %d Hz with %d FPS stream",
                    m_DisplayFps, m_MaxVideoFps);

        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        if (!SDL_GetWindowWMInfo(window, &info))
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_GetWindowWMInfo() failed: %s",
                         SDL_GetError());
            return false;
        }

        switch (info.subsystem)
        {
#ifdef Q_OS_WIN32
        case SDL_SYSWM_WINDOWS:
            // Don't use D3DKMTWaitForVerticalBlankEvent() on Windows 7, because
            // it blocks during other concurrent DX operations (like actually rendering).
            if (IsWindows8OrGreater())
            {
                m_VsyncSource = new DxVsyncSource(this);
            }
            break;
#endif

#if defined(SDL_VIDEO_DRIVER_WAYLAND) && defined(HAS_WAYLAND)
        case SDL_SYSWM_WAYLAND:
            m_VsyncSource = new WaylandVsyncSource(this);
            break;
#endif

        default:
            // Platforms without a VsyncSource will just render frames
            // immediately like they used to.
            break;
        }

        SDL_assert(m_VsyncSource != nullptr || !(m_RendererAttributes & RENDERER_ATTRIBUTE_FORCE_PACING));

        if (m_VsyncSource != nullptr && !m_VsyncSource->initialize(window, m_DisplayFps))
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Vsync source failed to initialize. Frame pacing will not be available!");
            delete m_VsyncSource;
            m_VsyncSource = nullptr;
        }
    }
    else
    {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Frame pacing disabled: target %d Hz with %d FPS stream",
                    m_DisplayFps, m_MaxVideoFps);
    }

    if (m_VsyncSource != nullptr)
    {
        m_VsyncThread = SDL_CreateThread(Pacer::vsyncThread, "PacerVsync", this);
    }

    if (m_VsyncRenderer->isRenderThreadSupported())
    {
        m_RenderThread = SDL_CreateThread(Pacer::renderThread, "PacerRender", this);
    }

    // creation of queue Thread
    if (!queueThreadMade)
    {

        m_DequeueThread = SDL_CreateThread(Pacer::renderFrameDequeueThreadProc, "dequeue thread", this);

        queueThreadMade = true;
    }

    return true;
}

void Pacer::signalVsync()
{
    m_VsyncSignalled.wakeOne();
}

// ThreadProc function
int Pacer::renderFrameDequeueThreadProc(void *context)
{
    Pacer *me = reinterpret_cast<Pacer *>(context);
    me->renderFrameDequeueThread();
    return 0;
}

// Dequeue Function
void Pacer::renderFrameDequeueThread()
{
    auto logger = Logger::GetInstance();
    auto playout_buffer = playoutBuffer::GetInstance();
    logger->Log("Render Frame function called", LogLevel::INFO);
    while (true)
    {
        // if playoutBuffer is in the dequeueing phase
        if (playout_buffer->dequeueing()) // fix
        {
            microseconds start = playout_buffer->getElapsedTime();
            //  note: value of 20 currently has no effect, set by dequeuing variable
            AVFrame *framede = playout_buffer->dequeue();
            microseconds end = playout_buffer->getElapsedTime();
            microseconds run_time = (end - start);

            // parse for unecessary log statements
            logger->LogGraph(std::to_string((start - lastFrameTimeDequeueMicro).count()), "interFrameTimeDequeue");

            lastFrameTimeDequeueMicro = playout_buffer->getElapsedTime();

            logger->tempCounterFramesOut++;
            logger->LogGraph(std::to_string(logger->tempCounterFramesOut), "framesOut");

            logger->LogGraph(std::to_string(playout_buffer->getQueueSize()), "queueSize");

            /*****************************************Pre-existing Code (bar logging messages) ***********************************/
            Uint32 beforeRender = SDL_GetTicks();
            logger->Log("Time before render " + std::to_string(beforeRender) + " Frame pkt " + std::to_string(framede->pkt_dts), LogLevel::INFO);
            m_VideoStats->totalPacerTime += beforeRender - framede->pkt_dts;

            // Render it

            m_VsyncRenderer->renderFrame(framede);
            logger->Log("after render frame ", LogLevel::INFO);
            Uint32 afterRender = SDL_GetTicks();

            m_VideoStats->totalRenderTime += afterRender - beforeRender;
            m_VideoStats->renderedFrames++;
            av_frame_free(&framede);

            // Drop frames if we have too many queued up for a while
            m_FrameQueueLock.lock();

            int frameDropTarget;

            if (m_RendererAttributes & RENDERER_ATTRIBUTE_NO_BUFFERING)
            {
                // Renderers that don't buffer any frames but don't support waitToRender() need us to buffer
                // an extra frame to ensure they don't starve while waiting to present.
                frameDropTarget = 1;
            }
            else
            {
                frameDropTarget = 0;
                for (int queueHistoryEntry : m_RenderQueueHistory)
                {
                    if (queueHistoryEntry == 0)
                    {
                        // Be lenient as long as the queue length
                        // resolves before the end of frame history
                        frameDropTarget = 2;
                        break;
                    }
                }

                // Keep a rolling 500 ms window of render queue history
                if (m_RenderQueueHistory.count() == m_MaxVideoFps / 2)
                {
                    m_RenderQueueHistory.dequeue();
                }

                m_RenderQueueHistory.enqueue(m_RenderQueue.count());
            }

            // Catch up if we're several frames ahead
            while (m_RenderQueue.count() > frameDropTarget)
            {
                AVFrame *framede = m_RenderQueue.dequeue();

                // Drop the lock while we call av_frame_free()
                m_FrameQueueLock.unlock();
                m_VideoStats->pacerDroppedFrames++; // take a look at this
                av_frame_free(&framede);
                m_FrameQueueLock.lock();
            }

            m_FrameQueueLock.unlock();

            /*****************************************END Pre-existing Code END  ***********************************/

           

            microseconds average_slp = playout_buffer->getAverageFrameTime(); // rename the varialbe here to averageFrameTime

            if (playout_buffer->getQueueMonitor())
            {
                playout_buffer->adjustOffsetVal();
            }
            int sleepOffset = playout_buffer->getSleepOffVal();
            microseconds expectedSleepTime = (average_slp - run_time - sleepForDifference - microseconds(sleepOffset));
            logger->LogGraph(std::to_string(sleepOffset), "sleep_offset");

            if (expectedSleepTime > microseconds(0))
            {

                logger->LogGraph(std::to_string(average_slp.count()), "sleepValue");
                logger->LogGraph(std::to_string(sleepForDifference.count()), "oversleepValue");

                // sleep calculation and execution
                microseconds beginSleepTime = playout_buffer->getElapsedTime();
                sleep_for(expectedSleepTime); // need to account for sleep inaccuracies
                microseconds endSleepTime = playout_buffer->getElapsedTime();
                microseconds realSleepTime = (endSleepTime - beginSleepTime);

                logger->LogGraph(std::to_string((endSleepTime - beginSleepTime).count()), "actualSleepTime");
                logger->LogGraph(std::to_string((average_slp).count()), "average_slp");
                logger->LogGraph(std::to_string((run_time).count()), "run_time");

                sleepForDifference = endSleepTime - beginSleepTime - expectedSleepTime;
            }
            else
            {
                logger->LogGraph(std::to_string((average_slp).count()), "average_slp");
                logger->LogGraph(std::to_string((run_time).count()), "run_time");
                logger->Log("sleep not necessary", LogLevel::INFO);
                sleepForDifference = microseconds(0);
            }
        }
        sleep_for(microseconds(500)); // if not in dequeue state, sleep for .5 milliseconds
        // readjust / refill parameter
    }
}

void Pacer::renderFrame(AVFrame *frame)
{
    auto logger = Logger::GetInstance();
    auto playoutBuffer = playoutBuffer::GetInstance();
    //playoutBuffer->setQueueType(playoutBuffer::EPolicy); // EPolicy, IPolicy
    //playoutBuffer->setQueueMonitor(true, 8);
    if(frame->key_frame){
        logger->Log("Key, Delta Count: " + std::to_string(playoutBuffer->getDeltaCount()), LogLevel::INFO);
        playoutBuffer->resetDeltaCount();
    }
    else{
        playoutBuffer->incrementDeltaCount();
        logger->Log("Delta", LogLevel::INFO);
    }        // true, queueMonitor is on, false, monitor off, int is buffer target size
    // Initial buffer for I/E policy
    // Fudge Factor
    // Initial Value for moving average (60hz)
    switch (playoutBuffer->getQueueType())
    {
    case playoutBuffer::EPolicy:
        playoutBuffer->enqueueEPolicy(frame);
        break;
    case playoutBuffer::IPolicy:
        playoutBuffer->enqueueIPolicy(frame);
        break;
    default:
        playoutBuffer->enqueueEPolicy(frame);
        break;
    }

    logger->tempCounterFramesIn++;
    logger->LogGraph(std::to_string(logger->tempCounterFramesIn), "framesIn");
    logger->LogGraph(std::to_string(playoutBuffer->getQueueSize()), "QueueSize");
}

void Pacer::dropFrameForEnqueue(QQueue<AVFrame *> &queue)
{
    SDL_assert(queue.size() <= MAX_QUEUED_FRAMES);
    if (queue.size() == MAX_QUEUED_FRAMES)
    {
        AVFrame *frame = queue.dequeue();
        av_frame_free(&frame);
    }
}

void Pacer::submitFrame(AVFrame *frame)
{
    // Make sure initialize() has been called
    SDL_assert(m_MaxVideoFps != 0);

    // Queue the frame and possibly wake up the render thread
    m_FrameQueueLock.lock();
    if (m_VsyncSource != nullptr)
    {
        dropFrameForEnqueue(m_PacingQueue);
        m_PacingQueue.enqueue(frame);
        m_FrameQueueLock.unlock();
        m_PacingQueueNotEmpty.wakeOne();
    }
    else
    {
        enqueueFrameForRenderingAndUnlock(frame);
    }
}
