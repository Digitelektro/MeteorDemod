#include "opencl.h"

#include <fstream>

#ifdef OPENCL_FOUND

namespace OpenCL {

std::vector<Context::Device> Context::getDevices(cl_device_type deviceType) {
    std::vector<Device> devices;
    cl_platform_id platformsIDs[cMaximumDeviceCount];
    cl_uint platformsCount = 0;
    cl_device_id deviceIDs[cMaximumDeviceCount];
    cl_uint deviceCount = 0;
    char deviceName[cMaximumDeviceNameLength];
    size_t deviceNameLen = 0;

    cl_int error = clGetPlatformIDs(cMaximumDeviceCount, platformsIDs, &platformsCount);
    if(error == CL_SUCCESS) {
        for(cl_uint p = 0; p < platformsCount; p++) {
            if(clGetDeviceIDs(platformsIDs[p], deviceType, cMaximumDeviceCount, deviceIDs, &deviceCount) == CL_SUCCESS) {
                for(cl_uint d = 0; d < deviceCount; d++) {
                    size_t workGroupSize = 0;
                    size_t maxComputeUnits = 0;
                    cl_device_type deviceType;

                    error = clGetDeviceInfo(deviceIDs[d], CL_DEVICE_TYPE, sizeof(cl_device_type), &deviceType, NULL);
                    if(error != CL_SUCCESS) {
                        continue;
                    }
                    error = clGetDeviceInfo(deviceIDs[d], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &workGroupSize, NULL);
                    if(error != CL_SUCCESS) {
                        continue;
                    }
                    error = clGetDeviceInfo(deviceIDs[d], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(size_t), &maxComputeUnits, NULL);
                    if(error != CL_SUCCESS) {
                        continue;
                    }
                    error = clGetDeviceInfo(deviceIDs[d], CL_DEVICE_NAME, cMaximumDeviceNameLength, deviceName, &deviceNameLen);
                    if(error != CL_SUCCESS) {
                        continue;
                    }

                    devices.emplace_back(Device{platformsIDs[p], deviceIDs[d], deviceType, std::string(&deviceName[0], &deviceName[deviceNameLen]), workGroupSize, maxComputeUnits});
                }
            }
        }
    }

    return devices;
}

Context::~Context() {
    if(mContext) {
        clReleaseContext(mContext);
    }
}

void Context::init() {
    cl_int error = 0;

    mContext = clCreateContext(NULL, 1, &mDevice.deviceID, NULL, NULL, &error);
    if(error != CL_SUCCESS) {
        throw std::runtime_error("Could not init OpenCL context!");
    }
}

cl_program Context::buildKernel(const std::string& path) {
    std::ifstream ifStream(path);
    std::string kernelSrc(std::istreambuf_iterator<char>{ifStream}, {});

    const char* srcs[1] = {kernelSrc.c_str()};
    const size_t lens[1] = {kernelSrc.length()};
    cl_int err = 0;
    char errorMessage[10000];
    size_t errorMessgeLen = 0;

    cl_program program = clCreateProgramWithSource(mContext, 1, srcs, lens, &err);
    err = clBuildProgram(program, 1, &mDevice.deviceID, NULL, NULL, NULL);

    if(err != CL_SUCCESS) {
        if(clGetProgramBuildInfo(program, mDevice.deviceID, CL_PROGRAM_BUILD_LOG, sizeof(errorMessage), errorMessage, &errorMessgeLen) == CL_SUCCESS) {
            throw std::runtime_error("OpenCV Build program error: " + std::string(&errorMessage[0], &errorMessage[errorMessgeLen]));
        } else {
            throw std::runtime_error("OpenCV Build program error!");
        }
    }

    return program;
}

cl_program Context::buildKernel(const char* kernelSrc, size_t srcLen) {
    cl_int err = 0;
    char errorMessage[10000];
    size_t errorMessgeLen = 0;
    const char* srcs[1] = {kernelSrc};
    const size_t lens[1] = {srcLen};
    cl_program program = clCreateProgramWithSource(mContext, 1, srcs, lens, &err);
    err = clBuildProgram(program, 1, &mDevice.deviceID, NULL, NULL, NULL);

    if(err != CL_SUCCESS) {
        if(clGetProgramBuildInfo(program, mDevice.deviceID, CL_PROGRAM_BUILD_LOG, sizeof(errorMessage), errorMessage, &errorMessgeLen) == CL_SUCCESS) {
            throw std::runtime_error("OpenCV Build program error: " + std::string(&errorMessage[0], &errorMessage[errorMessgeLen]));
        } else {
            throw std::runtime_error("OpenCV Build program error!");
        }
    }

    return program;
}

} // namespace OpenCL

#endif // OPENCL_FOUND
