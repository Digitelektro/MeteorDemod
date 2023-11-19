#pragma once

#ifdef OPENCL_FOUND

#define CL_TARGET_OPENCL_VERSION 300

#include <CL/cl.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace OpenCL {

class Context {
    friend class Program;

  public:
    struct Device {
        cl_platform_id platformID;
        cl_device_id deviceID;
        cl_device_type type;
        std::string name;
        size_t workGroupSize;
        size_t maxComputeUnits;
    };

  public:
    static std::vector<Device> getDevices(cl_device_type deviceType = CL_DEVICE_TYPE_ALL);

  public:
  public:
    Context() = delete;
    Context(const Device& device)
        : mDevice(device) {}

    ~Context();

    void init();
    cl_program buildKernel(const std::string& path);
    cl_program buildKernel(const char* kernelSrc, size_t srcLen);

    size_t getWorkGroupSize() const {
        return mDevice.workGroupSize;
    }
    size_t getMaxComputeUnits() const {
        return mDevice.workGroupSize;
    }

  private:
    Device mDevice;
    cl_context mContext = nullptr;


  private:
    static constexpr int cMaximumDeviceCount = 100;
    static constexpr int cMaximumDeviceNameLength = 300;
};

class Program {
  public:
    explicit Program(std::shared_ptr<Context> context, const std::string& path, const std::string& name)
        : mContext(context) {
        cl_int err = 0;
        mProgram = context->buildKernel(path);

        mQueue = clCreateCommandQueueWithProperties(mContext->mContext, mContext->mDevice.deviceID, 0, &err);
        if(err != CL_SUCCESS) {
            throw std::runtime_error("Create queue failed!");
        }
        mKernel = clCreateKernel(mProgram, name.c_str(), &err);
        if(err != CL_SUCCESS) {
            throw std::runtime_error("Create kernel failed!");
        }
    }

    explicit Program(std::shared_ptr<Context> context, const char* kernelSrc, size_t srcLen, const std::string& name)
        : mContext(context) {
        cl_int err = 0;
        mProgram = context->buildKernel(kernelSrc, srcLen);

        mQueue = clCreateCommandQueueWithProperties(mContext->mContext, mContext->mDevice.deviceID, 0, &err);
        if(err != CL_SUCCESS) {
            throw std::runtime_error("Create queue failed!");
        }
        mKernel = clCreateKernel(mProgram, name.c_str(), &err);
        if(err != CL_SUCCESS) {
            throw std::runtime_error("Create kernel failed!");
        }
    }

    ~Program() {
        clFlush(mQueue);
        clFinish(mQueue);
        clReleaseKernel(mKernel);
        clReleaseProgram(mProgram);
        for(auto& [index, memory] : mBuffers) {
            clReleaseMemObject(memory);
        }
        clReleaseCommandQueue(mQueue);
    }

    std::shared_ptr<Context> getContext() {
        return mContext;
    }

    template <typename T>
    inline void createBuffer(cl_mem_flags flags, size_t length, int index) {
        cl_int err = 0;
        cl_mem buffer = clCreateBuffer(mContext->mContext, flags, sizeof(T) * length, NULL, &err);
        if(err != CL_SUCCESS) {
            throw std::runtime_error("Create OpenCL buffer failed!");
        }
        mBuffers[index] = buffer;

        err = clSetKernelArg(mKernel, index, sizeof(cl_mem), &buffer);
        if(err != CL_SUCCESS) {
            throw std::runtime_error("Set kernel arg failed!");
        }
    }

    template <typename T>
    inline void enqueueWriteBuffer(const T* params, size_t length, size_t paramIndex) {
        cl_int err = 0;
        err = clEnqueueWriteBuffer(mQueue, mBuffers[paramIndex], CL_TRUE, 0, sizeof(T) * length, params, 0, NULL, NULL);
        if(err != CL_SUCCESS) {
            throw std::runtime_error("Write OpenCL buffer failed!");
        }
    }

    template <typename T>
    inline void enqueueReadBuffer(T* params, size_t length, size_t paramIndex) {
        cl_int err = 0;
        err = clEnqueueReadBuffer(mQueue, mBuffers[paramIndex], true, 0, sizeof(T) * length, params, 0, NULL, NULL);
        if(err != CL_SUCCESS) {
            throw std::runtime_error("Write OpenCL buffer failed!");
        }
    }

    template <typename T>
    inline void setKernelArg(T arg, cl_uint index) {
        cl_int err = 0;
        clSetKernelArg(mKernel, index, sizeof(T), &arg);
        if(err != CL_SUCCESS) {
            throw std::runtime_error("Set kernel arg failed!");
        }
    }

    void execute(size_t itemSize) {
        size_t globalItemSize = itemSize;
        cl_int err = clEnqueueNDRangeKernel(mQueue, mKernel, 1, NULL, &globalItemSize, NULL, 0, NULL, NULL);
        if(err != CL_SUCCESS) {
            throw std::runtime_error("Couldn't execute kernel!");
        }
    }


  private:
    std::shared_ptr<Context> mContext;
    cl_program mProgram = nullptr;
    cl_command_queue mQueue = nullptr;
    cl_kernel mKernel = nullptr;
    std::map<int, cl_mem> mBuffers;
};


} // namespace OpenCL


#endif // OPENCL_FOUND
