#include "acs.h"

char *state_name[] = {
	"ST_ANY",
	"ST_OFF",
	"ST_INIT",
	"ST_RDY",
	"ST_RW",
	"ST_MTQR"
};

char *event_name[] = {
	"EV_ANY",
	"EV_OFF",
	"EV_INIT",
	"EV_RDY",
	"EV_RW",
	"EV_MTQR",
	"EV_REP",
	"EV_STATUS",
	"EV_END"
};

static void print_state(int state){
	(void)state;
//	printf("%s",state_name[state+1]);	
}

/*
static void print_event(int event){
	(void)event;
//	printf("%s",event_name[event+1]);	
}
//*/

//#define STATE_STRING "*Calling transition -> " 
//#define TRAP_STRING "*Calling trap -> " 

static int state_off(ACS *acs){
	(void)acs;
//	printf("%sstate_off: ",STATE_STRING);	
	print_state(ST_OFF);
//	printf("\n");
	return ST_OFF;
}

static int state_init(ACS *acs){
	(void)acs;
//	printf("%sstate_init: ",STATE_STRING);	
	print_state(ST_INIT);
//	printf("\n");
	return ST_INIT;
}

static int state_rdy(ACS *acs){
	(void)acs;
//	printf("%sstate_rdy: ",STATE_STRING);	
	print_state(ST_RDY);
//	printf("\n");
	return ST_RDY;
}

static int state_rw(ACS *acs){
	(void)acs;
//	printf("%sstate_rw: ",STATE_STRING);	
	print_state(ST_RW);
//	printf("\n");
	return ST_RW;
}

static int state_mtqr(ACS *acs){
	(void)acs;
//	printf("%sstate_mtqr: ",STATE_STRING);	
	print_state(ST_MTQR);
//	printf("\n");
	return ST_MTQR;
}

static int trap_fsm_report(ACS *acs){
	(void)acs;
//	printf("%strap_fsm_report, keeping state!\n",TRAP_STRING);	
	return EXIT_SUCCESS;
}

static int trap_rw_status(ACS *acs){
	(void)acs;
//	printf("%strap_rw_status, keeping state!\n",TRAP_STRING);	
	return EXIT_SUCCESS;
}

static int trap_mtqr_status(ACS *acs){
	(void)acs;
//	printf("%strap_mtqr_status, keeping state!\n",TRAP_STRING);	
	return EXIT_SUCCESS;
}

const acs_trap trap[] = {
	{ST_ANY, 	EV_REP,			&trap_fsm_report},
	{ST_RW, 	EV_STATUS,	&trap_rw_status},
	{ST_MTQR, EV_STATUS,	&trap_mtqr_status}
};

#define EVENT_COUNT (int)(sizeof(trap)/sizeof(acs_trap))

static int fsm_trap(ACS *acs){
//	printf("%sfsm_trap, keeping state!\n",TRAP_STRING);	
	int i,trap_status;

	for(i = 0;i < EVENT_COUNT;++i){
		if(acs->cur_state == trap[i].state){
			if((acs->event == trap[i].event)){
				trap_status = (trap[i].fn)(acs);
				if(trap_status){
//					printf("trap error!\n");
				}
				break;
			}
		}
	}

	return acs->cur_state;
}

const acs_transition trans[] = {
	{ST_INIT, 	EV_RDY,		&state_rdy},
	{ST_INIT, 	EV_OFF,		&state_off},
	{ST_RDY, 		EV_RW,		&state_rw},
	{ST_RDY, 		EV_MTQR,	&state_mtqr},
	{ST_RDY, 		EV_OFF,		&state_off},
	{ST_RW, 		EV_RDY,		&state_rdy},
	{ST_MTQR, 	EV_RDY,		&state_rdy},
	{ST_ANY, 		EV_ANY,		&fsm_trap}
};

#define TRANS_COUNT (int)(sizeof(trans)/sizeof(acs_transition))

static acs_event getNextEvent(ACS *acs){
	acs_event event = EV_ANY;
	//char input[3]="-1\n";

	// TODO: get event from the bus

	if(event < EV_ANY || event >= EV_END){
	//	printf("error, event out of range\n");
		return acs->cur_state; 
	}
//	printf("event %s received\n", event_name[event+1]);
	return event;
}

int acs_statemachine(ACS *acs){
	int i;

	acs->cur_state = state_init(acs);
	
	while (acs->cur_state != ST_OFF) {
		acs->event = getNextEvent(acs);
		for (i = 0;i < TRANS_COUNT;++i) {
			if((acs->cur_state == trans[i].state)||(ST_ANY == trans[i].state)){
				if((acs->event == trans[i].event)||(EV_ANY == trans[i].event)){
					acs->cur_state = (trans[i].fn)(acs);
					break;
				}
			}
		}
	}
	
	return EXIT_SUCCESS;
}

extern int acsInit(ACS *acs){
	(void)acs;
	
	return EXIT_SUCCESS;
}

THD_WORKING_AREA(wa_acsThread,WA_ACSTHD_SIZE);
THD_FUNCTION(acsThread,acs) {
//  (void)arg;
  chRegSetThreadName("acsThread");

	acs_statemachine(acs);
  
	while (!chThdShouldTerminateX()) {
    chThdSleepMilliseconds(500);
  }
}
