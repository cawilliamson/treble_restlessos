#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <linux/ioctl.h>

typedef int32_t (*checkMaxEnumPrototype)(int32_t ged_error_num);
typedef int32_t (*createWrapPrototype)(uint64_t surfaceHandle, uint64_t BBQ_ID);
typedef void (*destroyWrapPrototype)(uint64_t surfaceHandle);
typedef int32_t (*dequeueBufferTagWrapPrototype)(uint64_t surfaceHandle, int32_t fence, intptr_t buffer_addr);
typedef int32_t (*queueBufferTagWrapPrototype)(uint64_t surfaceHandle, int32_t fence, int32_t QedBuffer_length, intptr_t buffer_addr);
typedef int32_t (*acquireBufferTagWrapPrototype)(uint64_t surfaceHandle, intptr_t buffer_addr);
typedef void (*bufferConnectWrapPrototype)(uint64_t surfaceHandle, int32_t BBQ_api_type, int32_t pid);
typedef void (*bufferDisconnectWrapPrototype)(uint64_t surfaceHandle);

#if 0
        mGedKpiCheckMaxEnum = reinterpret_cast<checkMaxEnumPrototype>(dlsym(mGedKpiSoHandle, "ged_kpi_check_max_enum"));
        if (NULL == mGedKpiCheckMaxEnum) {
            ALOGE("load checkMaxEnumPrototype() failed [%s]", dlerror());
            return;
        } else {
            if (!mGedKpiCheckMaxEnum(GED_ERROR_NUM)) {
                ALOGE("check GED_ERROR_NUM fail");
                return;
            }
        }

        mGedKpiCreate = reinterpret_cast<createWrapPrototype>(dlsym(mGedKpiSoHandle, "ged_kpi_create_wrap"));
        if (NULL == mGedKpiCreate) {
            ALOGE("finding createWrapPrototype() failed [%s]", dlerror());
        }

        mGedKpiDestroy = reinterpret_cast<destroyWrapPrototype>(dlsym(mGedKpiSoHandle, "ged_kpi_destroy_wrap"));
        if (NULL == mGedKpiDestroy) {
            ALOGE("finding destroyWrapPrototype() failed [%s]", dlerror());
        }

        mGedKpiDequeueBuffer = reinterpret_cast<dequeueBufferTagWrapPrototype>(dlsym(mGedKpiSoHandle, "ged_kpi_dequeue_buffer_tag_wrap"));
        if (NULL == mGedKpiDequeueBuffer) {
            ALOGE("finding dequeueBufferTagWrapPrototype() failed [%s]", dlerror());
        }

        mGedKpiQueueBuffer = reinterpret_cast<queueBufferTagWrapPrototype>(dlsym(mGedKpiSoHandle, "ged_kpi_queue_buffer_tag_wrap"));
        if (NULL == mGedKpiQueueBuffer) {
            ALOGE("finding queueBufferTagWrapPrototype() failed [%s]", dlerror());
        }

        mGedKpiAcquireBuffer = reinterpret_cast<acquireBufferTagWrapPrototype>(dlsym(mGedKpiSoHandle, "ged_kpi_acquire_buffer_tag_wrap"));
        if (NULL == mGedKpiAcquireBuffer) {
            ALOGE("finding acquireBufferTagWrapPrototype() failed [%s]", dlerror());
        }

        mGedKpiBufferConnect = reinterpret_cast<bufferConnectWrapPrototype>(dlsym(mGedKpiSoHandle, "ged_kpi_buffer_connect"));
        if (NULL == mGedKpiBufferConnect) {
            ALOGE("finding bufferConnectWrapPrototype() failed [%s]", dlerror());
        }

        mGedKpiBufferDisconnect = reinterpret_cast<bufferDisconnectWrapPrototype>(dlsym(mGedKpiSoHandle, "ged_kpi_buffer_disconnect"));
        if (NULL == mGedKpiBufferDisconnect) {
            ALOGE("finding bufferDisconnectWrapPrototype() failed [%s]", dlerror());
        }
#endif

#define GED_MAGIC 'g'
#define GED_BRIDGE_COMMAND_GPU_TIMESTAMP      103
#define GED_IOWR(INDEX)  _IOWR(GED_MAGIC, INDEX, GED_BRIDGE_PACKAGE)
#define GED_BRIDGE_IO_GPU_TIMESTAMP \
    GED_IOWR(GED_BRIDGE_COMMAND_GPU_TIMESTAMP)
typedef struct _GED_BRIDGE_PACKAGE {
    unsigned int ui32FunctionID;
    int i32Size;
    void *pvParamIn;
    int i32InBufferSize;
    void *pvParamOut;
    int i32OutBufferSize;
} GED_BRIDGE_PACKAGE;

struct GED_BRIDGE_IN_GPU_TIMESTAMP {
    int pid;
    uint64_t ullWnd;
    int32_t i32FrameID;
    int fence_fd;
    int QedBuffer_length;
    int isSF;
};

struct GED_BRIDGE_OUT_GPU_TIMESTAMP {
    int eError;
    int is_ged_kpi_enabled;
};

typedef int (*like_ioctl)(int, unsigned long, ...);
void* real_ioctl = nullptr;
extern "C" int ioctl(int fd, int request, ...) {
    va_list ap,ap2;
    va_start(ap, request);
    void *ptr = va_arg(ap, void*);
    if(request == 0xc0286767) {
        GED_BRIDGE_PACKAGE* p = reinterpret_cast<GED_BRIDGE_PACKAGE*>(ptr);
        fprintf(stderr, "package, %x, %d, %d, %d\n", p->ui32FunctionID, p->i32Size, p->i32InBufferSize, p->i32OutBufferSize);
        fprintf(stderr, "package, %d, %d, %d\n", sizeof(GED_BRIDGE_PACKAGE), sizeof(GED_BRIDGE_IN_GPU_TIMESTAMP), sizeof(GED_BRIDGE_OUT_GPU_TIMESTAMP));
        GED_BRIDGE_IN_GPU_TIMESTAMP *i = reinterpret_cast<GED_BRIDGE_IN_GPU_TIMESTAMP*>(p->pvParamIn);
        fprintf(stderr, "pid = %d, ullWnd = %p, i32FrameId = %x, fence_fd = %x, QedBuffer_length = %x, isSf = %x\n",
                i->pid, i->ullWnd, i->i32FrameID, i->fence_fd, i->QedBuffer_length, i->isSF);

        GED_BRIDGE_OUT_GPU_TIMESTAMP *o = reinterpret_cast<GED_BRIDGE_OUT_GPU_TIMESTAMP*>(p->pvParamOut);
        fprintf(stderr, "... %d %d\n", o->eError, o->is_ged_kpi_enabled);
    }
    fprintf(stderr, "ioctl called for %x vs %lx\n", request, _IOWR(GED_MAGIC, GED_BRIDGE_COMMAND_GPU_TIMESTAMP, GED_BRIDGE_PACKAGE));
    int ret = (reinterpret_cast<like_ioctl>(real_ioctl))(fd, request, ptr);
    if(request == 0xc0286767) {
        GED_BRIDGE_PACKAGE* p = reinterpret_cast<GED_BRIDGE_PACKAGE*>(ptr);
        GED_BRIDGE_OUT_GPU_TIMESTAMP *i = reinterpret_cast<GED_BRIDGE_OUT_GPU_TIMESTAMP*>(p->pvParamOut);
        fprintf(stderr, "... %d %d\n", i->eError, i->is_ged_kpi_enabled);
    }
    fprintf(stderr, "... returned %d\n", ret);
    perror("hello");
    va_end(ap);
    return ret;
}

int main(int argc, char **argv, char **envp) {
    fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);
    real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);

    void* fd = dlopen("libged_kpi.so", RTLD_LAZY | RTLD_LOCAL);
    fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);
    createWrapPrototype c = reinterpret_cast<createWrapPrototype>(dlsym(fd, "ged_kpi_create_wrap"));
    queueBufferTagWrapPrototype q = reinterpret_cast<queueBufferTagWrapPrototype>(dlsym(fd, "ged_kpi_queue_buffer_tag_wrap"));
    dequeueBufferTagWrapPrototype d = reinterpret_cast<dequeueBufferTagWrapPrototype>(dlsym(fd, "ged_kpi_dequeue_buffer_tag_wrap"));
    bufferConnectWrapPrototype conn = reinterpret_cast<bufferConnectWrapPrototype>(dlsym(fd, "ged_kpi_buffer_connect"));
    acquireBufferTagWrapPrototype a = reinterpret_cast<acquireBufferTagWrapPrototype>(dlsym(fd, "ged_kpi_acquire_buffer_tag_wrap"));
    fprintf(stderr, "%s %d %p\n", __FUNCTION__, __LINE__, c);
    c(0xdeadbeef, 0xcafecafe);
    fprintf(stderr, "%s %d %p\n", __FUNCTION__, __LINE__, c);
    //conn(0xdeadbeef, 1 /* NATIVE_WINDOW_API_EGL */, getpid());
    conn(0xdeadbeef, 1 /* NATIVE_WINDOW_API_EGL */, 1337);
    fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);
    q(0xdeadbeef, 0xaaaaaaaa, 0x11111111, 0xbbbbbbbb);
    a(0xdeadbeef, 0xbbbbbbbb);
    fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__);
    d(0xdeadbeef, 0xaaaaaaaa, 0xbbbbbbbb);
    return 0;
}
