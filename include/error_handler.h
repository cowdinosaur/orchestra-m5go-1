#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "esp_err.h"
#include "esp_log.h"

// Error handling macros for consistent error management

// Check and return on error with logging
#define CHECK_ERROR_RETURN(x, tag, msg, ...) do {                              \
    esp_err_t __err = (x);                                                     \
    if (__err != ESP_OK) {                                                     \
        ESP_LOGE(tag, msg ": %s", ##__VA_ARGS__, esp_err_to_name(__err));     \
        return __err;                                                          \
    }                                                                           \
} while(0)

// Check and goto error handler with logging
#define CHECK_ERROR_GOTO(x, tag, msg, label, ...) do {                         \
    esp_err_t __err = (x);                                                     \
    if (__err != ESP_OK) {                                                     \
        ESP_LOGE(tag, msg ": %s", ##__VA_ARGS__, esp_err_to_name(__err));     \
        goto label;                                                            \
    }                                                                           \
} while(0)

// Check and continue on error with warning
#define CHECK_ERROR_CONTINUE(x, tag, msg, ...) do {                            \
    esp_err_t __err = (x);                                                     \
    if (__err != ESP_OK) {                                                     \
        ESP_LOGW(tag, msg ": %s", ##__VA_ARGS__, esp_err_to_name(__err));     \
    }                                                                           \
} while(0)

// Validate pointer with null check
#define VALIDATE_POINTER(ptr, tag, name) do {                                  \
    if ((ptr) == NULL) {                                                       \
        ESP_LOGE(tag, "%s is NULL", name);                                     \
        return ESP_ERR_INVALID_ARG;                                            \
    }                                                                           \
} while(0)

// Validate range with bounds check
#define VALIDATE_RANGE(val, min, max, tag, name) do {                          \
    if ((val) < (min) || (val) > (max)) {                                      \
        ESP_LOGE(tag, "%s out of range [%d, %d]: %d",                         \
                 name, (int)(min), (int)(max), (int)(val));                    \
        return ESP_ERR_INVALID_ARG;                                            \
    }                                                                           \
} while(0)

// Safe mutex operations
#define TAKE_MUTEX_SAFE(mutex, timeout, tag) do {                              \
    if (xSemaphoreTake(mutex, timeout) != pdTRUE) {                           \
        ESP_LOGE(tag, "Failed to take mutex");                                 \
        return ESP_ERR_TIMEOUT;                                                \
    }                                                                           \
} while(0)

#define GIVE_MUTEX_SAFE(mutex) xSemaphoreGive(mutex)

// Safe queue operations
#define QUEUE_SEND_SAFE(queue, item, timeout, tag) do {                        \
    if (xQueueSend(queue, item, timeout) != pdTRUE) {                         \
        ESP_LOGW(tag, "Queue full, dropping item");                            \
        return ESP_ERR_NO_MEM;                                                 \
    }                                                                           \
} while(0)

// Memory allocation with null check
#define ALLOC_CHECK(ptr, size, tag) do {                                       \
    ptr = malloc(size);                                                        \
    if ((ptr) == NULL) {                                                       \
        ESP_LOGE(tag, "Failed to allocate %u bytes", (unsigned)(size));       \
        return ESP_ERR_NO_MEM;                                                 \
    }                                                                           \
} while(0)

// Standard error recovery strategies
typedef enum {
    ERROR_STRATEGY_IGNORE,      // Log and continue
    ERROR_STRATEGY_RETRY,       // Retry the operation
    ERROR_STRATEGY_RESET,       // Reset the module
    ERROR_STRATEGY_PANIC        // System panic/restart
} error_strategy_t;

// Error recovery function
static inline esp_err_t handle_error_with_strategy(esp_err_t err,
                                                    error_strategy_t strategy,
                                                    const char *tag,
                                                    const char *msg) {
    if (err == ESP_OK) return ESP_OK;

    switch (strategy) {
        case ERROR_STRATEGY_IGNORE:
            ESP_LOGW(tag, "%s: %s (ignoring)", msg, esp_err_to_name(err));
            return ESP_OK;

        case ERROR_STRATEGY_RETRY:
            ESP_LOGW(tag, "%s: %s (will retry)", msg, esp_err_to_name(err));
            return err;

        case ERROR_STRATEGY_RESET:
            ESP_LOGE(tag, "%s: %s (resetting module)", msg, esp_err_to_name(err));
            return err;

        case ERROR_STRATEGY_PANIC:
            ESP_LOGE(tag, "%s: %s (system panic)", msg, esp_err_to_name(err));
            esp_restart();
            return err;  // Never reached

        default:
            return err;
    }
}

#endif // ERROR_HANDLER_H