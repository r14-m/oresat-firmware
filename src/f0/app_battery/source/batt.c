#include "ch.h"
#include "batt.h"
#include "max17205.h"
#include "CANopen.h"
#include "chprintf.h"


#define ENABLE_NV_MEMORY_UPDATE_CODE      0

#if 0 || ENABLE_NV_MEMORY_UPDATE_CODE
#define DEBUG_SERIAL    (BaseSequentialStream*) &SD2
#include "chprintf.h"
#define dbgprintf(str, ...)       chprintf((BaseSequentialStream*) &SD2, str, ##__VA_ARGS__)
#else
#define dbgprintf(str, ...)
#endif


#define NCELLS          2U          /* Number of cells */

typedef enum {
	BATTERY_OD_ERROR_INFO_CODE_NONE = 0,
	BATTERY_OD_ERROR_INFO_CODE_PACK_1_COMM_ERROR,
	BATTERY_OD_ERROR_INFO_CODE_PACK_2_COMM_ERROR,
	BATTERY_OD_ERROR_INFO_CODE_PACK_FAIL_SAFE_HEATING,
	BATTERY_OD_ERROR_INFO_CODE_PACK_FAIL_SAFE_CHARGING,
} battery_od_error_info_code_t;

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

//The values for batt_nv_programing_cfg are detailed in the google document "MAX17205 Register Values"
static const max17205_regval_t batt_nv_programing_cfg[] = {
    {MAX17205_AD_NPACKCFG, MAX17205_SETVAL(MAX17205_AD_PACKCFG,
                                          _VAL2FLD(MAX17205_PACKCFG_NCELLS, NCELLS) |
                                          MAX17205_PACKCFG_BALCFG_40 |
										  MAX17205_PACKCFG_CHEN |
										  MAX17205_PACKCFG_TDEN |
										  MAX17205_PACKCFG_A1EN |
										  MAX17205_PACKCFG_A2EN )}, /* 0x3CA2 */
	{MAX17205_AD_NDESIGNCAP, 5200}, /*0x1450*/
	{MAX17205_AD_NNVCFG0, MAX17205_NNVCFG0_ENOCV |
							MAX17205_NNVCFG0_ENX |
							MAX17205_NNVCFG0_ENCFG |
							MAX17205_NNVCFG0_ENLCFG |
							MAX17205_NNVCFG0_ENDC },
	{MAX17205_AD_NNVCFG1, MAX17205_NNVCFG1_ENTTF | MAX17205_NNVCFG1_ENCTE},
	{MAX17205_AD_NNVCFG2, MAX17205_NNVCFG2_ENFC |
							(9 & MAX17205_NNVCFG2_CYCLESPSAVE_Msk)},
	{MAX17205_AD_NCONFIG, MAX17205_NCONFIG_TEN |
							(1<<4)},
    {0,0}
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
	{MAX17205_AD_CONFIG, MAX17205_CONFIG_TEN | MAX17205_CONFIG_ETHRM},
    {0,0}
};


static const MAX17205Config max17205configPack1 = {
	&I2CD1,
	&i2cconfig_1,
    batt_cfg
};

static const MAX17205Config max17205configPack2 = {
	&I2CD2,
	&i2cconfig_2,
    batt_cfg
};

static MAX17205Driver max17205devPack1;
static MAX17205Driver max17205devPack2;

typedef struct {
	bool is_data_valid;
	uint8_t pack_number;

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
	uint16_t reported_state_of_charge; //Percent

	uint16_t time_to_full; // seconds
	uint16_t time_to_empty; // seconds

	uint16_t full_capacity_mAh;
	uint16_t available_capacity_mAh;
	uint16_t mix_capacity;
	uint16_t reported_capacity_mAh;

	uint16_t cycles; // count

	int16_t avg_temp_1_C;
	int16_t avg_temp_2_C;
	int16_t avg_int_temp_C;
} batt_pack_data_t;

static batt_pack_data_t pack_1_data;
static batt_pack_data_t pack_2_data;

battery_heating_state_machine_state_t current_batery_state_machine_state = BATTERY_STATE_MACHINE_STATE_NOT_HEATING;

/**
 * TODO document this
 */
void run_battery_heating_state_machine(batt_pack_data_t *pk1_data, batt_pack_data_t *pk2_data) {
	if( pk1_data->is_data_valid && pk2_data->is_data_valid ) {
		const uint16_t total_state_of_charge = (pk1_data->present_state_of_charge + pk2_data->present_state_of_charge) / 2;

		switch (current_batery_state_machine_state) {
			case BATTERY_STATE_MACHINE_STATE_HEATING:
				dbgprintf("Turning heaters ON\r\n");
				palSetLine(LINE_MOARPWR);
				palSetLine(LINE_HEATER_ON_1);
				palSetLine(LINE_HEATER_ON_2);
				//Once they’re greater than 5 °C or the combined pack capacity is < 25%

				if( (pk1_data->avg_temp_1_C > 5 && pk2_data->avg_temp_1_C > 5) || (total_state_of_charge < 25) ) {
					current_batery_state_machine_state = BATTERY_STATE_MACHINE_STATE_NOT_HEATING;
				}
				break;
			case BATTERY_STATE_MACHINE_STATE_NOT_HEATING:
				dbgprintf("Turning heaters OFF\r\n");
				palClearLine(LINE_HEATER_ON_1);
				palClearLine(LINE_HEATER_ON_2);
				palClearLine(LINE_MOARPWR);

				if( (pk1_data->avg_temp_1_C < -5 || pk2_data->avg_temp_1_C < -5) && (pk1_data->present_state_of_charge > 25 || pk2_data->present_state_of_charge > 25) ) {
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

		CO_errorReport(CO->em, CO_EM_GENERIC_ERROR, CO_EMC_HARDWARE, BATTERY_OD_ERROR_INFO_CODE_PACK_FAIL_SAFE_HEATING);
	}
}

/**
 * TODO document this
 */
void update_battery_charging_state(batt_pack_data_t *pk_data, const ioline_t line_dchg_dis, const ioline_t line_chg_dis) {
	dbgprintf("LINE_DCHG_STAT_PK1 = %u\r\n", palReadLine(LINE_DCHG_STAT_PK1));
	dbgprintf("LINE_CHG_STAT_PK1 = %u\r\n", palReadLine(LINE_CHG_STAT_PK1));
	dbgprintf("LINE_DCHG_STAT_PK2 = %u\r\n", palReadLine(LINE_DCHG_STAT_PK2));
	dbgprintf("LINE_CHG_STAT_PK2 = %u\r\n", palReadLine(LINE_CHG_STAT_PK2));

	if( pk_data->is_data_valid ) {
		if( pk_data->VCell_mV < 3000 || pk_data->present_state_of_charge < 20 ) {
			//Disable discharge on both packs
			dbgprintf("Disabling discharge on pack %u\r\n", pk_data->pack_number);
			palSetLine(line_dchg_dis);
		} else {
			dbgprintf("Enabling discharge on pack %u\r\n", pk_data->pack_number);
			//Allow discharge on both packs
			palClearLine(line_dchg_dis);
		}


		if( pk_data->VCell_mV > 4100 ) {
			dbgprintf("Disabling charging on pack %u\r\n", pk_data->pack_number);
			palSetLine(line_chg_dis);
		} else {
			dbgprintf("Enabling charging on pack %u\r\n", pk_data->pack_number);
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

		CO_errorReport(CO->em, CO_EM_GENERIC_ERROR, CO_EMC_HARDWARE, BATTERY_OD_ERROR_INFO_CODE_PACK_FAIL_SAFE_CHARGING);
	}
}

/**
 * TODO document this
 */
bool populate_pack_data(MAX17205Driver *driver, batt_pack_data_t *dest) {
	msg_t r = 0;
	memset(dest, 0, sizeof(*dest));

	if( driver->state != MAX17205_READY ) {
		return(false);
	}

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

    dbgprintf("avg_temp_1_C = %d C, ", dest->avg_temp_1_C);
    dbgprintf("avg_temp_2_C = %d C, ", dest->avg_temp_2_C);
    dbgprintf("avg_int_temp_C = %d C", dest->avg_int_temp_C);
    dbgprintf("\r\n");

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

    dbgprintf("cell1_mV = %u, cell2_mV = %u, VCell_mV = %u, batt_mV = %u\r\n", dest->cell1_mV, dest->cell2_mV, dest->VCell_mV, dest->batt_mV);

    uint16_t max_min_volt_raw = 0;
    if( (r = max17205ReadRaw(driver, MAX17205_AD_MAXMINVOLT, &max_min_volt_raw)) != MSG_OK ) {
		dest->is_data_valid = false;
	} else {
		dest->VCell_max_volt_mV = (max_min_volt_raw >> 8) * 20;
		dest->VCell_min_volt_mV = (max_min_volt_raw & 0xFF) * 20;
		dbgprintf("VCell_max_volt_mV = %u, VCell_min_volt_mV = %u\r\n", dest->VCell_max_volt_mV, dest->VCell_min_volt_mV);
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

		dbgprintf("max_mA = %d, min_mA = %d\r\n", dest->max_current_mA, dest->min_current_mA);
	}

    dbgprintf("avg_current_mA = %d mA\r\n", dest->avg_current_mA);


    /* capacity */
	if( (r = max17205ReadCapacity(driver, MAX17205_AD_FULLCAPREP, &dest->full_capacity_mAh)) != MSG_OK ) {
		dest->is_data_valid = false;
    }
	if( (r = max17205ReadCapacity(driver, MAX17205_AD_AVCAP, &dest->available_capacity_mAh)) != MSG_OK ) {
		dest->is_data_valid = false;
	}
	if( (r = max17205ReadCapacity(driver, MAX17205_AD_MIXCAP, &dest->mix_capacity)) != MSG_OK ) {
		dest->is_data_valid = false;
	}
	if( (r = max17205ReadCapacity(driver, MAX17205_AD_REPCAP, &dest->reported_capacity_mAh)) != MSG_OK ) {
		dest->is_data_valid = false;
	}



    dbgprintf("full_capacity_mAh = %u, available_capacity_mAh = %u, mix_capacity = %u\r\n", dest->full_capacity_mAh, dest->available_capacity_mAh, dest->mix_capacity);



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
    if( (r = max17205ReadPercentage(driver, MAX17205_AD_REPSOC, &dest->reported_state_of_charge)) != MSG_OK ) {
		dest->is_data_valid = false;
	}


    dbgprintf("time_to_empty = %u (seconds), time_to_full = %u (seconds), available_state_of_charge = %u%%, present_state_of_charge = %u%%\r\n", dest->time_to_empty, dest->time_to_full, dest->available_state_of_charge, dest->present_state_of_charge);

    /* other info */
    if( (r = max17205ReadRaw(driver, MAX17205_AD_CYCLES, &dest->cycles)) != MSG_OK ) {
    	dest->is_data_valid = false;
	}

    dbgprintf("cycles = %u\r\n", dest->cycles);


    return(dest->is_data_valid);
}

/**
 * Helper function to trigger write of volatile memory on MAX71205 chip
 */
bool prompt_nv_memory_write(MAX17205Driver *devp, const MAX17205Config *config, const char *pack_str) {
	bool ret = false;
	dbgprintf("\r\n%s\r\n", pack_str);

	uint16_t masking_register = 0;
	uint8_t num_writes_left = 0;
	if( max17205ReadNVWriteCountMaskingRegister(config, &masking_register, &num_writes_left) == MSG_OK ) {
		dbgprintf("Memory Update Masking of register is 0x%X, num_writes_left = %u\r\n", masking_register, num_writes_left);
	}

	bool all_elements_match = true;
	dbgprintf("Current and expected NV settings:\r\n");
	for (int idx = 0; batt_nv_programing_cfg[idx].reg != 0; idx++) {
		uint16_t reg_value = 0;
		if( max17205ReadRaw(devp, batt_nv_programing_cfg[idx].reg, &reg_value) == MSG_OK ) {
			dbgprintf("   %-30s register 0x%X is 0x%X     expected  0x%X\r\n", max17205RegToStr(batt_nv_programing_cfg[idx].reg), batt_nv_programing_cfg[idx].reg, reg_value, batt_nv_programing_cfg[idx].value);
			if( reg_value != batt_nv_programing_cfg[idx].value ) {
				all_elements_match = false;
			}
		} else {
			dbgprintf("Failed to read reg value\r\n");
		}
	}

#if ENABLE_NV_MEMORY_UPDATE_CODE
	if( all_elements_match ) {
		dbgprintf("All NV Ram elements already match expected values...\r\n");
	} else {
		dbgprintf("One or more NV Ram elements don't match expected values...\r\n");
		bool write_reg_success_flag = true;
		for (int idx = 0; batt_nv_programing_cfg[idx].reg != 0; idx++) {
			if( max17205WriteRaw(devp, batt_nv_programing_cfg[idx].reg, batt_nv_programing_cfg[idx].value) == MSG_OK ) {
				dbgprintf("Successfully wrote reg value\r\n");
			} else {
				dbgprintf("Failed to write reg value\r\n");
				write_reg_success_flag = false;
			}
		}

		if( write_reg_success_flag ) {
			dbgprintf("Current and expected NV settings:\r\n");
			for (int idx = 0; batt_nv_programing_cfg[idx].reg != 0; idx++) {
				uint16_t reg_value = 0;
				if( max17205ReadRaw(devp, batt_nv_programing_cfg[idx].reg, &reg_value) == MSG_OK ) {
					dbgprintf("   %-30s register 0x%X is 0x%X     expected  0x%X\r\n", max17205RegToStr(batt_nv_programing_cfg[idx].reg), batt_nv_programing_cfg[idx].reg, reg_value, batt_nv_programing_cfg[idx].value);
				} else {
					dbgprintf("Failed to read reg value\r\n");
				}
			}


			dbgprintf("Write NV memory on MAX17205 for %s ? y/n? ", pack_str);
			uint8_t ch = 0;
			sdRead(&SD2, &ch, 1);
			dbgprintf("\r\n");

			if (ch == 'y') {
				ret = true;

				dbgprintf("Attempting to write non volatile memory on MAX17205...\r\n");
				chThdSleepMilliseconds(50);

				if (max17205NonvolatileBlockProgram(config) == MSG_OK ) {
					dbgprintf("Successfully wrote non volatile memory on MAX17205...\r\n");
				} else {
					dbgprintf("Failed to write non volatile memory on MAX17205...\r\n");
				}
			}
		}
	}
#endif

	return(ret);
}

/* Battery monitoring thread */
THD_WORKING_AREA(batt_wa, 0x400);
THD_FUNCTION(batt, arg)
{
    (void)arg;

    max17205ObjectInit(&max17205devPack1);
    max17205ObjectInit(&max17205devPack2);
    const bool pack_1_init_flag = max17205Start(&max17205devPack1, &max17205configPack1);
    dbgprintf("max17205Start(pack1) = %u\r\n", pack_1_init_flag);

#if 0
    max17205PrintintNonvolatileMemory(&max17205configPack1);
    while(1) {
    	chThdSleepMilliseconds(1000);
    }
#endif


    const bool pack_2_init_flag = max17205Start(&max17205devPack2, &max17205configPack2);
    dbgprintf("max17205Start(pack2) = %u\r\n", pack_2_init_flag);


#if 1
    prompt_nv_memory_write(&max17205devPack1, &max17205configPack1, "Pack 1");
    prompt_nv_memory_write(&max17205devPack2, &max17205configPack2, "Pack 2");
#if ENABLE_NV_MEMORY_UPDATE_CODE
    dbgprintf("Done with NV RAM update code, disable ENABLE_NV_MEMORY_UPDATE_CODE and re-write firmware.\r\n");
	for (;;) {
		dbgprintf(".");
		chThdSleepMilliseconds(1000);
	}
#endif
#endif


    uint16_t pack_1_comm_rx_error_count = 0;
    uint16_t pack_2_comm_rx_error_count = 0;
    while (!chThdShouldTerminateX()) {
    	dbgprintf("================================= %u ms\r\n", TIME_I2MS(chVTGetSystemTime()));

    	dbgprintf("Populating Pack 1 Data\r\n");
    	if( ! populate_pack_data(&max17205devPack1, &pack_1_data) ) {
    		pack_1_comm_rx_error_count++;
            CO_errorReport(CO->em, CO_EM_GENERIC_ERROR, CO_EMC_COMMUNICATION, BATTERY_OD_ERROR_INFO_CODE_PACK_1_COMM_ERROR);
    	}
    	pack_1_data.pack_number = 1;

    	dbgprintf("\r\nPopulating Pack 2 Data\r\n");
    	chThdSleepMilliseconds(100);
    	if( ! populate_pack_data(&max17205devPack2, &pack_2_data) ) {
    		pack_2_comm_rx_error_count++;
    		CO_errorReport(CO->em, CO_EM_GENERIC_ERROR, CO_EMC_COMMUNICATION, BATTERY_OD_ERROR_INFO_CODE_PACK_2_COMM_ERROR);
    	}
    	pack_2_data.pack_number = 2;


		OD_battery1.vbatt = pack_1_data.batt_mV;
		OD_battery1.VCellMax = pack_1_data.VCell_max_volt_mV;
		OD_battery1.VCellMin = pack_1_data.VCell_min_volt_mV;
		OD_battery1.VCell = pack_1_data.cell1_mV;
		OD_battery1.VCell2 = pack_1_data.cell2_mV;
		OD_battery1.currentAvg = pack_1_data.avg_current_mA;
		OD_battery1.currentMax = pack_1_data.max_current_mA;
		OD_battery1.currentMin = pack_1_data.min_current_mA;
		OD_battery1.fullCapacity = pack_1_data.full_capacity_mAh;
		OD_battery1.timeToEmpty = pack_1_data.time_to_empty;
		OD_battery1.timeToFull = pack_1_data.time_to_full;
		OD_battery1.cycles = pack_1_data.cycles;
		OD_battery1.reportedStateOfCharge = pack_1_data.reported_state_of_charge;
		OD_battery1.reportedCapacity = pack_1_data.reported_capacity_mAh;
		OD_battery1.tempAvg1 = pack_1_data.avg_temp_1_C;
		OD_battery1.tempAvg2 = pack_1_data.avg_temp_2_C;
		OD_battery1.tempAvgInt = pack_1_data.avg_int_temp_C;
		OD_battery1.dischargeDisable = palReadLine(LINE_DCHG_DIS_PK1);
		OD_battery1.chargeDisable = palReadLine(LINE_CHG_DIS_PK1);
		OD_battery1.dischargeStatus = palReadLine(LINE_DCHG_STAT_PK1);
		OD_battery1.chargeStatus = palReadLine(LINE_CHG_STAT_PK1);


		OD_battery2.vbatt = pack_1_data.batt_mV;
		OD_battery2.VCellMax = pack_1_data.VCell_max_volt_mV;
		OD_battery2.VCellMin = pack_1_data.VCell_min_volt_mV;
		OD_battery2.VCell = pack_1_data.cell1_mV;
		OD_battery2.VCell2 = pack_1_data.cell2_mV;
		OD_battery2.currentAvg = pack_1_data.avg_current_mA;
		OD_battery2.currentMax = pack_1_data.max_current_mA;
		OD_battery2.currentMin = pack_1_data.min_current_mA;
		OD_battery2.fullCapacity = pack_1_data.full_capacity_mAh;
		OD_battery2.timeToEmpty = pack_1_data.time_to_empty;
		OD_battery2.timeToFull = pack_1_data.time_to_full;
		OD_battery2.cycles = pack_1_data.cycles;
		OD_battery2.reportedStateOfCharge = pack_1_data.reported_state_of_charge;
		OD_battery2.reportedCapacity = pack_1_data.reported_capacity_mAh;
		OD_battery2.tempAvg1 = pack_1_data.avg_temp_1_C;
		OD_battery2.tempAvg2 = pack_1_data.avg_temp_2_C;
		OD_battery2.tempAvgInt = pack_1_data.avg_int_temp_C;
		OD_battery2.dischargeDisable = palReadLine(LINE_DCHG_DIS_PK2);
		OD_battery2.chargeDisable = palReadLine(LINE_CHG_DIS_PK2);
		OD_battery2.dischargeStatus = palReadLine(LINE_DCHG_STAT_PK2);
		OD_battery2.chargeStatus = palReadLine(LINE_CHG_STAT_PK2);

		CO_OD_RAM.heaterStatus = palReadLine(LINE_MOARPWR);

        run_battery_heating_state_machine(&pack_1_data, &pack_2_data);
        update_battery_charging_state(&pack_1_data, LINE_DCHG_DIS_PK1, LINE_CHG_DIS_PK1);
        update_battery_charging_state(&pack_2_data, LINE_DCHG_DIS_PK2, LINE_CHG_DIS_PK2);

        palToggleLine(LINE_LED);

        chThdSleepMilliseconds(1000);
    }

    dbgprintf("Terminating battery thread...\r\n");

    max17205Stop(&max17205devPack1);
    max17205Stop(&max17205devPack2);

    palClearLine(LINE_LED);
    chThdExit(MSG_OK);
}
