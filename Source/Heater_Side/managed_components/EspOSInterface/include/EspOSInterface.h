#ifndef OSESPINTERFACE_H
#define OSESPINTERFACE_H

#include "OSInterface.h"

#include "esp_log.h"

#ifdef OSInterfaceLogVerbose
#undef OSInterfaceLogVerbose
#endif
#define OSInterfaceLogVerbose(tag, format, ...) ESP_LOGV(tag, format, ##__VA_ARGS__)

#ifdef OSInterfaceLogDebug
#undef OSInterfaceLogDebug
#endif
#define OSInterfaceLogDebug(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)

#ifdef OSInterfaceLogInfo
#undef OSInterfaceLogInfo
#endif
#define OSInterfaceLogInfo(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)

#ifdef OSInterfaceLogWarning
#undef OSInterfaceLogWarning
#endif
#define OSInterfaceLogWarning(tag, format, ...) ESP_LOGW(tag, AT format, ##__VA_ARGS__)

#ifdef OSInterfaceLogError
#undef OSInterfaceLogError
#endif
#define OSInterfaceLogError(tag, format, ...) ESP_LOGE(tag, AT format, ##__VA_ARGS__)

#ifdef OSInterfaceSetLogLevel
#undef OSInterfaceSetLogLevel
#endif
#define OSInterfaceSetLogLevel(tag, level) esp_log_level_set(tag, static_cast<esp_log_level_t>(level))

#ifdef OSInterfaceGetLogLevel
#undef OSInterfaceGetLogLevel
#endif
#define OSInterfaceGetLogLevel(tag) esp_log_get_level(tag)

class EspOSInterface : public OSInterface
{
public:
    void osSleep(uint32_t ms) override;

    uint32_t osMillis() override;

    OSInterface_Mutex* osCreateMutex() override;

    OSInterface_BinarySemaphore* osCreateBinarySemaphore() override;

    void* osMalloc(uint32_t size) override;

    void osFree(void* ptr) override;

    void osRunProcess(OSInterfaceProcess process, void* arg) override;

    void osRunProcess(OSInterfaceProcess process, const char* processName, void* arg) override;

private:
    static constexpr uint32_t processDefaultStackSize = 2048;
    static constexpr uint8_t processDefaultPriority = 5;
    using ProcessData = struct
    {
        OSInterfaceProcess process;
        void* arg;
        const char* processName;
    };

    static void osRunProcessLauncher(void* data);
};

#endif // OSESPINTERFACE_H
