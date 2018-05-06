#ifndef _ACS_H_
#define _ACS_H_

#include "ch.h"
#include "hal.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define WA_ACSTHD_SIZE 128

extern char *state_name[];
extern char *event_name[];

typedef enum {
	ST_ANY=-1,
	ST_OFF,
	ST_INIT,
	ST_RDY,
	ST_RW,
	ST_MTQR
}acs_state;

typedef enum {
	EV_ANY=-1,
	EV_OFF,
	EV_INIT,
	EV_RDY,
	EV_RW,
	EV_MTQR,
	EV_REP,
	EV_STATUS,
	EV_END // this must be the last event
}acs_event;


typedef struct{
	acs_state cur_state;
	acs_event event;
}ACS;

typedef struct{
	acs_state state;
	acs_event event;
	int (*fn)(ACS *acs);
}acs_transition,acs_trap;

extern int acs_statemachine(ACS *acs);

extern int acsInit(ACS *acs);

extern THD_WORKING_AREA(wa_acsThread,WA_ACSTHD_SIZE);
extern THD_FUNCTION(acsThread,acs);

#endif
