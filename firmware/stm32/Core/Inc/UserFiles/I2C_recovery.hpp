#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Recovers the I2C bus at startup, in case the transmission was cut off
mid-transfer by a microcontroller reset and SDA got stuck low. */

void I2C1_BusRecovery(void);

#ifdef __cplusplus
}
#endif