/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *          Stub platform manager for fake platform.
 */

#pragma once

#include <platform/PlatformManager.h>

namespace chip {
namespace DeviceLayer {

/**
 * Concrete implementation of the PlatformManager singleton object for Linux platforms.
 */
class PlatformManagerImpl final : public PlatformManager
{
    // Allow the PlatformManager interface class to delegate method calls to
    // the implementation methods provided by this class.
    friend PlatformManager;

public:
    // ===== Platform-specific members that may be accessed directly by the application.

private:
    // ===== Methods that implement the PlatformManager abstract interface.

    CHIP_ERROR _InitChipStack() { return CHIP_NO_ERROR; }

    CHIP_ERROR _AddEventHandler(EventHandlerFunct handler, intptr_t arg = 0) { return CHIP_ERROR_NOT_IMPLEMENTED; }
    void _RemoveEventHandler(EventHandlerFunct handler, intptr_t arg = 0) {}
    void _ScheduleWork(AsyncWorkFunct workFunct, intptr_t arg = 0) {}
    void _RunEventLoop() {}
    CHIP_ERROR _StartEventLoopTask() { return CHIP_ERROR_NOT_IMPLEMENTED; }
    CHIP_ERROR _StopEventLoopTask() { return CHIP_ERROR_NOT_IMPLEMENTED; }
    CHIP_ERROR _PostEvent(const ChipDeviceEvent * event) { return CHIP_NO_ERROR; }
    void _DispatchEvent(const ChipDeviceEvent * event) {}
    CHIP_ERROR _StartChipTimer(int64_t durationMS) { return CHIP_ERROR_NOT_IMPLEMENTED; }

    void _LockChipStack() {}
    bool _TryLockChipStack() { return true; }
    void _UnlockChipStack() {}
    CHIP_ERROR _Shutdown() { return CHIP_ERROR_NOT_IMPLEMENTED; }

    CHIP_ERROR _GetCurrentHeapFree(uint64_t & currentHeapFree);
    CHIP_ERROR _GetCurrentHeapUsed(uint64_t & currentHeapUsed);
    CHIP_ERROR _GetCurrentHeapHighWatermark(uint64_t & currentHeapHighWatermark);

    // ===== Members for internal use by the following friends.

    friend PlatformManager & PlatformMgr();
    friend PlatformManagerImpl & PlatformMgrImpl();
    friend class Internal::BLEManagerImpl;

    static PlatformManagerImpl sInstance;
};

/**
 * Returns the public interface of the PlatformManager singleton object.
 *
 * chip applications should use this to access features of the PlatformManager object
 * that are common to all platforms.
 */
inline PlatformManager & PlatformMgr()
{
    return PlatformManagerImpl::sInstance;
}

/**
 * Returns the platform-specific implementation of the PlatformManager singleton object.
 *
 * chip applications can use this to gain access to features of the PlatformManager
 * that are specific to the ESP32 platform.
 */
inline PlatformManagerImpl & PlatformMgrImpl()
{
    return PlatformManagerImpl::sInstance;
}

} // namespace DeviceLayer
} // namespace chip
