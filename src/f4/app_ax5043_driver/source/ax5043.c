/*! \file ax5043.h */


/*!
 * \addtogroup ax5043
 *  * @{
 *
 */
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "ax5043.h"


#define DEBUG_SERIAL  SD2
#define DEBUG_CHP     ((BaseSequentialStream *) &DEBUG_SERIAL)

#include <stdbool.h>
#include <stdint.h>


// physical layer from config.c file generated from radiolab
const uint8_t axradio_phy_pn9 = 0;
const uint8_t axradio_phy_nrchannels = 1;
const uint32_t axradio_phy_chanfreq[1] = { 0x0912aaab };
const uint8_t axradio_phy_chanpllrnginit[1] = { 0x0a };
const uint8_t axradio_phy_chanvcoiinit[1] = { 0x98 };
uint8_t axradio_phy_chanpllrng[1];
uint8_t axradio_phy_chanvcoi[1];
const uint8_t axradio_phy_vcocalib = 0;
const int32_t axradio_phy_maxfreqoffset = 913;
const int8_t axradio_phy_rssioffset = 64;
// axradio_phy_rssioffset is added to AX5043_RSSIREFERENCE and subtracted from chip RSSI value to prevent overflows (8bit RSSI only goes down to -128)
// axradio_phy_rssioffset is also added to AX5043_RSSIABSTHR
const int8_t axradio_phy_rssireference = (int8_t)(0xF9 + 64);
const int8_t axradio_phy_channelbusy = -87 + 64;
const uint16_t axradio_phy_cs_period = 7; // timer0 units, 10ms
const uint8_t axradio_phy_cs_enabled = 0;
const uint8_t axradio_phy_lbt_retries = 0;
const uint8_t axradio_phy_lbt_forcetx = 0;
const uint16_t axradio_phy_preamble_wor_longlen = 23; // wor_longlen + wor_len totals to 240.0ms plus 112bits
const uint16_t axradio_phy_preamble_wor_len = 184;
const uint16_t axradio_phy_preamble_longlen = 0;
const uint16_t axradio_phy_preamble_len = 72;
const uint8_t axradio_phy_preamble_byte = 0x7e;
const uint8_t axradio_phy_preamble_flags = 0x38;
const uint8_t axradio_phy_preamble_appendbits = 0;
const uint8_t axradio_phy_preamble_appendpattern = 0x00;

//framing variables generated from radiolab
const uint8_t axradio_framing_maclen = 3;
const uint8_t axradio_framing_addrlen = 1;
const uint8_t axradio_framing_destaddrpos = 0;
const uint8_t axradio_framing_sourceaddrpos = 0xff;
const uint8_t axradio_framing_lenpos = 2;
const uint8_t axradio_framing_lenoffs = 0;
const uint8_t axradio_framing_lenmask = 0xff;
const uint8_t axradio_framing_swcrclen = 0;

const uint8_t axradio_framing_synclen = 32;
const uint8_t axradio_framing_syncword[] = { 0xcc, 0xaa, 0xcc, 0xaa};
const uint8_t axradio_framing_syncflags = 0x38;
const uint8_t axradio_framing_enable_sfdcallback = 0;

const uint32_t axradio_framing_ack_timeout = 8; // 98.9ms in wtimer0 units (640Hz)
const uint32_t axradio_framing_ack_delay = 313; // 1.0ms in wtimer1 units (20MHz/64)
const uint8_t axradio_framing_ack_retransmissions = 0;
const uint8_t axradio_framing_ack_seqnrpos = 0xff;

const uint8_t axradio_framing_minpayloadlen = 0; // must be set to 1 if the payload directly follows the destination address, and a CRC is configured
//WOR
const uint16_t axradio_wor_period = 128;
const uint8_t axradio_phy_innerfreqloop = 0;


/**
 * writes  to an AX5043 register.
 * @param spip: SPI Configuration, reg: Register address, value: register value, ret_value: returned data.
 * @return the value of the register.
 */
uint8_t ax5043_write_reg(SPIDriver * spip, uint16_t reg, uint8_t value, uint8_t ret_value[])
{
  uint8_t command_buf[3] = {0,0,0};
  
  if(reg <  0x0070)
  {
    command_buf[0]=0x80|reg;
    command_buf[1]=value;
    spiSelect(spip);
    spiStartExchange(spip, 2, command_buf, ret_value);
    while((*spip).state != SPI_READY) { }
    spiUnselect(spip);
  }
  else
  {
    command_buf[0]=0xF0|(reg>>8);
    command_buf[1]=reg;
    command_buf[2]=value;
    spiSelect(spip);
    spiStartExchange(spip, 3, command_buf, ret_value);
    while((*spip).state != SPI_READY) { }
    spiUnselect(spip);
  }
  return ret_value[0];   //retun status while writting the register

}


/**
 * Reds an AX5043 register. This has retry logic. This calls the the function ax5043_write_reg_spi.
 * @param spip: SPI Configuration, reg: Register address, value: register value, ret_value: returned data.
 * @return the value in the register.
 */
uint8_t ax5043_read_reg(SPIDriver * spip, uint16_t reg, uint8_t value, uint8_t ret_value[])
{
  uint8_t command_buf[3] = {0,0,0 };
  //uint8_t rx_buf[2]={0 ,0 };

  if(reg <  0x0070)
  {
    command_buf[0]=reg;
    command_buf[1]=value;
    spiSelect(spip);
    spiStartExchange(spip, 2, command_buf, ret_value);
    while((*spip).state != SPI_READY) { }
    spiUnselect(spip);
    return ret_value[1];    //return the reg value when reading the register
  }
  else
  {
    command_buf[0]=0x70|(reg>>8);
    command_buf[1]=reg;
    command_buf[2]=value;
    spiSelect(spip);
    spiStartExchange(spip, 3, command_buf, ret_value);
    while((*spip).state != SPI_READY) { }
    spiUnselect(spip);
    return ret_value[2];    //return the reg value when reading the register
  }

}


/**
 * Sets powermode register of AX5043
 * @param spip: SPI Configuration, reg_value:register value
 * @return void 
 * TODO return a -ve return code if there are any errors
 */
void ax5043_set_pwrmode(SPIDriver * spip, uint8_t reg_value)
{
  uint8_t ret_value[3]={0,0,0};
  uint8_t value;
  value = ax5043_read_reg(spip, AX5043_REG_PWRMODE, (uint8_t)0x00, ret_value);
  value = value & 0xF0;
  value = value | reg_value;
  ax5043_write_reg(spip, AX5043_REG_PWRMODE, value, ret_value);
}


/**
 * sets AX5043 address registers
 * @param spip: SPI Configuration.
 * @return void 
 * TODO return a -ve return code if there are any errors
 */
void ax5043_set_addr(SPIDriver * spip, const struct axradio_address_mask local_addr)
{
  uint8_t ret_value[3]={0,0,0};
  //set address values
  ax5043_write_reg(spip, AX5043_REG_PKTADDR0, (uint8_t)local_addr.addr[0], ret_value);
  ax5043_write_reg(spip, AX5043_REG_PKTADDR1, (uint8_t)local_addr.addr[1], ret_value);
  ax5043_write_reg(spip, AX5043_REG_PKTADDR2, (uint8_t)local_addr.addr[2], ret_value);
  ax5043_write_reg(spip, AX5043_REG_PKTADDR3, (uint8_t)local_addr.addr[3], ret_value);
  //set address mask
  ax5043_write_reg(spip, AX5043_REG_PKTADDRMASK0, (uint8_t)local_addr.mask[0], ret_value);
  ax5043_write_reg(spip, AX5043_REG_PKTADDRMASK1, (uint8_t)local_addr.mask[1], ret_value);
  ax5043_write_reg(spip, AX5043_REG_PKTADDRMASK2, (uint8_t)local_addr.mask[2], ret_value);
  ax5043_write_reg(spip, AX5043_REG_PKTADDRMASK3, (uint8_t)local_addr.mask[3], ret_value);
}

/**
 * Resets the AX5043
 * @param spip: SPI Configuration.
 * @return void 
 * TODO return a -ve return code if there are any errors
 */
void ax5043_reset(SPIDriver * spip)
{

  //uint8_t reg=0;
  uint8_t value = 0;
  uint8_t ret_value[3]={0,0,0};

  spiSelect(spip);
  chThdSleepMicroseconds(5);
  spiUnselect(spip);
  chThdSleepMicroseconds(5);
  spiSelect(spip);
  chThdSleepMicroseconds(5);
  //spiUnselect(spip);
  //chThdSleepMicroseconds(10);

  //Reset the chip through powermode register 
  ax5043_write_reg(spip, AX5043_REG_PWRMODE, AX5043_RESET_BIT, ret_value);
  chThdSleepMilliseconds(1);
  //chThdSleepMicroseconds(5);

  //read the powermode register
  value=ax5043_read_reg(&SPID2, AX5043_REG_PWRMODE, value, ret_value);
  //write to powermode register for enabling XOEN and REFIN and shutdown mode
  //page 33 in programming manual
  //value = value | AX5043_OSC_EN_BIT | AX5043_REF_EN_BIT;
  value = AX5043_OSC_EN_BIT | AX5043_REF_EN_BIT | AX5043_POWERDOWN;
  ax5043_write_reg(spip, AX5043_REG_PWRMODE, value, ret_value);

  //ax5043_write_reg(spip, AX5043_REG_MODULATION, (uint8_t)0x00, ret_value);
  ax5043_write_reg(spip, AX5043_REG_SCRATCH, (uint8_t)0xAA, ret_value);
  value = ax5043_read_reg(&SPID2, AX5043_REG_SCRATCH, (uint8_t)0x00, ret_value);
  if (value != 0xAA)
  {
        chprintf(DEBUG_CHP, "Scratch register does not match 0\r\n");
  }
  ax5043_write_reg(spip, AX5043_REG_SCRATCH, (uint8_t)0x55, ret_value);
  value = ax5043_read_reg(spip, AX5043_REG_SCRATCH, (uint8_t)0x00, ret_value);
  if (value != 0x55)
  {
        chprintf(DEBUG_CHP, "Scratch register does not match 1\r\n");
  }
  //ax5043_write_reg(spip, AX5043_REG_PINFUNCIRQ, (uint8_t)0x02, ret_value);
  //chThdSleepMilliseconds(10);
  

}





/**
 * writes to FIFO
 * Takn from Bradenburg library which seems to be taken from onSemi's code
 * @param conf the AX5043 configuration handler
 * @return void 
 * 
 */
void ax5043_writefifo(SPIDriver * spip,const uint8_t *ptr, uint8_t len)
{
    uint8_t ret_value[3]={0,0,0};
	if (!len)
		return;
	do {
		ax5043_write_reg(spip, AX5043_REG_FIFODATA, *ptr++, ret_value);
	} while (--len);
}

 

uint8_t ax5043_readfifo(SPIDriver * spip, uint8_t axradio_rxbuffer[], uint8_t len) 
{
  uint8_t ret_value[3]={0,0,0};
  uint8_t loc = 0;
  while (len--) {
    axradio_rxbuffer[loc] = ax5043_read_reg(spip, AX5043_REG_FIFODATA, (uint8_t)0x00, ret_value);
    //chprintf(DEBUG_CHP,"Packet: %x \r\n", axradio_rxbuffer[loc]);
    loc++;
  }
  return loc;
}


/**
 * Sets AX5043 registers. This is based on config file generated by radio lab.
 * @param spip: SPI Configuration.
 * @return void 
 * TODO return a -ve return code if there are any errors
 */
// TX: fcarrier=435.500MHz dev= 12.500kHz br= 50.000kBit/s pwr= 10.0dBm
// RX: fcarrier=435.500MHz bw= 75.000kHz br= 50.000kBit/s

void ax5043_set_regs_group(AX5043Driver *devp, ax5043_reg_group_t group) {
    uint8_t rxbuf[3] = {0, 0, 0};
    int i = 0;
    ax5043_regval_t* entry = devp->config->reg_values;
    while (entry[i].reg != AX5043_REG_END) {
      if (entry[i].group == group){
        ax5043_write_reg(devp->config->spip, entry[i].reg, entry[i].val, rxbuf);
      }
      i++;
    }
}





/**
 * Sets AX5043 registers for TX. This is based on config file generated by radio lab.
 * @param spip: SPI Configuration.
 * @return void 
 * TODO return a -ve return code if there are any errors
 */
void ax5043_set_regs_rxcont_singleparamset(SPIDriver * spip)
{
  uint8_t ret_value[3]={0,0,0};
  ax5043_write_reg(spip, AX5043_REG_RXPARAMSETS,    (uint8_t)0xFF, ret_value); 
  ax5043_write_reg(spip, AX5043_REG_FREQDEV13,      (uint8_t)0x00, ret_value);
  ax5043_write_reg(spip, AX5043_REG_FREQDEV03,      (uint8_t)0x00, ret_value);
  ax5043_write_reg(spip, AX5043_REG_AGCGAIN3,       (uint8_t)0xB5, ret_value);
}







/**
 * prepare AX5043 for tx
 * @param spip: SPI Configuration.
 * @return void 
 * TODO return a -ve return code if there are any errors
 */
void ax5043_prepare_tx(AX5043Driver *devp)
{

  //uint8_t reg=0;
  //uint8_t value=0;
  uint8_t ret_value[3]={0,0,0};
  SPIDriver *spip = devp->config->spip;

  //ax5043_standby(spip);
  ax5043_set_pwrmode(spip, AX5043_STANDBY);
  //ax5043_fifo_en(spip);
  ax5043_set_pwrmode(spip, AX5043_FIFO_ENABLED);
  //ax5043_set_regs_tx(spip);
  ax5043_set_regs_group(devp,tx);
  ax5043_init_registers_common(spip);

  ax5043_write_reg(spip, AX5043_REG_FIFOTHRESH1, (uint8_t)0x00, ret_value);
  ax5043_write_reg(spip, AX5043_REG_FIFOTHRESH0, (uint8_t)0x80, ret_value);
  ax5043_write_reg(spip, AX5043_REG_IRQMASK0, (uint8_t)0x00, ret_value);
  ax5043_write_reg(spip, AX5043_REG_IRQMASK1, (uint8_t)0x01, ret_value);


  //set address values
  //ax5043_set_local_addr(spip);

  //wait for xtal
  while ((ax5043_read_reg(spip, AX5043_REG_XTALSTATUS, (uint8_t)0x00, ret_value) & 0x01) == 0) {
    chThdSleepMilliseconds(1);
  }

  //ax5043_full_tx(spip);
}


/**
 * prepare AX5043 for rx
 * @param spip: SPI Configuration.
 * @return void 
 * TODO return a -ve return code if there are any errors
 */
void ax5043_prepare_rx(AX5043Driver *devp)
{

  //uint8_t reg=0;
  //int8_t value= (int8_t)(0xF9 + 64);
  uint8_t ret_value[3]={0,0,0};
  SPIDriver *spip = devp->config->spip;

  //ax5043_write_reg(spip, AX5043_REG_PWRMODE, AX5043_STANDBY, ret_value);
  //ax5043_write_reg(spip, AX5043_REG_LPOSCCONFIG, (uint8_t)0x00, ret_value);
  //ax5043_write_reg(spip, AX5043_REG_PWRMODE, AX5043_POWERDOWN, ret_value);

  //ax5043_set_regs_rx(spip);
  ax5043_set_regs_group(devp,rx);
  ax5043_init_registers_common(spip);

  
  //follows code from function ax5043_receiver_on_continuous() in easyax5043.c from radiolab

  ax5043_write_reg(spip, AX5043_REG_RSSIREFERENCE, axradio_phy_rssireference, ret_value);
  //ax5043_set_regs_rxcont(spip);
  ax5043_set_regs_group(devp,rx_cont);
  
  ax5043_write_reg(spip, AX5043_REG_FIFOSTAT, (uint8_t)0x03, ret_value);//FIFO reset
  //ax5043_write_reg(spip, AX5043_REG_PWRMODE, AX5043_FULL_RX, ret_value);//Full RX
  ax5043_set_pwrmode(spip, AX5043_FULL_RX);

  ax5043_write_reg(spip, AX5043_REG_FIFOTHRESH1, (uint8_t)0x00, ret_value);
  ax5043_write_reg(spip, AX5043_REG_FIFOTHRESH0, (uint8_t)0x80, ret_value);
  ax5043_write_reg(spip, AX5043_REG_IRQMASK0, (uint8_t)0x01, ret_value);
  ax5043_write_reg(spip, AX5043_REG_IRQMASK1, (uint8_t)0x00, ret_value);
  
  //set address values
  //ax5043_set_local_addr(spip);

}




 

uint8_t axradio_get_pllvcoi(SPIDriver * spip)
{
    uint8_t ret_value[3]={0,0,0};
    uint8_t x = axradio_phy_chanvcoiinit[0];
    if (x & 0x80) {
      if (!(axradio_phy_chanpllrnginit[0] & 0xF0)) {
        x += (axradio_phy_chanpllrng[0] & 0x0F) - (axradio_phy_chanpllrnginit[0] & 0x0F);
       x &= 0x3f;
        x |= 0x80;
      }
      return x;
    }
    return ax5043_read_reg(spip, AX5043_REG_PLLVCOI, (uint8_t)0x00, ret_value);
}




void ax5043_init_registers_common(SPIDriver * spip)
{
    uint8_t ret_value[3]={0,0,0};
    uint8_t rng = axradio_phy_chanpllrng[0];
    if (rng & 0x20)
        chprintf(DEBUG_CHP, "\r\r ERROR at ax5043_init_registers_common --\r\n");
    if ( ax5043_read_reg(spip, AX5043_REG_PLLLOOP, (uint8_t)0x00, ret_value) & 0x80) {
        ax5043_write_reg(spip, AX5043_REG_PLLRANGINGB, (uint8_t)(rng & 0x0F), ret_value);
    } else {
       ax5043_write_reg(spip, AX5043_REG_PLLRANGINGA, (uint8_t)(rng & 0x0F), ret_value);
    }
    rng = axradio_get_pllvcoi(spip);
   if (rng & 0x80)
        ax5043_write_reg(spip, AX5043_REG_PLLVCOI, (uint8_t)(rng), ret_value);
}




/**
 * Initializes AX5043. This is written based on easyax5043.c generated from AX Radiolab
 * @param conf the AX5043 configuration handler
 * @return void 
 * TODO return a -ve return code if there are any errors
 */
void ax5043_init(AX5043Driver *devp)
{

  SPIDriver *spip = devp->config->spip;
  uint8_t ret_value[3]={0,0,0};

  ax5043_reset(spip);         


  //ax5043_shutdown(spip);
  ax5043_set_regs_group(devp,common);
  //ax5043_set_regs(spip);
  //ax5043_set_regs_tx(spip);
  ax5043_set_regs_group(devp,tx);

  ax5043_write_reg(spip, AX5043_REG_PLLLOOP, (uint8_t)0x09, ret_value);
  ax5043_write_reg(spip, AX5043_REG_PLLCPI, (uint8_t)0x08, ret_value);

  //ax5043_standby(spip);
  ax5043_set_pwrmode(spip, AX5043_STANDBY);
  ax5043_write_reg(spip, AX5043_REG_MODULATION, (uint8_t)0x08, ret_value);
  ax5043_write_reg(spip, AX5043_REG_FSKDEV2, (uint8_t)0x00, ret_value);
  ax5043_write_reg(spip, AX5043_REG_FSKDEV1, (uint8_t)0x00, ret_value);
  ax5043_write_reg(spip, AX5043_REG_FSKDEV0, (uint8_t)0x00, ret_value);

  //wait for Xtal
  while ((ax5043_read_reg(spip, AX5043_REG_XTALSTATUS, (uint8_t)0x00, ret_value) & 0x01) == 0)
  {
    chThdSleepMilliseconds(1);
  }
  chprintf(DEBUG_CHP, "XTAL Status 0x%x \r\n", ax5043_read_reg(spip, AX5043_REG_XTALSTATUS, (uint8_t)0x00, ret_value));
  
  //set frequency based on line 693 on conig.c and 1640 on easyax5043.c from 
  //codeblocks generated code
  uint32_t f = axradio_phy_chanfreq[0];
  ax5043_write_reg(spip, AX5043_REG_FREQA0, (uint8_t)f, ret_value);  
  ax5043_write_reg(spip, AX5043_REG_FREQA1, (uint8_t)(f >> 8), ret_value); 
  ax5043_write_reg(spip, AX5043_REG_FREQA2, (uint8_t)(f >> 16), ret_value); 
  ax5043_write_reg(spip, AX5043_REG_FREQA3, (uint8_t)(f >> 24), ret_value); 



  //PLL autoranging
  uint8_t r;
  if( !(axradio_phy_chanpllrnginit[0] & 0xF0) ) { // start values for ranging available
    r = axradio_phy_chanpllrnginit[0] | 0x10;
  }
  else {
    r = 0x18;
  }
  ax5043_write_reg(spip, AX5043_REG_PLLRANGINGA, (uint8_t)r, ret_value); 
  chThdSleepMilliseconds(1);
  while ((ax5043_read_reg(spip, AX5043_REG_PLLRANGINGA, (uint8_t)0x00, ret_value) & 0x10) != 0)
  {
    chThdSleepMilliseconds(1);
  }
  axradio_phy_chanpllrng[0] = ax5043_read_reg(spip, AX5043_REG_PLLRANGINGA, (uint8_t)0x00, ret_value); 
  chprintf(DEBUG_CHP, "\r\r PLL ranging done. 0x%x --\r\n", axradio_phy_chanpllrng[0]);


  //VCOI calibration
  /*
  //ax5043_set_regs_tx(spip);
  ax5043_set_regs_group(devp,tx);
  ax5043_write_reg(spip, AX5043_REG_MODULATION, (uint8_t)0x08, ret_value);
  ax5043_write_reg(spip, AX5043_REG_FSKDEV2, (uint8_t)0x00, ret_value);
  ax5043_write_reg(spip, AX5043_REG_FSKDEV1, (uint8_t)0x00, ret_value);
  ax5043_write_reg(spip, AX5043_REG_FSKDEV0, (uint8_t)0x00, ret_value);
  value=ax5043_read_reg(spip, AX5043_REG_PLLLOOP, (uint8_t)0x00, ret_value);
  value = value | 0x04;
  ax5043_write_reg(spip, AX5043_REG_PLLLOOP, value, ret_value);
  value=ax5043_read_reg(spip, AX5043_REG_0xF35, (uint8_t)0x00, ret_value);
  value = value | 0x80;
  if (2 & (uint8_t)~value)
    ++value;
  ax5043_write_reg(spip, AX5043_REG_0xF35, value, ret_value);
  //ax5043_synth_tx(spip);
  ax5043_set_pwrmode(spip, AX5043_SYNTH_TX);
  vcoi_save = ax5043_read_reg(spip, AX5043_REG_PLLVCOI, (uint8_t)0x00, ret_value);
  ax5043_write_reg(spip, AX5043_REG_PLLRANGINGA, (uint8_t)(pll_range_after & 0x0F), ret_value); 
  ax5043_write_reg(spip, AX5043_REG_FREQA0, (uint8_t)0xab, ret_value);  
  ax5043_write_reg(spip, AX5043_REG_FREQA1, (uint8_t)0xaa, ret_value); 
  ax5043_write_reg(spip, AX5043_REG_FREQA2, (uint8_t)0x12, ret_value); 
  ax5043_write_reg(spip, AX5043_REG_FREQA3, (uint8_t)0x09, ret_value); 
  //tune voltage. doesn't make sense. So, not implementing it for now..
  
  num_retries = 64;
  while (num_retries > 0)
  {
    ax5043_write_reg(spip, AX5043_REG_GPADCCTRL, (uint8_t)0x84, ret_value); 
    do{} while (ax5043_read_reg(spip, AX5043_REG_GPADCCTRL, (uint8_t)0x00, ret_value) & 0x80);
    num_retries--;
  }
  num_retries = 32;
  while (num_retries > 0)
  {
    ax5043_write_reg(spip, AX5043_REG_GPADCCTRL, (uint8_t)0x84, ret_value); 
    do{} while (ax5043_read_reg(spip, AX5043_REG_GPADCCTRL, (uint8_t)0x00, ret_value) & 0x80);
    num_retries--;
  }
  //ax5043_write_reg(spip, AX5043_REG_PLLVCOI, vcoi_save, ret_value);
  */

  //ax5043_shutdown(spip);
  ax5043_set_pwrmode(spip, AX5043_POWERDOWN);
  //ax5043_set_regs(spip);
  ax5043_set_regs_group(devp,common);
  //ax5043_set_regs_rx(spip);
  ax5043_set_regs_group(devp,rx);

  ax5043_read_reg(spip, AX5043_REG_PLLRANGINGA, (uint8_t)(axradio_phy_chanpllrng[0] & 0x0F), ret_value);
  f = axradio_phy_chanfreq[0];
  ax5043_write_reg(spip, AX5043_REG_FREQA0, (uint8_t)f, ret_value);  
  ax5043_write_reg(spip, AX5043_REG_FREQA1, (uint8_t)(f >> 8), ret_value); 
  ax5043_write_reg(spip, AX5043_REG_FREQA2, (uint8_t)(f >> 16), ret_value); 
  ax5043_write_reg(spip, AX5043_REG_FREQA3, (uint8_t)(f >> 24), ret_value); 


}



/**
 * Transmits a packet
 * The logic is similar to transmit_isr()
 * @param conf the AX5043 configuration handler
 * @return void 
 * 
 */

void transmit_loop(SPIDriver * spip, ax5043_trxstate_t axradio_trxstate, uint16_t axradio_txbuffer_len,uint8_t axradio_txbuffer[], uint16_t axradio_txbuffer_cnt)
{
    uint8_t ret_value[3]={0,0,0};

    for (;;) {
        uint8_t cnt = ax5043_read_reg(spip,AX5043_REG_FIFOFREE0, (uint8_t)0x00, ret_value);

        if (ax5043_read_reg(spip,AX5043_REG_FIFOFREE1, (uint8_t)0x00, ret_value))
            cnt = 0xff;

        switch (axradio_trxstate) {
        case trxstate_tx_longpreamble:
            if (!axradio_txbuffer_cnt) {
                axradio_trxstate = trxstate_tx_shortpreamble;
                axradio_txbuffer_cnt = axradio_phy_preamble_len;
                goto shortpreamble;
            }
            if (cnt < 4) {
                ax5043_write_reg(spip,AX5043_REG_FIFOSTAT, 4, ret_value); // commit
                chThdSleepMilliseconds(1);
                continue;
            }
            cnt = 7;
            if (axradio_txbuffer_cnt < 7)
                cnt = axradio_txbuffer_cnt;
            axradio_txbuffer_cnt -= cnt;
            cnt <<= 5;
            ax5043_write_reg(spip,AX5043_REG_FIFODATA, AX5043_FIFOCMD_REPEATDATA | (3 << 5), ret_value);
            ax5043_write_reg(spip,AX5043_REG_FIFODATA, axradio_phy_preamble_flags, ret_value);
            ax5043_write_reg(spip,AX5043_REG_FIFODATA, cnt, ret_value);
            ax5043_write_reg(spip,AX5043_REG_FIFODATA, axradio_phy_preamble_byte, ret_value);
            break;

        case trxstate_tx_shortpreamble:
        shortpreamble:
            if (!axradio_txbuffer_cnt) {
                if (cnt < 15) {
                    ax5043_write_reg(spip,AX5043_REG_FIFOSTAT, 4, ret_value); // commit
                    chThdSleepMilliseconds(1);
                    continue;
                }
                if (axradio_phy_preamble_appendbits) {
                    uint8_t byte;
                    ax5043_write_reg(spip,AX5043_REG_FIFODATA, AX5043_FIFOCMD_DATA | (2 << 5), ret_value);
                    ax5043_write_reg(spip,AX5043_REG_FIFODATA, 0x1C, ret_value);
                    byte = axradio_phy_preamble_appendpattern;
                    if (ax5043_read_reg(spip,AX5043_REG_PKTADDRCFG, (uint8_t)0x00, ret_value) & 0x80) {
                        // msb first -> stop bit below
                        byte &= 0xFF << (8-axradio_phy_preamble_appendbits);
                        byte |= 0x80 >> axradio_phy_preamble_appendbits;
                    } else {
                         // lsb first -> stop bit above
                        byte &= 0xFF >> (8-axradio_phy_preamble_appendbits);
                        byte |= 0x01 << axradio_phy_preamble_appendbits;
                    }
                    ax5043_write_reg(spip,AX5043_REG_FIFODATA, byte, ret_value);
                }
                if ((ax5043_read_reg(spip,AX5043_REG_FRAMING, (uint8_t)0x00, ret_value) & 0x0E) == 0x06 && axradio_framing_synclen) {
                    // write SYNC word if framing mode is raw_patternmatch, might use SYNCLEN > 0 as a criterion, but need to make sure SYNCLEN=0 for WMBUS (chip automatically sends SYNCWORD but matching in RX works via MATCH0PAT)
                    uint8_t len_byte = axradio_framing_synclen;
                    uint8_t i = (len_byte & 0x07) ? 0x04 : 0;
                    // SYNCLEN in bytes, rather than bits. Ceiled to next integer e.g. fractional bits are counted as full bits;v
                    len_byte += 7;
                    len_byte >>= 3;
                    ax5043_write_reg(spip,AX5043_REG_FIFODATA, AX5043_FIFOCMD_DATA | ((len_byte + 1) << 5), ret_value);
                    ax5043_write_reg(spip,AX5043_REG_FIFODATA, axradio_framing_syncflags | i, ret_value);
                    for (i = 0; i < len_byte; ++i) {
                        // better put a brace, it might prevent SDCC from optimizing away the assignement...
                        ax5043_write_reg(spip,AX5043_REG_FIFODATA, axradio_framing_syncword[i], ret_value);
                    }
                }
                axradio_trxstate = trxstate_tx_packet;
                continue;
            }
            if (cnt < 4) {
                ax5043_write_reg(spip,AX5043_REG_FIFOSTAT, 4, ret_value); // commit
                chThdSleepMilliseconds(1);
                continue;
            }
            cnt = 255;
            if (axradio_txbuffer_cnt < 255*8)
                cnt = axradio_txbuffer_cnt >> 3;
            if (cnt) {
                axradio_txbuffer_cnt -= ((uint16_t)cnt) << 3;
                ax5043_write_reg(spip,AX5043_REG_FIFODATA, AX5043_FIFOCMD_REPEATDATA | (3 << 5), ret_value);
                ax5043_write_reg(spip,AX5043_REG_FIFODATA, axradio_phy_preamble_flags, ret_value);
                ax5043_write_reg(spip,AX5043_REG_FIFODATA, cnt, ret_value);
                ax5043_write_reg(spip,AX5043_REG_FIFODATA, axradio_phy_preamble_byte, ret_value);
                continue;
            }
            {
                uint8_t byte = axradio_phy_preamble_byte;
                cnt = axradio_txbuffer_cnt;
                axradio_txbuffer_cnt = 0;
                ax5043_write_reg(spip,AX5043_REG_FIFODATA, AX5043_FIFOCMD_DATA | (2 << 5), ret_value);
                ax5043_write_reg(spip,AX5043_REG_FIFODATA, 0x1C, ret_value);
                if (ax5043_read_reg(spip,AX5043_REG_PKTADDRCFG, (uint8_t)0x00, ret_value) & 0x80) {
                    // msb first -> stop bit below
                    byte &= 0xFF << (8-cnt);
                    byte |= 0x80 >> cnt;
                } else {
                     // lsb first -> stop bit above
                    byte &= 0xFF >> (8-cnt);
                    byte |= 0x01 << cnt;
                }
                ax5043_write_reg(spip,AX5043_REG_FIFODATA, byte, ret_value);
            }
            continue;

        case trxstate_tx_packet:
            if (cnt < 11) {
                ax5043_write_reg(spip,AX5043_REG_FIFOSTAT, 4, ret_value); // commit
                chThdSleepMilliseconds(1);
                continue;
            }
            {
                uint8_t flags = 0;
                if (!axradio_txbuffer_cnt)
                    flags |= 0x01; // flag byte: pkt_start
                {
                    uint16_t len = axradio_txbuffer_len - axradio_txbuffer_cnt;
                    cnt -= 3;
                    if (cnt >= len) {
                        cnt = len;
                        flags |= 0x02; // flag byte: pkt_end
                    }
                }
                if (!cnt)
                    goto pktend;
                ax5043_write_reg(spip,AX5043_REG_FIFODATA, AX5043_FIFOCMD_DATA | (7 << 5), ret_value);
                ax5043_write_reg(spip,AX5043_REG_FIFODATA, cnt + 1, ret_value); // write FIFO chunk length byte (length includes the flag byte, thus the +1)
                ax5043_write_reg(spip,AX5043_REG_FIFODATA, flags, ret_value);
                ax5043_writefifo(spip,&axradio_txbuffer[axradio_txbuffer_cnt], cnt);
                axradio_txbuffer_cnt += cnt;
                if (flags & 0x02)
                    goto pktend;
            }
            break;

        default:
            chprintf(DEBUG_CHP, "ERROR: Unexpected state found in transmit \r\n");
        }
    }

pktend:
    ax5043_write_reg(spip,AX5043_REG_RADIOEVENTMASK0, 0x01, ret_value); // enable REVRDONE event
    ax5043_write_reg(spip,AX5043_REG_FIFOSTAT, 4, ret_value); // commit
}





/**
 * Transmits a packet
 * Logic similar to axradio_transmit()
 * @param conf the AX5043 configuration handler
 * @return void 
 * 
 */

uint8_t transmit_packet(SPIDriver * spip, const struct axradio_address *addr, const uint8_t *pkt, uint16_t pktlen) 
{
    uint8_t ret_value[3]={0,0,0};
	ax5043_trxstate_t axradio_trxstate;
	uint16_t axradio_txbuffer_len;
	uint8_t axradio_txbuffer[PKTDATA_BUFLEN];
	struct axradio_address_mask axradio_localaddr;
	uint16_t axradio_txbuffer_cnt = 0;

    axradio_txbuffer_len = pktlen + axradio_framing_maclen;
    if (axradio_txbuffer_len > sizeof(axradio_txbuffer))
        return AXRADIO_ERR_INVALID;
    memset(axradio_txbuffer, 0, axradio_framing_maclen);
    memcpy(&axradio_txbuffer[axradio_framing_maclen], pkt, pktlen);
    if (axradio_framing_destaddrpos != 0xff)
        memcpy(&axradio_txbuffer[axradio_framing_destaddrpos], &addr->addr, axradio_framing_addrlen);
    if (axradio_framing_sourceaddrpos != 0xff)
        memcpy(&axradio_txbuffer[axradio_framing_sourceaddrpos], &axradio_localaddr.addr, axradio_framing_addrlen);
    if (axradio_framing_lenmask) {
        uint8_t len_byte = (uint8_t)(axradio_txbuffer_len - axradio_framing_lenoffs) & axradio_framing_lenmask; // if you prefer not counting the len byte itself, set LENOFFS = 1
        axradio_txbuffer[axradio_framing_lenpos] = (axradio_txbuffer[axradio_framing_lenpos] & (uint8_t)~axradio_framing_lenmask) | len_byte;
    }
/*
    if (axradio_framing_swcrclen)
        axradio_txbuffer_len = axradio_framing_append_crc(axradio_txbuffer, axradio_txbuffer_len);
    if (axradio_phy_pn9)
        pn9_buffer(axradio_txbuffer, axradio_txbuffer_len, 0x1ff, -(ax5043_read_reg(spip, AX5043_REG_ENCODING, (uint8_t)0x00, ret_value) & 0x01));
    axradio_txbuffer_cnt = axradio_phy_preamble_longlen;
*/
    //ax5043_prepare_tx(spip);

    ax5043_read_reg(spip,AX5043_REG_RADIOEVENTREQ0, (uint8_t)0x00, ret_value);; // make sure REVRDONE bit is cleared, so it is a reliable indicator that the packet is out
    ax5043_write_reg(spip, AX5043_REG_FIFOSTAT, 3, ret_value); // clear FIFO data & flags (prevent transmitting anything left over in the FIFO, this has no effect if the FIFO is not powerered, in this case it is reset any way)
    axradio_trxstate = trxstate_tx_longpreamble;

    if ((ax5043_read_reg(spip,AX5043_REG_MODULATION, (uint8_t)0x00, ret_value) & 0x0F) == 9) { // 4-FSK
        ax5043_write_reg(spip,AX5043_REG_FIFODATA, AX5043_FIFOCMD_DATA | (7 << 5), ret_value);
        ax5043_write_reg(spip,AX5043_REG_FIFODATA, 2, ret_value);  // length (including flags)
        ax5043_write_reg(spip,AX5043_REG_FIFODATA, 0x01, ret_value);  // flag PKTSTART -> dibit sync
        ax5043_write_reg(spip,AX5043_REG_FIFODATA, 0x11, ret_value); // dummy byte for forcing dibit sync
    }
    transmit_loop(spip, axradio_trxstate, axradio_txbuffer_len, axradio_txbuffer, axradio_txbuffer_cnt);
    //ax5043_write_reg(spip,AX5043_REG_PWRMODE, AX5043_FULL_TX, ret_value);
    ax5043_set_pwrmode(spip, AX5043_FULL_TX);

    ax5043_read_reg(spip,AX5043_REG_RADIOEVENTREQ0, (uint8_t)0x00, ret_value);
    //printf("INFO: Waiting for transmission to complete\n");
    while (ax5043_read_reg(spip,AX5043_REG_RADIOSTATE, (uint8_t)0x00, ret_value) != 0) {
        chThdSleepMilliseconds(1);
    }
    //printf("INFO: Transmission complete\n");

    ax5043_write_reg(spip,AX5043_REG_RADIOEVENTMASK0, 0x00, ret_value);

    return AXRADIO_ERR_NOERROR;
}



 

uint8_t receive_loop(SPIDriver * spip, uint8_t axradio_rxbuffer[]) {
    uint8_t ret_value[3]={0,0,0};
    uint8_t fifo_cmd;
    uint8_t i;
    uint8_t b0;
    uint8_t b1;
    uint8_t b2;
    uint8_t b3;
    uint8_t len = ax5043_read_reg(spip, AX5043_REG_RADIOEVENTREQ0, (uint8_t)0x00, ret_value); // clear request so interrupt does not fire again. sync_rx enables interrupt on radio state changed in order to wake up on SDF detected

    uint8_t bytesRead = 0;

 
    chprintf(DEBUG_CHP,"INFO: Waiting for a packet\r\n");
    while ((ax5043_read_reg(spip, AX5043_REG_FIFOSTAT, (uint8_t)0x00, ret_value) & 0x01) != 1) { // FIFO not empty
        fifo_cmd = ax5043_read_reg(spip, AX5043_REG_FIFODATA, (uint8_t)0x00, ret_value); // read command
        len = (fifo_cmd & 0xE0) >> 5; // top 3 bits encode payload len
        if (len == 7)
            len = ax5043_read_reg(spip, AX5043_REG_FIFODATA, (uint8_t)0x00, ret_value); // 7 means variable length, -> get length byte
        fifo_cmd &= 0x1F;
        switch (fifo_cmd) {
        case AX5043_FIFOCMD_DATA:
            if (!len)
                break;
            ax5043_read_reg(spip, AX5043_REG_FIFODATA, (uint8_t)0x00, ret_value); // Discard the flags
            --len;
            bytesRead = ax5043_readfifo(spip, axradio_rxbuffer, len);
               break;

        case AX5043_FIFOCMD_RFFREQOFFS:
            if (axradio_phy_innerfreqloop || len != 3)
                goto dropchunk;
            i = ax5043_read_reg(spip, AX5043_REG_FIFODATA, (uint8_t)0x00, ret_value);
            i &= 0x0F;
            i |= 1 + (uint8_t)~(i & 0x08);
            b3 = ((int8_t)i) >> 8;
            b2 = i;
            b1 = ax5043_read_reg(spip, AX5043_REG_FIFODATA, (uint8_t)0x00, ret_value);
            b0 = ax5043_read_reg(spip, AX5043_REG_FIFODATA, (uint8_t)0x00, ret_value);

            chprintf(DEBUG_CHP,"INFO: RF Frequency Offset: 0x%02x%02x%02x%02x\r\n", b3, b2, b1, b0);
            break;

        case AX5043_FIFOCMD_FREQOFFS:
             if (!axradio_phy_innerfreqloop || len != 2)
                 goto dropchunk;
             b1 = ax5043_read_reg(spip, AX5043_REG_FIFODATA, (uint8_t)0x00, ret_value);
             b0 = ax5043_read_reg(spip, AX5043_REG_FIFODATA, (uint8_t)0x00, ret_value);

             chprintf(DEBUG_CHP,"INFO: Frequency offset: 0x%02x%02x\r\n", b1, b2);
             break;

        case AX5043_FIFOCMD_RSSI:
            if (len != 1)
                goto dropchunk;
            {
                int8_t r;
                r = ax5043_read_reg(spip, AX5043_REG_FIFODATA, (uint8_t)0x00, ret_value);

                chprintf(DEBUG_CHP,"INFO: RSSI %d\r\n", (int)r);
            }
            break;

        default:
               chprintf(DEBUG_CHP, "ERROR: Unknown FIFO cmd %x \r\n", fifo_cmd);
               dropchunk:
                 chprintf(DEBUG_CHP, "WARNING: Discarding chunk in FIFO\r\n");
            if (!len)
                break;
            i = len;
            do {
                int8_t data;
                data = ax5043_read_reg(spip, AX5043_REG_FIFODATA, (uint8_t)0x00, ret_value);// purge FIFO
                chprintf(DEBUG_CHP,"Unknown: %x \r\n", data);
            }
            while (--i);
               break;
        }
    }

    //chprintf(DEBUG_CHP, "INFO: Done waiting for a packet\r\n");

    return bytesRead;

}



/*==========================================================================*/
/* Interface implementation.                                                */
/*==========================================================================*/

static const struct AX5043VMT vmt_device = {
    (size_t)0,
};

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Initializes an instance.
 *
 * @param[out] devp     pointer to the @p AX5043Driver object
 *
 * @init
 */
void ax5043ObjectInit(AX5043Driver *devp) {
    devp->vmt = &vmt_device;

    devp->config = NULL;

    devp->state = AX5043_STOP;
}


/**
 * @brief   Configures and activates the AX5043 Radio Driver.
 *
 * @param[in] devp      pointer to the @p AX5043Driver object
 * @param[in] config    pointer to the @p AX5043Config object
 *
 * @api
 * TODO return a -ve return code if there are any errors
 */
void ax5043Start(AX5043Driver *devp, const AX5043Config *config) {
    uint8_t rxbuf[3]={0,0,0};

    osalDbgCheck((devp != NULL) && (config != NULL));
    osalDbgAssert((devp->state == AX5043_STOP) ||
            (devp->state == AX5043_READY),
            "ax5043Start(), invalid state");

    devp->config = config;
}

//!@

