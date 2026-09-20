// thread_safety.h
// Shared thread synchronization primitives

#ifndef THREAD_SAFETY_H
#define THREAD_SAFETY_H

#include <string>
#include <mutex>
#include <atomic>

namespace framework
{
    // Mutex for recorder synchronization
    extern std::mutex recorderMutex;

    // Mutex for toast messages
    extern std::mutex toastMutex;

    // Thread control atomics
    extern std::atomic<bool> stopSimThread;
    extern std::atomic<bool> pendingExit;

    // Helper function for thread-safe toast
    void showToast(const std::string& message);
}

// Mutex for voxel buffer synchronization
extern std::mutex gVoxelBufferMutex;

#endif // THREAD_SAFETY_H
