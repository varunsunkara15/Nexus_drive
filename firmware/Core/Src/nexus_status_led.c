#include "nexus_status_led.h"
#include "main.h"

void NexusStatusLed_Update(NexusDriveState state, uint32_t now_ms)
{
    const uint32_t channels = LED_R_Pin | LED_G_Pin | LED_B_Pin;
    uint32_t lit;

    switch (state) {
    case NEXUS_DRIVE_IDLE:
        lit = LED_B_Pin;
        break;
    case NEXUS_DRIVE_READY:
        lit = LED_G_Pin | LED_B_Pin;
        break;
    case NEXUS_DRIVE_RUNNING:
        lit = (now_ms % 1000U < 250U) ? LED_G_Pin : 0U;
        break;
    case NEXUS_DRIVE_FAULT:
    default:
        lit = LED_R_Pin;
        break;
    }

    /* PB6, PB7 and PB9 sink current. Update all three in one GPIO write. */
    GPIOB->BSRR = (channels & ~lit) | (lit << 16);
}
