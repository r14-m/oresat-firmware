#include "ch.h"
#include "batt.h"
#include "max17205.h"
#include "CANopen.h"
#include "chprintf.h"

#define DEBUG_SERIAL    (BaseSequentialStream*) &SD2

#define NCELLS          2U          /* Number of cells */

typedef enum {
	BATTERY_STATE_MACHINE_STATE_NOT_HEATING = 0,
	BATTERY_STATE_MACHINE_STATE_HEATING,
} battery_heating_state_machine_state_t;


static const I2CConfig i2cconfig_1 = {
    STM32_TIMINGR_PRESC(0xBU) |
    STM32_TIMINGR_SCLDEL(0x4U) | STM32_TIMINGR_SDADEL(0x2U) |
    STM32_TIMINGR_SCLH(0xFU)  | STM32_TIMINGR_SCLL(0x13U),
    0,
    0
};

//TODO timing for i2c2 is different the i2c1. Scope the lines and match them up.
static const I2CConfig i2cconfig_2 = {
    STM32_TIMINGR_PRESC(0xFU) |
    STM32_TIMINGR_SCLDEL(0x4U) | STM32_TIMINGR_SDADEL(0x2U) |
    STM32_TIMINGR_SCLH(0xFU)  | STM32_TIMINGR_SCLL(0x13U),
    0,
    0
};

static const max17205_regval_t batt_cfg[] = {
    {MAX17205_AD_PACKCFG, MAX17205_SETVAL(MAX17205_AD_PACKCFG,
                                          _VAL2FLD(MAX17205_PACKCFG_NCELLS, NCELLS) |
                                          MAX17205_PACKCFG_BALCFG_40 |
										  MAX17205_PACKCFG_CHEN |
										  MAX17205_PACKCFG_TDEN |
										  MAX17205_PACKCFG_A1EN |
										  MAX17205_PACKCFG_A2EN )},
    {MAX17205_AD_NRSENSE, MAX17205_RSENSE2REG(10000U)},
    {0,0}
};

static const MAX17205Config max17205configPack1 = {
	&I2CD2,
	&i2cconfig_2,
    batt_cfg
};

static const MAX17205Config max17205configPack2 = {
	&I2CD1,
	&i2cconfig_1,
    batt_cfg
};

static MAX17205Driver max17205devPack1;
static MAX17205Driver max17205devPack2;

typedef struct {
	bool is_data_valid;

	uint16_t cell1_mV;
	uint16_t cell2_mV;
	uint16_t VCell_mV;
	uint16_t VCell_max_volt_mV;
	uint16_t VCell_min_volt_mV;
	uint16_t batt_mV;//x

	int16_t avg_current_mA;
	int16_t max_current_mA;
	int16_t min_current_mA;

	uint16_t available_state_of_charge; //Percent
	uint16_t present_state_of_charge; //Percent

	uint16_t time_to_full; // seconds
	uint16_t time_to_empty; // seconds

	uint16_t full_capacity_mAh;
	uint16_t available_capacity_mAh;
	uint16_t mix_capacity;
	uint16_t reporting_capacity_mAh;

	uint16_t cycles; // count

	int16_t avg_temp_1_C;
	int16_t avg_temp_2_C;
	int16_t avg_int_temp_C;
} batt_pack_data_t;

static batt_pack_data_t pack_1_data;
static batt_pack_data_t pack_2_data;

battery_heating_state_machine_state_t current_batery_state_machine_state = BATTERY_STATE_MACHINE_STATE_NOT_HEATING;


void run_battery_heating_state_machine(batt_pack_data_t *pk1_data, batt_pack_data_t *pk2_data) {
	if( pk1_data->is_data_valid && pk2_data->is_data_valid ) {
		int16_t avg_temp_1_C = pk1_data->avg_temp_1_C;
		int16_t avg_temp_2_C = pk2_data->avg_temp_1_C;

		uint16_t present_state_of_charge_1 = pk1_data->present_state_of_charge;
		uint16_t present_state_of_charge_2 = pk2_data->present_state_of_charge;

		const uint16_t total_state_of_charge = (present_state_of_charge_1 + present_state_of_charge_2) / 2;


		switch (current_batery_state_machine_state) {
			case BATTERY_STATE_MACHINE_STATE_HEATING:
				chprintf(DEBUG_SERIAL, "Turning heaters ON\r\n");
				palSetLine(LINE_MOARPWR);
				palSetLine(LINE_HEATER_ON_1);
				palSetLine(LINE_HEATER_ON_2);
				//Once they’re greater than 5 °C or the combined pack capacity is < 25%

				if( (avg_temp_1_C > 5 && avg_temp_2_C > 5) || (total_state_of_charge < 25) ) {
					current_batery_state_machine_state = BATTERY_STATE_MACHINE_STATE_NOT_HEATING;
				}
				break;
			case BATTERY_STATE_MACHINE_STATE_NOT_HEATING:
				chprintf(DEBUG_SERIAL, "Turning heaters OFF\r\n");
				palClearLine(LINE_HEATER_ON_1);
				palClearLine(LINE_HEATER_ON_2);
				palClearLine(LINE_MOARPWR);

				if( (avg_temp_1_C < -5 || avg_temp_2_C < -5) && (present_state_of_charge_1 > 25 || present_state_of_charge_2 > 25) ) {
					current_batery_state_machine_state = BATTERY_STATE_MACHINE_STATE_HEATING;
				}
				break;
			default:
				current_batery_state_machine_state = BATTERY_STATE_MACHINE_STATE_NOT_HEATING;
				break;
		}
	} else {
		//Fail safe
		palClearLine(LINE_HEATER_ON_1);
		palClearLine(LINE_HEATER_ON_2);
		palClearLine(LINE_MOARPWR);
	}
}


void update_battery_charging_state(batt_pack_data_t *pk_data, const ioline_t line_dchg_dis, const ioline_t line_chg_dis, const char* pk_str) {

	chprintf(DEBUG_SERIAL, "LINE_DCHG_STAT_PK1 = %u\r\n", palReadLine(LINE_DCHG_STAT_PK1));
	chprintf(DEBUG_SERIAL, "LINE_CHG_STAT_PK1 = %u\r\n", palReadLine(LINE_CHG_STAT_PK1));

	chprintf(DEBUG_SERIAL, "LINE_DCHG_STAT_PK2 = %u\r\n", palReadLine(LINE_DCHG_STAT_PK2));
	chprintf(DEBUG_SERIAL, "LINE_CHG_STAT_PK2 = %u\r\n", palReadLine(LINE_CHG_STAT_PK2));

	/*

Turning heaters OFF
LINE_DCHG_STAT_PK1 = 1
LINE_CHG_STAT_PK1 = 1
LINE_DCHG_STAT_PK2 = 1
LINE_CHG_STAT_PK2 = 1
Disabling discharge on PK1
Enabling charging on PK1
LINE_DCHG_STAT_PK1 = 1
LINE_CHG_STAT_PK1 = 1
LINE_DCHG_STAT_PK2 = 1
LINE_CHG_STAT_PK2 = 1
Enabling discharge on PK2
Enabling charging on PK2

	 */




	if( pk_data->is_data_valid ) {
		if( pk_data->VCell_mV < 3000 || pk_data->present_state_of_charge < 20 ) {
			//Disable discharge on both packs
			chprintf(DEBUG_SERIAL, "Disabling discharge on %s\r\n", pk_str);
			palSetLine(line_dchg_dis);
		} else {
			chprintf(DEBUG_SERIAL, "Enabling discharge on %s\r\n", pk_str);
			//Allow discharge on both packs
			palClearLine(line_dchg_dis);
		}


		if( pk_data->VCell_mV > 4100 ) {
			chprintf(DEBUG_SERIAL, "Disabling charging on %s\r\n", pk_str);
			palSetLine(line_chg_dis);
		} else {
			chprintf(DEBUG_SERIAL, "Enabling charging on %s\r\n", pk_str);
			palClearLine(line_chg_dis);
			if( pk_data->present_state_of_charge > 90 ) {
				const int16_t vcell_delta_mV = pk_data->cell1_mV - pk_data->cell2_mV;

				if( vcell_delta_mV < -50 || vcell_delta_mV > 50 ) {
					//TODO command cell  balancing - this appears to be done in hardware based on config registers???
				}
			}
		}
	} else {
		//fail safe mode
		palSetLine(line_dchg_dis);
		palSetLine(line_chg_dis);
	}
}

/*

=================================
Populating Pack 1 Data
  max17205ReadTemperatureChecked(0x137 MAX17205_AD_AVGTEMP1) = 23 C (raw: 2964 0xB94)
  max17205ReadTemperatureChecked(0x139 MAX17205_AD_AVGTEMP2) = 23 C (raw: 2964 0xB94)
  max17205ReadTemperatureChecked(0x138 MAX17205_AD_AVGINTTEMP) = 23 C (raw: 2964 0xB94)
avg_temp_1_C = 23 C, avg_temp_2_C = 23 C, avg_int_temp_C = 23 C
  max17205ReadVoltageChecked(0xD4 MAX17205_AD_AVGCELL1) = 4064 mV
  max17205ReadVoltageChecked(0x19 MAX17205_AD_AVGVCELL) = 4063 mV
  max17205ReadBattVoltage(0xDA MAX17205_AD_BATT) = 8130 mV
cell1_mV = 4064, cell2_mV = 4066, VCell_mV = 4063, batt_mV = 8130
max_volt_mV = 4060, min_volt_mV = 4040
  max17205ReadCurrent(0xB MAX17205_AD_AVGCURRENT) = 88 mA (raw: 564 0x234)
max_mA = 80, min_mA = 80
avg_current_mA = 88 mA
  max17205ReadCapacityChecked(0x10 MAX17205_AD_FULLCAP) = 1477 mAh (raw: 2955 0xB8B)
  max17205ReadCapacityChecked(0x1F MAX17205_AD_AVCAP) = 1113 mAh (raw: 2227 0x8B3)
  max17205ReadCapacityChecked(0xF MAX17205_AD_MIXCAP) = 1135 mAh (raw: 2271 0x8DF)
  max17205ReadCapacityChecked(0x5 MAX17205_AD_REPCAP) = 1134 mAh (raw: 2268 0x8DC)
full_capacity_mAh = 1477, available_capacity_mAh = 1113, mix_capacity = 1135
  max17205ReadTimeChecked(0x11 MAX17205_AD_TTE) = 40954 seconds (682 minutes) (raw: 65535 0xFFFF)
  max17205ReadTimeChecked(0x20 MAX17205_AD_TTF) = 0 seconds (0 minutes) (raw: 0 0x0)
  max17205ReadPercentageChecked(0xE MAX17205_AD_AVSOC) = 75% (raw: 19290 0x4B5A)
  max17205ReadPercentageChecked(0xFF MAX17205_AD_VFSOC) = 75% (raw: 19325 0x4B7D)
time_to_empty = 40954 (seconds), time_to_full = 0 (seconds), available_state_of_charge = 75%, present_state_of_charge = 75%
cycles = 0

Populating Pack 2 Data

max_mA = 0, min_mA = -40
avg_current_mA = 0 mA
  max17205ReadCapacityChecked(0x10 MAX17205_AD_FULLCAP) = 1477 mAh (raw: 2955 0xB8B)
  max17205ReadCapacityChecked(0x1F MAX17205_AD_AVCAP) = 109 mAh (raw: 219 0xDB)
  max17205ReadCapacityChecked(0xF MAX17205_AD_MIXCAP) = 131 mAh (raw: 263 0x107)
  max17205ReadCapacityChecked(0x5 MAX17205_AD_REPCAP) = 132 mAh (raw: 264 0x108)
full_capacity_mAh = 1477, available_capacity_mAh = 109, mix_capacity = 131
  max17205ReadTimeChecked(0x11 MAX17205_AD_TTE) = 40954 seconds (682 minutes) (raw: 65535 0xFFFF)
  max17205ReadTimeChecked(0x20 MAX17205_AD_TTF) = 40954 seconds (682 minutes) (raw: 65535 0xFFFF)
  max17205ReadPercentageChecked(0xE MAX17205_AD_AVSOC) = 7% (raw: 1896 0x768)
  max17205ReadPercentageChecked(0xFF MAX17205_AD_VFSOC) = 8% (raw: 2251 0x8CB)
time_to_empty = 40954 (seconds), time_to_full = 40954 (seconds), available_state_of_charge = 7%, present_state_of_charge = 8%

 */



bool populate_pack_data(MAX17205Driver *driver, batt_pack_data_t *dest) {
	msg_t r = 0;
	memset(dest, 0, sizeof(*dest));
	dest->is_data_valid = true;


    if( (r = max17205ReadAverageTemperature(driver, MAX17205_AD_AVGTEMP1, &dest->avg_temp_1_C)) != MSG_OK ) {
    	dest->is_data_valid = false;
    }
    if( (r = max17205ReadAverageTemperature(driver, MAX17205_AD_AVGTEMP2, &dest->avg_temp_2_C)) != MSG_OK ) {
    	dest->is_data_valid = false;
    }
    if( (r = max17205ReadAverageTemperature(driver, MAX17205_AD_AVGINTTEMP, &dest->avg_int_temp_C)) != MSG_OK ) {
    	dest->is_data_valid = false;
    }

    chprintf(DEBUG_SERIAL, "avg_temp_1_C = %d C, ", dest->avg_temp_1_C);
    chprintf(DEBUG_SERIAL, "avg_temp_2_C = %d C, ", dest->avg_temp_2_C);
    chprintf(DEBUG_SERIAL, "avg_int_temp_C = %d C", dest->avg_int_temp_C);
    chprintf(DEBUG_SERIAL, "\r\n");

    /* Record pack and cell voltages to object dictionary */
    if( (r = max17205ReadVoltage(driver, MAX17205_AD_AVGCELL1, &dest->cell1_mV)) != MSG_OK ) {
    	dest->is_data_valid = false;
    }

    if( (r = max17205ReadVoltage(driver, MAX17205_AD_AVGVCELL, &dest->VCell_mV)) != MSG_OK ) {
    	dest->is_data_valid = false;
    }
    if( (r = max17205ReadBattVoltage(driver, MAX17205_AD_BATT, &dest->batt_mV)) != MSG_OK ) {
    	dest->is_data_valid = false;
    }

    if( dest->is_data_valid ) {
    	dest->cell2_mV = dest->batt_mV - dest->cell1_mV;
    }

    chprintf(DEBUG_SERIAL, "cell1_mV = %u, cell2_mV = %u, VCell_mV = %u, batt_mV = %u\r\n", dest->cell1_mV, dest->cell2_mV, dest->VCell_mV, dest->batt_mV);

    uint16_t max_min_volt_raw = 0;
    if( (r = max17205ReadRaw(driver, MAX17205_AD_MAXMINVOLT, &max_min_volt_raw)) != MSG_OK ) {
		dest->is_data_valid = false;
	} else {
		dest->VCell_max_volt_mV = (max_min_volt_raw >> 8) * 20;
		dest->VCell_min_volt_mV = (max_min_volt_raw & 0xFF) * 20;
		chprintf(DEBUG_SERIAL, "VCell_max_volt_mV = %u, VCell_min_volt_mV = %u\r\n", dest->VCell_max_volt_mV, dest->VCell_min_volt_mV);
	}


    if( (r = max17205ReadCurrent(driver, MAX17205_AD_AVGCURRENT, &dest->avg_current_mA)) != MSG_OK ) {
		dest->is_data_valid = false;
	}

    uint16_t max_min_current_raw = 0;
    if( (r = max17205ReadRaw(driver, MAX17205_AD_MAXMINCURR, &max_min_current_raw)) != MSG_OK ) {
		dest->is_data_valid = false;
	} else {
		//Assumes Rsense = 0.01 ohms
		int8_t max_raw = (max_min_current_raw >> 8);
		int8_t min_raw = max_min_current_raw & 0xFF;
		dest->max_current_mA = ((int16_t) max_raw) * 40;// 0.0004/0.01 = 0.04
		dest->min_current_mA = ((int16_t) min_raw) * 40;// 0.0004/0.01 = 0.04

		chprintf(DEBUG_SERIAL, "max_mA = %d, min_mA = %d\r\n", dest->max_current_mA, dest->min_current_mA);
	}

    chprintf(DEBUG_SERIAL, "avg_current_mA = %d mA\r\n", dest->avg_current_mA);


    /* capacity */
	if( (r = max17205ReadCapacity(driver, MAX17205_AD_FULLCAP, &dest->full_capacity_mAh)) != MSG_OK ) {
		dest->is_data_valid = false;
    }
	if( (r = max17205ReadCapacity(driver, MAX17205_AD_AVCAP, &dest->available_capacity_mAh)) != MSG_OK ) {
		dest->is_data_valid = false;
	}
	if( (r = max17205ReadCapacity(driver, MAX17205_AD_MIXCAP, &dest->mix_capacity)) != MSG_OK ) {
		dest->is_data_valid = false;
	}
	if( (r = max17205ReadCapacity(driver, MAX17205_AD_REPCAP, &dest->reporting_capacity_mAh)) != MSG_OK ) {
		dest->is_data_valid = false;
	}



    chprintf(DEBUG_SERIAL, "full_capacity_mAh = %u, available_capacity_mAh = %u, mix_capacity = %u\r\n", dest->full_capacity_mAh, dest->available_capacity_mAh, dest->mix_capacity);



    /* state of charge */
    if( (r = max17205ReadTime(driver, MAX17205_AD_TTE, &dest->time_to_empty)) != MSG_OK ) {
    	dest->is_data_valid = false;
	}
    if( (r = max17205ReadTime(driver, MAX17205_AD_TTF, &dest->time_to_full)) != MSG_OK ) {
    	dest->is_data_valid = false;
	}

    if( (r = max17205ReadPercentage(driver, MAX17205_AD_AVSOC, &dest->available_state_of_charge)) != MSG_OK ) {
    	dest->is_data_valid = false;
	}
    if( (r = max17205ReadPercentage(driver, MAX17205_AD_VFSOC, &dest->present_state_of_charge)) != MSG_OK ) {
    	dest->is_data_valid = false;
	}


    chprintf(DEBUG_SERIAL, "time_to_empty = %u (seconds), time_to_full = %u (seconds), available_state_of_charge = %u%%, present_state_of_charge = %u%%\r\n", dest->time_to_empty, dest->time_to_full, dest->available_state_of_charge, dest->present_state_of_charge);

    /* other info */
    if( (r = max17205ReadRaw(driver, MAX17205_AD_CYCLES, &dest->cycles)) != MSG_OK ) {
    	dest->is_data_valid = false;
	}

    chprintf(DEBUG_SERIAL, "cycles = %u\r\n", dest->cycles);


    return(dest->is_data_valid);
}

/* Battery monitoring thread */
THD_WORKING_AREA(batt_wa, 0x400);
THD_FUNCTION(batt, arg)
{
    (void)arg;

    max17205ObjectInit(&max17205devPack1);
    max17205ObjectInit(&max17205devPack2);
    const bool pack_1_init_flag = max17205Start(&max17205devPack1, &max17205configPack1);
    chprintf(DEBUG_SERIAL, "max17205Start(pack1) = %u\r\n", pack_1_init_flag);

    max17205PrintintNonvolatileMemory(&max17205configPack1);
    while(1) {
    	chThdSleepMilliseconds(1000);
    }

    const bool pack_2_init_flag = max17205Start(&max17205devPack2, &max17205configPack2);
    chprintf(DEBUG_SERIAL, "max17205Start(pack2) = %u\r\n", pack_2_init_flag);

#if 0
    //TODO delete this block
    while (!chThdShouldTerminateX()) {
    	chprintf(DEBUG_SERIAL, "=================================\r\n");
    	chThdSleepMilliseconds(1000);

		i2cStart(max17205configPack2.i2cp, max17205configPack2.i2ccfg);
		max17205devPack2.state = MAX17205_READY;
		chprintf(DEBUG_SERIAL, "Done starting i2c, %u\r\n", max17205configPack2.i2cp->state);
		chThdSleepMilliseconds(100);

		uint16_t cell1_mV = 0;
		uint16_t cell2_mV = 0;
		uint16_t VCell_mV = 0;

		msg_t r;
    	r = max17205ReadVoltage(&max17205devPack2, MAX17205_AD_AVGCELL1, &cell1_mV);
		//r = max17205ReadVoltageChecked(&max17205dev, MAX17205_AD_AVGCELL2, &cell2);
		//r = max17205ReadVoltageChecked(&max17205dev, MAX17205_AD_AVGVCELL, &VCell);

		chprintf(DEBUG_SERIAL, "cell1 = %u, cell2 = %u, VCell = %u\r\n", cell1_mV, cell2_mV, VCell_mV);

		i2cStop(max17205configPack2.i2cp);
    }
#endif

    uint16_t pack_1_comm_rx_error_count = 0;
    uint16_t pack_2_comm_rx_error_count = 0;
    while (!chThdShouldTerminateX()) {
    	chprintf(DEBUG_SERIAL, "=================================\r\n");
    	chThdSleepMilliseconds(4000);

    	chprintf(DEBUG_SERIAL, "Populating Pack 1 Data\r\n");
    	if( ! populate_pack_data(&max17205devPack1, &pack_1_data) ) {
    		pack_1_comm_rx_error_count++;
    	}

    	chprintf(DEBUG_SERIAL, "\r\nPopulating Pack 2 Data\r\n");
    	chThdSleepMilliseconds(100);
    	if( ! populate_pack_data(&max17205devPack2, &pack_2_data) ) {
    		pack_2_comm_rx_error_count++;
    	}

    	OD_battery.cell1 = pack_1_data.VCell_mV;
    	OD_battery.cell2 = pack_2_data.VCell_mV;
    	OD_battery.VCell = (pack_1_data.VCell_mV + pack_2_data.VCell_mV) / 2;
    	OD_battery.timeToEmpty = (pack_1_data.time_to_empty + pack_2_data.time_to_empty) / 2;
    	OD_battery.timeToFull = (pack_1_data.time_to_full + pack_2_data.time_to_full) / 2;
    	OD_battery.availableStateOfCharge = (pack_1_data.available_state_of_charge + pack_2_data.available_state_of_charge) / 2;
    	OD_battery.presentStateOfCharge = (pack_1_data.present_state_of_charge + pack_2_data.present_state_of_charge) / 2;

    	OD_battery.fullCapacity = (pack_1_data.full_capacity_mAh + pack_2_data.full_capacity_mAh);
    	OD_battery.availableCapacity = (pack_1_data.available_capacity_mAh + pack_2_data.available_capacity_mAh);
    	OD_battery.mixCapacity = (pack_1_data.mix_capacity + pack_2_data.mix_capacity);

    	//TODO publish pack_1_rx_error_count and pack_2_rx_error_count to OD_battery


        run_battery_heating_state_machine(&pack_1_data, &pack_2_data);
        update_battery_charging_state(&pack_1_data, LINE_DCHG_DIS_PK1, LINE_CHG_DIS_PK1, "PK1");
        update_battery_charging_state(&pack_2_data, LINE_DCHG_DIS_PK2, LINE_CHG_DIS_PK2, "PK2");

        palToggleLine(LINE_LED);
    }

    max17205Stop(&max17205devPack1);
    max17205Stop(&max17205devPack2);

    palClearLine(LINE_LED);
    chThdExit(MSG_OK);
}
