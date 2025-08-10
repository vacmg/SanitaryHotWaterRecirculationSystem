#include "EspOSInterface.h"
#include <cstdlib>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>


class espMutex final : public OSInterface_Mutex
{
public:
    espMutex()
    {
        mutex = xSemaphoreCreateMutex();
    }

    ~espMutex() override
    {
        vSemaphoreDelete(mutex);
    }

    void signal() override
    {
        if (xSemaphoreGive(mutex) == pdFALSE)
        {
            // Crash if we can't give the semaphore.
            OSInterfaceLogError("EspOSInterface", "Failed to give mutex");
            abort();
        }
    }

    bool wait(const uint32_t max_time_to_wait_ms) override
    {
        return xSemaphoreTake(mutex, pdMS_TO_TICKS(max_time_to_wait_ms)) == pdTRUE;
    }

private:
    SemaphoreHandle_t mutex{};
};

class espBinarySemaphore final : public OSInterface_BinarySemaphore
{
public:
    espBinarySemaphore()
    {
        semaphore = xSemaphoreCreateBinary();
    }

    ~espBinarySemaphore() override
    {
        vSemaphoreDelete(semaphore);
    }

    void signal() override
    {
        if (xSemaphoreGive(semaphore) == pdFALSE)
        {
            // Crash if we can't give the semaphore.
            OSInterfaceLogError("EspOSInterface", "Failed to give semaphore");
            abort();
        }
    }

    bool wait(const uint32_t max_time_to_wait_ms) override
    {
        return xSemaphoreTake(semaphore, pdMS_TO_TICKS(max_time_to_wait_ms)) == pdTRUE;
    }

private:
    SemaphoreHandle_t semaphore{};
};

uint32_t EspOSInterface::osMillis()
{
    return pdTICKS_TO_MS(xTaskGetTickCount());
}

void EspOSInterface::osSleep(const uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

OSInterface_Mutex* EspOSInterface::osCreateMutex()
{
    return new espMutex();
}

OSInterface_BinarySemaphore* EspOSInterface::osCreateBinarySemaphore()
{
    return new espBinarySemaphore();
}

void* EspOSInterface::osMalloc(const uint32_t size)
{
    return heap_caps_malloc_prefer(size, 2,MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM, MALLOC_CAP_8BIT | MALLOC_CAP_DEFAULT);
}

void EspOSInterface::osFree(void* ptr)
{
    heap_caps_free(ptr);
}

void EspOSInterface::osRunProcess(const OSInterfaceProcess process, void* arg)
{
    osRunProcess(process, "NewProcess", arg);
}

void EspOSInterface::osRunProcess(const OSInterfaceProcess process, const char* processName, void* arg)
{
    ProcessData* processData = new ProcessData{.process = process, .arg = arg, .processName = processName};
    const auto res = xTaskCreate(osRunProcessLauncher, processName, processDefaultStackSize, processData, processDefaultPriority,
                nullptr);
    if (res != pdPASS)
    {
        OSInterfaceLogError("EspOSInterface", "Failed to create task for process %s", processName);
        delete processData;
    }
}

void EspOSInterface::osRunProcessLauncher(void* data)
{
    auto* processData = static_cast<ProcessData*>(data);
    OSInterfaceLogInfo("OSInterface", "Running process %s", processData->processName);
    processData->process(processData->arg);
    delete processData;
    vTaskDelete(nullptr);
}
