/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "firmware_exposure.h"

int main(void)
{
    printk("OSTC4 development firmware: SYNTHETIC self-test, no sensor exposure\n");
    const int status = firmware_exposure_smoke();
    /* Always fail closed, including when a smoke assertion exits early. */
    (void)ostc4_exposure_missing();
    printk("OSTC4 synthetic smoke status=%d; exposure INVALID; no dive guidance\n", status);
    return status;
}
