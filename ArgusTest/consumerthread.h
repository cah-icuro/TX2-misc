#ifndef CONSUMERTHREAD
#define CONSUMERTHREAD

#include <iostream>
#include <fstream>
#include <queue>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "Error.h"
#include "Thread.h"

#include <Argus/Argus.h>
#include <EGLStream/EGLStream.h>
#include <EGLStream/NV/ImageNativeBuffer.h>

#include "NvVideoConverter.h"
#include <NvApplicationProfiler.h>

#include "opencv_consumer_interface.h"

#define FRAME_CONVERTER_BUF_NUMBER (10)

/*******************************************************************************
 * FrameConsumer thread:
 *   Creates an EGLStream::FrameConsumer object to read frames from the stream
 *   and create NvBuffers (dmabufs) from acquired frames before providing the
 *   buffers to V4L2 for video conversion. The converter will feed the image to
 *   processing routine.
 ******************************************************************************/
class ConsumerThread : public Thread
{
public:
    explicit ConsumerThread(OutputStream* stream);
    ~ConsumerThread();

    bool isInError()
    {
        return m_gotError;
    }

private:
    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    bool createImageConverter();
    void abort();

    static bool converterCapturePlaneDqCallback(
            struct v4l2_buffer *v4l2_buf,
            NvBuffer *buffer,
            NvBuffer *shared_buffer,
            void *arg);
    static bool converterOutputPlaneDqCallback(
            struct v4l2_buffer *v4l2_buf,
            NvBuffer *buffer,
            NvBuffer *shared_buffer,
            void *arg);

    void writeFrameToOpencvConsumer(
        camera_caffe_context *p_ctx,
        NvBuffer *buffer);

    OutputStream* m_stream;
    UniqueObj<FrameConsumer> m_consumer;
    NvVideoConverter *m_ImageConverter;
    std::queue < NvBuffer * > *m_ConvOutputPlaneBufQueue;
    pthread_mutex_t m_queueLock;
    pthread_cond_t m_queueCond;
    int conv_buf_num;
    int m_numPendingFrames;

    camera_caffe_context *m_pContext;

    bool m_gotError;
};

ConsumerThread::ConsumerThread(OutputStream* stream) :
        m_stream(stream),
        m_ImageConverter(NULL),
        m_gotError(false)
{
    conv_buf_num = FRAME_CONVERTER_BUF_NUMBER;
    m_ConvOutputPlaneBufQueue = new std::queue < NvBuffer * >;
    pthread_mutex_init(&m_queueLock, NULL);
    pthread_cond_init(&m_queueCond, NULL);
    m_pContext = &ctx;
    m_numPendingFrames = 0;
}

ConsumerThread::~ConsumerThread()
{
    delete m_ConvOutputPlaneBufQueue;
    if (m_ImageConverter)
    {
        if (DO_STAT)
             m_ImageConverter->printProfilingStats(std::cout);
        delete m_ImageConverter;
    }
}

bool ConsumerThread::threadInitialize()
{
    // Create the FrameConsumer.
    m_consumer = UniqueObj<FrameConsumer>(FrameConsumer::create(m_stream));
    if (!m_consumer)
        ORIGINATE_ERROR("Failed to create FrameConsumer");

    // Create Video converter
    if (!createImageConverter())
        ORIGINATE_ERROR("Failed to create video m_ImageConverteroder");

    return true;
}

bool ConsumerThread::threadExecute()
{
    IStream *iStream = interface_cast<IStream>(m_stream);
    IFrameConsumer *iFrameConsumer = interface_cast<IFrameConsumer>(m_consumer);

    // Wait until the producer has connected to the stream.
    CONSUMER_PRINT("Waiting until producer is connected...\n");
    if (iStream->waitUntilConnected() != STATUS_OK)
        ORIGINATE_ERROR("Stream failed to connect.");
    CONSUMER_PRINT("Producer has connected; continuing.\n");

    // Keep acquire frames and queue into converter
    while (!m_gotError)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.m.planes = planes;

        pthread_mutex_lock(&m_queueLock);
        while (!m_gotError &&
            ((m_ConvOutputPlaneBufQueue->empty()) || (m_numPendingFrames >= MAX_PENDING_FRAMES)))
        {
            pthread_cond_wait(&m_queueCond, &m_queueLock);
        }

        if (m_gotError)
        {
            pthread_mutex_unlock(&m_queueLock);
            break;
        }

        NvBuffer *buffer = NULL;
        int fd = -1;

        buffer = m_ConvOutputPlaneBufQueue->front();
        m_ConvOutputPlaneBufQueue->pop();
        pthread_mutex_unlock(&m_queueLock);

        // Acquire a frame.
        UniqueObj<Frame> frame(iFrameConsumer->acquireFrame());
        IFrame *iFrame = interface_cast<IFrame>(frame);
        if (!iFrame)
            break;

        // Get the IImageNativeBuffer extension interface and create the fd.
        NV::IImageNativeBuffer *iNativeBuffer =
            interface_cast<NV::IImageNativeBuffer>(iFrame->getImage());
        if (!iNativeBuffer)
            ORIGINATE_ERROR("IImageNativeBuffer not supported by Image.");
        fd = iNativeBuffer->createNvBuffer(Size2D<uint32_t>(ctx.width, ctx.height),
                                           NvBufferColorFormat_YUV420,
                                           NvBufferLayout_BlockLinear);

        // Push the frame into V4L2.
        v4l2_buf.index = buffer->index;
        // Set the bytesused to some non-zero value so that
        // v4l2 convert processes the buffer
        buffer->planes[0].bytesused = 1;
        buffer->planes[0].fd = fd;
        buffer->planes[1].fd = fd;
        buffer->planes[2].fd = fd;
        pthread_mutex_lock(&m_queueLock);
        m_numPendingFrames++;
        pthread_mutex_unlock(&m_queueLock);

        CONSUMER_PRINT("acquireFd %d (%d frames)\n", fd, m_numPendingFrames);

        int ret = m_ImageConverter->output_plane.qBuffer(v4l2_buf, buffer);
        if (ret < 0) {
            abort();
            ORIGINATE_ERROR("Fail to qbuffer for conv output plane");
        }
    }

    // Wait till capture plane DQ Thread finishes
    // i.e. all the capture plane buffers are dequeued
    m_ImageConverter->capture_plane.waitForDQThread(2000);

    CONSUMER_PRINT("Done.\n");

    requestShutdown();

    return true;
}

bool ConsumerThread::threadShutdown()
{
    return true;
}

void ConsumerThread::writeFrameToOpencvConsumer(
    camera_caffe_context *p_ctx, NvBuffer *buffer)
{
    NvBuffer::NvBufferPlane *plane = &buffer->planes[0];
    uint8_t *pdata = (uint8_t *) plane->data;

    // output RGB frame to opencv, only 1 plane
    if (p_ctx->lib_handler)
    {
        p_ctx->opencv_img_processing(p_ctx->opencv_handler
            , pdata, plane->fmt.width, plane->fmt.height);
    }
}

bool ConsumerThread::converterCapturePlaneDqCallback(
    struct v4l2_buffer *v4l2_buf,
    NvBuffer * buffer,
    NvBuffer * shared_buffer,
    void *arg)
{
    ConsumerThread *thiz = (ConsumerThread*)arg;
    camera_caffe_context *p_ctx = thiz->m_pContext;
    int e;

    if (!v4l2_buf)
    {
        REPORT_ERROR("Failed to dequeue buffer from conv capture plane");
        thiz->abort();
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        return false;
    }

    thiz->writeFrameToOpencvConsumer(p_ctx, buffer);

    e = thiz->m_ImageConverter->capture_plane.qBuffer(*v4l2_buf, NULL);
    if (e < 0)
        ORIGINATE_ERROR("qBuffer failed");

    return true;
}

bool ConsumerThread::converterOutputPlaneDqCallback(
    struct v4l2_buffer *v4l2_buf,
    NvBuffer * buffer,
    NvBuffer * shared_buffer,
    void *arg)
{
    ConsumerThread *thiz = (ConsumerThread*)arg;

    if (!v4l2_buf)
    {
        REPORT_ERROR("Failed to dequeue buffer from conv capture plane");
        thiz->abort();
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        return false;
    }
    NvBufferDestroy(shared_buffer->planes[0].fd);

    CONSUMER_PRINT("releaseFd %d (%d frames)\n", shared_buffer->planes[0].fd, thiz->m_numPendingFrames);
    pthread_mutex_lock(&thiz->m_queueLock);
    thiz->m_numPendingFrames--;
    thiz->m_ConvOutputPlaneBufQueue->push(buffer);
    pthread_cond_broadcast(&thiz->m_queueCond);
    pthread_mutex_unlock(&thiz->m_queueLock);

    return true;
}

bool ConsumerThread::createImageConverter()
{
    int ret = 0;

    // YUV420 --> RGB32 converter
    m_ImageConverter = NvVideoConverter::createVideoConverter("conv");
    if (!m_ImageConverter)
        ORIGINATE_ERROR("Could not create m_ImageConverteroder");

    if (DO_STAT)
        m_ImageConverter->enableProfiling();

    m_ImageConverter->capture_plane.
        setDQThreadCallback(converterCapturePlaneDqCallback);
    m_ImageConverter->output_plane.
        setDQThreadCallback(converterOutputPlaneDqCallback);

    ret = m_ImageConverter->setOutputPlaneFormat(V4L2_PIX_FMT_YUV420M, m_pContext->width,
                                    m_pContext->height, V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set output plane format");

    ret = m_ImageConverter->setCapturePlaneFormat(V4L2_PIX_FMT_ABGR32, m_pContext->width,
                                    m_pContext->height, V4L2_NV_BUFFER_LAYOUT_PITCH);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set capture plane format");

    // Query, Export and Map the output plane buffers so that we can read
    // raw data into the buffers
    ret = m_ImageConverter->output_plane.setupPlane(V4L2_MEMORY_DMABUF, conv_buf_num, false, false);
    if (ret < 0)
        ORIGINATE_ERROR("Could not setup output plane");

    // Query, Export and Map the output plane buffers so that we can write
    // m_ImageConverteroded data from the buffers
    ret = m_ImageConverter->capture_plane.setupPlane(V4L2_MEMORY_MMAP, conv_buf_num, true, false);
    if (ret < 0)
        ORIGINATE_ERROR("Could not setup capture plane");

    // Add all empty conv output plane buffers to m_ConvOutputPlaneBufQueue
    for (uint32_t i = 0; i < m_ImageConverter->output_plane.getNumBuffers(); i++)
    {
        m_ConvOutputPlaneBufQueue->push(
            m_ImageConverter->output_plane.getNthBuffer(i));
    }

    // conv output plane STREAMON
    ret = m_ImageConverter->output_plane.setStreamStatus(true);
    if (ret < 0)
        ORIGINATE_ERROR("fail to set conv output stream on");

    // conv capture plane STREAMON
    ret = m_ImageConverter->capture_plane.setStreamStatus(true);
    if (ret < 0)
        ORIGINATE_ERROR("fail to set conv capture stream on");

    // Start threads to dequeue buffers on conv capture plane,
    // conv output plane and capture plane
    m_ImageConverter->capture_plane.startDQThread(this);
    m_ImageConverter->output_plane.startDQThread(this);

    // Enqueue all empty conv capture plane buffers
    for (uint32_t i = 0; i < m_ImageConverter->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        ret = m_ImageConverter->capture_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0) {
            abort();
            ORIGINATE_ERROR("Error queueing buffer at conv capture plane");
        }
    }

    printf("create vidoe converter return true\n");
    return true;
}

void ConsumerThread::abort()
{
    m_ImageConverter->abort();
    m_gotError = true;
}

#endif // CONSUMERTHREAD

