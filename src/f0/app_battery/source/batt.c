#include "batt.h"
#include "max17205.h"
#include "CANopen.h"

#define NCELLS          2U          /* Number of cells */
#define FASTCHG_THRSH   250U        /* Threshold below VBUS to enable Fast Charge (mV) */
#define SLOWCHG_THRSH   500U        /* Threshold below VBUS to enable Slow Charge (mV) */

/* ModelGauge Register Standard Resolutions */
#define MAX17205_STD_CAPACITY(cap) (cap*5)              /* uVh / R_SENSE */
#define MAX17205_STD_PERCENTAGE(per) (per/256)          /* % */
#define MAX17205_STD_VOLTAGE(volt) ((volt*78125)/1000)  /* mV */
#define MAX17205_STD_CURRENT(curr) ((curr*15625)/10000) /* uV / R_SENSE */
#define MAX17205_STD_TEMPERATUR(temp) (temp/256)        /* C */
#define MAX17205_STD_RESISTANCE(res) (res/4096)         /* ohm */
#define MAX17205_STD_TIME(time) ((time/4625)/1000)      /* s */

static const I2CConfig i2cconfig = {
    STM32_TIMINGR_PRESC(0xBU) |
    STM32_TIMINGR_SCLDEL(0x4U) | STM32_TIMINGR_SDADEL(0x2U) |
    STM32_TIMINGR_SCLH(0xFU)  | STM32_TIMINGR_SCLL(0x13U),
    0,
    0
};

static const max17205_regval_t batt_cfg[] = {
    {MAX17205_AD_PACKCFG, MAX17205_SETVAL(MAX17205_AD_PACKCFG, _VAL2FLD(MAX17205_PACKCFG_NCELLS, NCELLS) | MAX17205_PACKCFG_BALCFG_40 | MAX17205_PACKCFG_CHEN)},
    {0,0}
};

static const MAX17205Config max17205config = {
    &I2CD1,
    &i2cconfig,
    batt_cfg
};

static MAX17205Driver max17205dev;

/* Battery monitoring thread */
THD_WORKING_AREA(batt_wa, 0x400);
THD_FUNCTION(batt, arg)
{
    (void)arg;

    max17205ObjectInit(&max17205dev);
    max17205Start(&max17205dev, &max17205config);

    while (!chThdShouldTerminateX()) {
        chThdSleepMilliseconds(1000);

        /* Record pack and cell voltages to object dictionary */
        OD_battery.cell1 = MAX17205_STD_VOLTAGE(max17205ReadRaw(&max17205dev, MAX17205_AD_AVGCELL1));
        OD_battery.cell2 = MAX17205_STD_VOLTAGE(max17205ReadRaw(&max17205dev, MAX17205_AD_AVGCELL2));
        OD_battery.VCell = MAX17205_STD_VOLTAGE(max17205ReadRaw(&max17205dev, MAX17205_AD_AVGVCELL));

        if (OD_battery.VCell >= OD_battery.VBUS - FASTCHG_THRSH) {
            /*palSetLine(LINE_FASTCHG);*/
        } else if (OD_battery.VCell >= OD_battery.VBUS - SLOWCHG_THRSH) {
            /*palClearLine(LINE_FASTCHG);*/
        } else {
            /*palClearLine(LINE_FASTCHG);*/
        }

        /* capacity */
        OD_battery.fullCapacity = MAX17205_STD_CAPACITY(max17205ReadRaw(&max17205dev, MAX17205_AD_FULLCAP));
        OD_battery.availableCapacity = MAX17205_STD_CAPACITY(max17205ReadRaw(&max17205dev, MAX17205_AD_AVCAP));
        OD_battery.mixCapacity = MAX17205_STD_CAPACITY(max17205ReadRaw(&max17205dev, MAX17205_AD_MIXCAP));

        /* state of charge */
        OD_battery.timeToEmpty = MAX17205_STD_TIME(max17205ReadRaw(&max17205dev, MAX17205_AD_TTE));
        OD_battery.timeToFull = MAX17205_STD_TIME(max17205ReadRaw(&max17205dev, MAX17205_AD_TTF));
        OD_battery.availableStateOfCharge = MAX17205_STD_PERCENTAGE(max17205ReadRaw(&max17205dev, MAX17205_AD_AVSOC));
        OD_battery.presentStateOfCharge = MAX17205_STD_PERCENTAGE(max17205ReadRaw(&max17205dev, MAX17205_AD_VFSOC));

        /* other info */
        OD_battery.cycles = max17205ReadRaw(&max17205dev, MAX17205_AD_CYCLES);

        /* Toggle LED */
        palToggleLine(LINE_LED);
    }

    max17205Stop(&max17205dev);

    palClearLine(LINE_LED);
    chThdExit(MSG_OK);
}
