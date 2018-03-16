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

using namespace Argus;

const int WIDTH = 640;
const int HEIGHT = 480;

#define FIVE_SEC_NANO (5000000000)

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)
#define CONSUMER_PRINT(...) printf("CONSUMER: " __VA_ARGS__)
#define CHECK_ERROR(expr) \
    do { \
        if ((expr) < 0) { \
            abort(); \
            ORIGINATE_ERROR(#expr " failed"); \
        } \
    } while (0);

#define EXIT_IF_NULL(val,msg)   \
        {if (!val) {printf("%s\n",msg); return EXIT_FAILURE;}}
#define EXIT_IF_NOT_OK(val,msg) \
        {if (val!=Argus::STATUS_OK) {printf("%s\n",msg); return EXIT_FAILURE;}}


//////

int main() {

    // Create the CameraProvider object and get the core interface.
    UniqueObj<CameraProvider> cameraProvider = UniqueObj<CameraProvider>(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider) {
        ORIGINATE_ERROR("Failed to create CameraProvider");
    }

    // Get the camera devices.
    std::vector<CameraDevice*> cameraDevices;
    Argus::Status status = iCameraProvider->getCameraDevices(&cameraDevices);
    if (cameraDevices.size() == 0) {
        ORIGINATE_ERROR("No cameras available");
    }

    // Create the capture session using the first device and get the core interface.
    UniqueObj<CaptureSession> captureSession(
            iCameraProvider->createCaptureSession(cameraDevices[0], &status));

    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(captureSession);
    if (!iCaptureSession) {
        ORIGINATE_ERROR("Failed to get ICaptureSession interface");
    }

    // Get settings then create the OutputStream
    printf("Creating output stream\n");
    UniqueObj<OutputStreamSettings> streamSettings(iCaptureSession->createOutputStreamSettings());
    IOutputStreamSettings *iStreamSettings = interface_cast<IOutputStreamSettings>(streamSettings);
    if (iStreamSettings) {
            iStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
            iStreamSettings->setResolution(Size2D<uint32_t>(WIDTH, HEIGHT));
    }
    else {
        ORIGINATE_ERROR("NULL for output stream settings!");
    }
    UniqueObj<OutputStream> stream(iCaptureSession->createOutputStream(streamSettings.get()));

    IStream *iStream = interface_cast<IStream>(stream);
    EXIT_IF_NULL(iStream, "Cannot get OutputStream Interface");

    UniqueObj<EGLStream::FrameConsumer> consumer(EGLStream::FrameConsumer::create(stream.get()));

    EGLStream::IFrameConsumer *iFrameConsumer = interface_cast<EGLStream::IFrameConsumer>(consumer);
    EXIT_IF_NULL(iFrameConsumer, "Failed to initialize Consumer");

    UniqueObj<Request> request(iCaptureSession->createRequest(CAPTURE_INTENT_STILL_CAPTURE));

    IRequest *iRequest = interface_cast<IRequest>(request);
    EXIT_IF_NULL(iRequest, "Failed to get capture request interface");

    status = iRequest->enableOutputStream(stream.get());
    EXIT_IF_NOT_OK(status, "Failed to enable stream in capture request");

    uint32_t requestId = iCaptureSession->capture(request.get());
    EXIT_IF_NULL(requestId, "Failed to submit capture request");

    /**
     * Acquire a frame from the capture request, create a JPG file
     */

    // Frame
    UniqueObj<EGLStream::Frame> frame(iFrameConsumer->acquireFrame(FIVE_SEC_NANO, &status));

    EGLStream::IFrame *iFrame = interface_cast<EGLStream::IFrame>(frame);
    EXIT_IF_NULL(iFrame, "Failed to get IFrame interface");

    // Image
    EGLStream::Image *image = iFrame->getImage();
    EXIT_IF_NULL(image, "Failed to get Image from iFrame->getImage()");

    EGLStream::IImageJPEG *iImageJPEG = interface_cast<EGLStream::IImageJPEG>(image);
    EXIT_IF_NULL(iImageJPEG, "Failed to get ImageJPEG Interface");

    // Write JPEG
    status = iImageJPEG->writeJPEG("oneShot.jpg");
    EXIT_IF_NOT_OK(status, "Failed to write JPEG");

    printf("\nDone -- exiting.\n\n");



    return 0;
}

