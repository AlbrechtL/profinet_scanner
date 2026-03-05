#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <stdint.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <strings.h>

typedef uint32_t DWORD;
typedef DWORD* LPDWORD;
typedef void* LPVOID;
typedef int BOOL;
typedef unsigned int UINT;
typedef int errno_t;

#ifndef WINAPI
#define WINAPI
#endif

typedef struct thread_handle_s {
    pthread_t thread;
    bool joined;
} *HANDLE;

typedef DWORD(WINAPI *LPTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);

typedef void* LPSECURITY_ATTRIBUTES;

#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0 0
#endif

static inline void Sleep(DWORD milliseconds)
{
    usleep((useconds_t)milliseconds * 1000);
}

typedef struct thread_start_ctx_s {
    LPTHREAD_START_ROUTINE start_routine;
    LPVOID parameter;
} thread_start_ctx_t;

static inline void* platform_thread_trampoline(void* arg)
{
    thread_start_ctx_t* ctx = (thread_start_ctx_t*)arg;
    DWORD result = ctx->start_routine(ctx->parameter);
    free(ctx);
    return (void*)(uintptr_t)result;
}

static inline HANDLE CreateThread(
    LPSECURITY_ATTRIBUTES threadAttributes,
    size_t stackSize,
    LPTHREAD_START_ROUTINE startAddress,
    LPVOID parameter,
    DWORD creationFlags,
    LPDWORD threadId)
{
    (void)threadAttributes;
    (void)stackSize;
    (void)creationFlags;

    HANDLE handle = (HANDLE)malloc(sizeof(*handle));
    if (!handle) {
        return NULL;
    }

    thread_start_ctx_t* ctx = (thread_start_ctx_t*)malloc(sizeof(*ctx));
    if (!ctx) {
        free(handle);
        return NULL;
    }

    ctx->start_routine = startAddress;
    ctx->parameter = parameter;
    handle->joined = false;

    if (pthread_create(&handle->thread, NULL, platform_thread_trampoline, ctx) != 0) {
        free(ctx);
        free(handle);
        return NULL;
    }

    if (threadId) {
        *threadId = 0;
    }

    return handle;
}

static inline DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds)
{
    (void)milliseconds;
    if (!handle) {
        return (DWORD)-1;
    }

    if (!handle->joined) {
        pthread_join(handle->thread, NULL);
        handle->joined = true;
    }

    free(handle);
    return WAIT_OBJECT_0;
}

static inline int localtime_s(struct tm* tmResult, const time_t* sourceTime)
{
    return localtime_r(sourceTime, tmResult) == NULL ? -1 : 0;
}

static inline errno_t fopen_s(FILE** stream, const char* filename, const char* mode)
{
    if (!stream) {
        return EINVAL;
    }
    *stream = fopen(filename, mode);
    return *stream ? 0 : errno;
}

static inline errno_t strcpy_s(char* destination, size_t destinationSize, const char* source)
{
    if (!destination || !source || destinationSize == 0) {
        return EINVAL;
    }

    size_t srcLen = strlen(source);
    if (srcLen >= destinationSize) {
        destination[0] = '\0';
        return ERANGE;
    }

    memcpy(destination, source, srcLen + 1);
    return 0;
}

static inline char* strtok_s(char* strToken, const char* delimiters, char** context)
{
    return strtok_r(strToken, delimiters, context);
}

#define printf_s printf
#define scanf_s scanf
#define fprintf_s fprintf
#define sprintf_s snprintf
#define _snprintf_s(buffer, sizeOfBuffer, count, format, ...) snprintf((buffer), (sizeOfBuffer), (format), __VA_ARGS__)
#define _popen popen
#define _pclose pclose

#endif
