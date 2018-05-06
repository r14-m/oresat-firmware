#include "statemachine.h"

//ACS acs;

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
	"EV_END"
};

static void print_state(int state){
	printf("%s",state_name[state+1]);	
}

static void print_event(int event){
	printf("%s",event_name[event+1]);	
}

static int state_off(ACS *acs){
	(void)acs;
	printf("***call to state_off: ");	
	print_state(ST_OFF);
	printf("\n");
	return ST_OFF;
}

static int state_init(ACS *acs){
	(void)acs;
	printf("***call to state_init: ");	
	print_state(ST_INIT);
	printf("\n");
	return ST_INIT;
}

static int state_rdy(ACS *acs){
	(void)acs;
	printf("***call to state_rdy: ");	
	print_state(ST_RDY);
	printf("\n");
	return ST_RDY;
}

static int state_rw(ACS *acs){
	(void)acs;
	printf("***call to state_rw: ");	
	print_state(ST_RW);
	printf("\n");
	return ST_RW;
}

static int state_mtqr(ACS *acs){
	(void)acs;
	printf("***call to state_mtqr: ");	
	print_state(ST_MTQR);
	printf("\n");
	return ST_MTQR;
}

static int trap_report(ACS *acs){
	printf("***call to fsm_report, keeping state\n");	
	return acs->cur_state;
}

acs_trap trap[] = {
	{ST_ANY, 	EV_REP,		&trap_report}
};

#define EVENT_COUNT (int)(sizeof(trap)/sizeof(acs_trap))

static int fsm_trap(ACS *acs){
	printf("***call to fsm_trap, keeping state\n");	
	int i;
	
	for(i = 0;i < EVENT_COUNT;++i){
		if((acs->event == trap[i].event)){
			acs->cur_state = (trap[i].fn)(acs);
			break;
		}
	}

	return acs->cur_state;
}

acs_transition trans[] = {
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

acs_event getNextEvent(ACS *acs){
	acs_event event;
	char input[3]="-1\n";

	printf("current state: %d ",acs->cur_state);
	print_state(acs->cur_state);
	printf("\n");
	printf("request event? ");
	scanf(" %s", input);
	event = atoi(input);
	if(event < EV_ANY || event >= EV_END){
		printf("error, event out of range\n");
		return acs->cur_state; 
	}
	printf("event %s received\n", event_name[event+1]);
	return event;
}

void print_welcome(){
	printf("state enum: "); 
	printf("\t%d, \t%d, \t%d, \t %d, \t %d, \t %d\n",
			ST_ANY,ST_OFF,ST_INIT,ST_RDY,ST_RW,ST_MTQR);
	
	printf("state names: \t"); 
	print_state(ST_ANY); printf(", ");
	print_state(ST_OFF); printf(", ");
	print_state(ST_INIT); printf(", ");
	print_state(ST_RDY); printf(", ");
	print_state(ST_RW); printf(", ");
	print_state(ST_MTQR); printf("\n");
	
	printf("event enum: "); 
	printf("\t%d, \t%d, \t%d, \t %d, \t %d, \t %d, \t %d\n",
			EV_ANY,EV_OFF,EV_INIT,EV_RDY,EV_RW,EV_MTQR,EV_REP);

	printf("event names: \t"); 
	print_event(EV_ANY); printf(", ");
	print_event(EV_OFF); printf(", ");
	print_event(EV_INIT); printf(", ");
	print_event(EV_RDY); printf(", ");
	print_event(EV_RW); printf(", ");
	print_event(EV_MTQR); printf(", ");
	print_event(EV_REP); printf(", ");
	print_event(EV_END); printf("\n\n");
}

int acs_statemachine(ACS *acs){
	int i;

	print_welcome();

	acs->cur_state = state_init(acs);
	printf("entry state: %s\n",state_name[acs->cur_state+1]);
	printf("TRANS_CNT: %d\n",TRANS_COUNT);
	
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
		printf("\n");
	}
	
	return EXIT_SUCCESS;
}

