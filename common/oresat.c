#include "oresat.h"
#include "CANopen.h"

void oresat_init(uint8_t node_id) {
    /*uint8_t obr_node_id = (FLASH->OBR & FLASH_OBR_DATA0) >> FLASH_OBR_DATA0_Pos;*/
    uint8_t obr_node_id = 0xFF;

    //If node ID is not overridden, set node ID
    if (node_id == 0) {
        //TODO: If node ID is not set or is invalid, get new node ID
        if (obr_node_id  == 0xFF) {
            obr_node_id = 0x7F;
        }
        node_id = obr_node_id;
    }

    //TODO: If the node_id is out of range, request a new node ID
    if (node_id > 0x7F)
        node_id = 0x7F;

    //Initialize CAN Subsystem
    CO_init((uint32_t)&CAND1,node_id,1000);

    return;
}
