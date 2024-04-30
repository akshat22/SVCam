#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <iostream>
#include <fstream>
#include "C:\Program Files\SVS-VISTEK GmbH\SVCam Kit\SDK\include\sv_gen_sdk.h"

// Global variables for device and stream handles
 SV_DEVICE_HANDLE hDevice = NULL;
 SV_REMOTE_DEVICE_HANDLE hRemoteDevice = NULL;
 SV_STREAM_HANDLE hStream = NULL;

// Function to initialize the SDK
bool InitializeSDK() {
    std::string ctiPath;
    std::string genicamPath;
    std::string genicamCachePath;
    std::string clProtocolPath;

    char buffer[1024] = { 0 };
    int res = GetEnvironmentVariableA("GENICAM_GENTL64_PATH", buffer, sizeof(buffer));
    if (0 == res)
        return false;

    ctiPath = std::string(buffer);

    memset(buffer, 0, sizeof(buffer));
    res = GetEnvironmentVariableA("SVS_GENICAM_ROOT", buffer, sizeof(buffer));
    if (0 == res)
        return false;

    genicamPath = std::string(buffer);

    memset(buffer, 0, sizeof(buffer));
    res = GetEnvironmentVariableA("SVS_GENICAM_CACHE", buffer, sizeof(buffer));
    if (0 == res)
        return false;

    genicamCachePath = std::string(buffer);

    memset(buffer, 0, sizeof(buffer));
    res = GetEnvironmentVariableA("SVS_GENICAM_CLPROTOCOL", buffer, sizeof(buffer));
    if (0 == res)
        return false;

    clProtocolPath = std::string(buffer);

     try
     {
         SV_RETURN ret = SVLibInit(ctiPath.c_str(), genicamPath.c_str(), genicamCachePath.c_str(), clProtocolPath.c_str());
         if (SV_ERROR_SUCCESS != ret) { 
             printf("SVLibInit Failed! :%d", ret);
             return false;
         }
     }
     catch(const std::exception& e)
     {
         std::cerr << e.what() << '\n';
     }

    return true;
}

bool SaveImageToFile(const char* filename, uint8_t* imageData, size_t imageSize) {

    // Open the file for writing in binary mode
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file : " << filename << std::endl;
        return false;
    }

    // Write image data to the file (Need to interpret the raw image data)
    file.write(reinterpret_cast<char*>(imageData), imageSize);

    // Check for any write errors
    if (file.bad()) {
        std::cerr << "Error occurred while writing to file: " << filename << std::endl;
        return false;
    }

    // Close the file
    file.close();

    std::cout << "Image saved to file: " << filename << std::endl;
    return true;
}

bool CaptureAndSaveImages() {

    SV_RETURN ret;

    // Open streaming channel
    char streamId[256];
    size_t streamIdSize = sizeof(streamId);
    ret = SVDeviceGetStreamId(hDevice, 0, streamId, &streamIdSize);
    ret = SVDeviceStreamOpen(hDevice, streamId, &hStream);
    std::cout << ret << std::endl;
    if (ret != SV_ERROR_SUCCESS) {
        std::cerr << "Failed to open streaming channel\n";
        return false;
    }

    // Start acquisition
    SV_FEATURE_HANDLE hFeature;
    ret = SVFeatureGetByName(hRemoteDevice, "TLParamsLocked", &hFeature);
    if (ret != SV_ERROR_SUCCESS) {
        std::cerr << "Failed to get TLParamsLocked feature\n";
        return false;
    }
    ret = SVFeatureSetValueInt64(hRemoteDevice, hFeature, 1);
    if (ret != SV_ERROR_SUCCESS) {
        std::cerr << "Failed to set TLParamsLocked feature\n";
        return false;
    }

    // Retrieve payload size to allocate the buffers
    int64_t payloadSize;
    ret = SVFeatureGetByName(hRemoteDevice, "PayloadSize", &hFeature);
    if (ret != SV_ERROR_SUCCESS) {
        std::cerr << "Failed to get PayloadSize feature\n";
        return false;
    }
    ret = SVFeatureGetValueInt64(hRemoteDevice, hFeature, &payloadSize);
    if (ret != SV_ERROR_SUCCESS) {
        std::cerr << "Failed to get payload size\n";
        return false;
    }

    // Allocate buffers with the retrieved payload size
    const uint32_t bufcount = 3; // Number of buffers to allocate
    for (uint32_t i = 0; i < bufcount; ++i) {
        size_t* buffer = new size_t[payloadSize];
        memset(buffer, 0, sizeof(size_t) * payloadSize);

        SV_BUFFER_HANDLE hBuffer = NULL;

        // Allocate memory to thr data stream
        ret = SVStreamAnnounceBuffer(hStream, buffer, static_cast<uint32_t>(payloadSize), nullptr, &hBuffer);
        if (ret != SV_ERROR_SUCCESS) {
            std::cerr << "Failed to announce buffer\n";
            delete[] buffer;
            continue;
        }

        // Queue the buffer for data acquisition
        ret = SVStreamQueueBuffer(hStream, hBuffer);
        if (ret != SV_ERROR_SUCCESS) {
            std::cerr << "Failed to queue buffer\n";
            delete[] buffer;
            continue;
        }
    }

    // Start acquisition on the host
    SVStreamFlushQueue(hStream, SV_ACQ_QUEUE_ALL_TO_INPUT);
    ret = SVStreamAcquisitionStart(hStream, SV_ACQ_START_FLAGS_DEFAULT, INFINITE);
    if (ret != SV_ERROR_SUCCESS) {
        std::cerr << "Failed to start acquisition\n";
        return false;
    }

    // Start acquisiton on the remote device (the SVS-Vistek Camera)
    SVFeatureGetByName(hRemoteDevice, "AcquisitionStart", &hFeature);
    ret = SVFeatureCommandExecute(hRemoteDevice, hFeature, 1000);
    if (ret != SV_ERROR_SUCCESS) {
        std::cerr << "Failed to start acquisition on the remote_device\n";
        return false;
    }

    // Acquring new images
    for (uint32_t i = 0; i < 1; ++i) {
        size_t* buffer = new size_t[payloadSize];
        memset(buffer, 0, sizeof(size_t) * payloadSize);

        SV_BUFFER_HANDLE hBuffer = NULL;
        ret = SVStreamAnnounceBuffer(hStream, buffer, static_cast<uint32_t>(payloadSize), nullptr, &hBuffer);
        if (ret != SV_ERROR_SUCCESS) {
            std::cerr << "Failed to announce buffer\n";
            delete[] buffer;
            continue;
        }
        ret = SVStreamWaitForNewBuffer(hStream, NULL, &hBuffer, 1000);
        if (ret != SV_ERROR_SUCCESS) {
            std::cerr << "Failed to wait for filled buffer\n";
            continue;
        }

         SV_BUFFER_INFO *bufferInfo = new SV_BUFFER_INFO();
         ret = SVStreamBufferGetInfo(hStream, hBuffer, bufferInfo);
         if (ret != SV_ERROR_SUCCESS) {
             std::cerr << "Failed to get buffer info\n";
             delete bufferInfo;
             continue;
         }

        // Re-queue the buffer for further use
        ret = SVStreamQueueBuffer(hStream, hBuffer);
        if (ret != SV_ERROR_SUCCESS) {
            std::cerr << "Failed to queue buffer\n";
            continue;
        }

        SaveImageToFile("Hello.jpeg", bufferInfo->pImagePtr, bufferInfo->iImageSize);

    }

    // Stop the acquisition
    SVFeatureGetByName(streamId, "AcquisitionStop", &hFeature);
    SVFeatureCommandExecute(streamId, hFeature, 1000);

    SVStreamAcquisitionStop(streamId, SV_ACQ_STOP_FLAGS_DEFAULT);
    SVStreamFlushQueue(streamId, SV_ACQ_QUEUE_INPUT_TO_OUTPUT);
    SVStreamFlushQueue(streamId, SV_ACQ_QUEUE_OUTPUT_DISCARD);

    hFeature = NULL;
    SVFeatureGetByName(streamId, "TLParamsLocked", &hFeature);
    SVFeatureSetValueInt64(streamId, hFeature, 0);

    return true;
}

bool DiscoverAndEnumerateDevices() {
    
    SV_RETURN ret;
 
    uint32_t systemCount;
    SVLibSystemGetCount(&systemCount);
    //std::cout << systemCount;

    for (uint32_t i = 0; i < systemCount; ++i) {
        SV_SYSTEM_HANDLE hSystem;
        ret = SVLibSystemOpen(i, &hSystem);
        if (ret != SV_ERROR_SUCCESS) {
            std::cerr << "Failed to open system\n";
            return false;
        }

        ret = SVSystemUpdateInterfaceList(hSystem, NULL, 1000);
        if (ret != SV_ERROR_SUCCESS) {
            std::cerr << "Failed to update interface list\n";
            SVSystemClose(hSystem);
            return false;
        }

        // Get all available interfaces
        uint32_t interfaceCount;
        SVSystemGetNumInterfaces(hSystem, &interfaceCount);
        //std::cout << interfaceCount << std::endl;

        for (uint32_t j = 0; j < interfaceCount; ++j) {

            SV_INTERFACE_HANDLE hInterface;

            // Get the interface ID
            char interfaceId[256];
            size_t psize = sizeof(interfaceId);
            ret = SVSystemGetInterfaceId(hSystem, j, interfaceId, &psize);
            //std::cout << "Interface Id: " << interfaceId << std::endl;

            ret = SVSystemInterfaceOpen(hSystem, interfaceId, &hInterface);
            if (ret != SV_ERROR_SUCCESS) {
                std::cerr << "Failed to open interface\n";
                continue;
            }

            ret = SVInterfaceUpdateDeviceList(hInterface, NULL, 1000);
            if (ret != SV_ERROR_SUCCESS) {
                std::cerr << "Failed to update device list\n";
                SVInterfaceClose(hInterface);
                continue;
            }

            // Get the number of devices on the interface
            uint32_t deviceCount;
            SVInterfaceGetNumDevices(hInterface, &deviceCount);
            //std::cout << "Device count: " << deviceCount << std::endl;

            for (uint32_t k = 0; k < deviceCount; ++k) {

                // Get the device ID
                char deviceId[256];
                size_t deviceIdSize = sizeof(deviceId);
                ret = SVInterfaceGetDeviceId(hInterface, k, deviceId, &deviceIdSize);
                if (ret != SV_ERROR_SUCCESS) {
                    std::cerr << "Failed to get device ID\n";
                    continue;
                }
                std::cout << "Device Id: " << deviceId << std::endl;

                // Open the remote device
                ret = SVInterfaceDeviceOpen(hInterface, deviceId, SV_DEVICE_ACCESS_CONTROL, &hDevice, &hRemoteDevice);
                if (ret != SV_ERROR_SUCCESS) {
                    std::cerr << "Failed to open remote device\n";
                    continue;
                }

                CaptureAndSaveImages();

                // Close the remote device
                ret = SVDeviceClose(hDevice);
                if (ret != SV_ERROR_SUCCESS) {
                    std::cerr << "Failed to close remote device\n";
                    continue;
                }
            }

            // Close the interface
            ret = SVInterfaceClose(hInterface);
            if (ret != SV_ERROR_SUCCESS) {
                std::cerr << "Failed to close interface\n";
                continue;
            }
        }

        // Close the system
        ret = SVSystemClose(hSystem);
        if (ret != SV_ERROR_SUCCESS) {
            std::cerr << "Failed to close system\n";
            return false;
        }
    }
    return true;
}

int main() {
    if (!InitializeSDK()) {
        std::cerr << "Failed to initialize SDK\n";
        return 1;
    }
    if (!DiscoverAndEnumerateDevices()) {
        std::cerr << "Failed to discover devices\n";
        return 1;
    }

    SVLibClose();
    return 0;
}