#ifndef NEXUS_STATUS_LED_H
#define NEXUS_STATUS_LED_H

#include <stdint.h>

typedef enum {
    NEXUS_DRIVE_IDLE,
    NEXUS_DRIVE_READY,
    NEXUS_DRIVE_RUNNING,
    NEXUS_DRIVE_FAULT
} NexusDriveState;

/* Call periodically after MX_GPIO_Init(), using the drive state and HAL_GetTick(). */
void NexusStatusLed_Update(NexusDriveState state, uint32_t now_ms);

#endif
