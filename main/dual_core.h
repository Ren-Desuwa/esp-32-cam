#ifndef DUAL_CORE_H
#define DUAL_CORE_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Task handle for core 0 task
TaskHandle_t core0TaskHandle = NULL;

// Stack size for core 0 task
#define CORE0_STACK_SIZE 8192

// Task priority
#define CORE0_TASK_PRIORITY 1
#define ENABLE_CORE0_LOOP2   // Comment this out to disable

#ifdef ENABLE_CORE0_LOOP2
// Function declaration for loop2
void loop2();
#endif

// Core 0 task wrapper function
void core0Task(void* parameter) {
    for (;;) {
    #ifdef ENABLE_CORE0_LOOP2
        loop2();
    #endif
        vTaskDelay(1); // Let other tasks run
    }
}

// Function to start the dual core functionality
void startDualCore() {
    xTaskCreatePinnedToCore(
        core0Task,
        "Core0Task",
        CORE0_STACK_SIZE,
        NULL,
        CORE0_TASK_PRIORITY,
        &core0TaskHandle,
        0
    );
}

// Function to stop core 0 task
void stopCore0() {
    if (core0TaskHandle != NULL) {
        vTaskDelete(core0TaskHandle);
        core0TaskHandle = NULL;
    }
}

// Function to check if core 0 is running
bool isCore0Running() {
    return (core0TaskHandle != NULL);
}

// Task creation helper
static inline void createPinnedTask(
    TaskFunction_t taskFunction,
    const char* taskName,
    uint32_t stackSize,
    void* parameters,
    UBaseType_t priority,
    TaskHandle_t* taskHandle, // corrected type
    BaseType_t coreId
) {
    xTaskCreatePinnedToCore(
        taskFunction,
        taskName,
        stackSize,
        parameters,
        priority,
        taskHandle,
        coreId
    );
}

// Task stop helper
static inline void stopTask(TaskHandle_t* taskHandle) { // corrected type
    if (*taskHandle != NULL) {
        vTaskDelete(*taskHandle);
        *taskHandle = NULL;
    }
}


#endif // DUAL_CORE_H