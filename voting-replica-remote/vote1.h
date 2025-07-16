/**********************************************************/
/*           vote.h version 5.2 with name vote1.h         */
/*         selective Solicitation                         */
/*    with variable sized data messages                   */
/**********************************************************/

#include <sys/time.h>
#include <signal.h>
#include <time.h>
#include <assert.h>
#include <math.h>

#define COMM_PORT 8710   //port used for multicasting
#define UDP_PORT 8720    //port used for control messages
#define SERVER_ADDRESS 192.168.0.1

#define WAKEUP_POINT 0.9  //The time the interface card should be waken up before 100% of the spread time is done.


#define HV_SIZE 20 //the size of the history vector
#define TRACE_SIZE 20//the size of the voting trace, greater than the maximum possible number of iterations.
#define PAYLOAD_SIZE 60000//the maximum size of the payload
#define HV_ENABLED 1//whether history vector is enabled or not

//Td: Measured in milli second, the estimate for max network delay
//a msg can incur.

//Different voting mode the software supports:
#define ICED 0
#define IDEC 1
#define EDEC 2
#define HYBRID 3

//TYPES OF MSGS

#define START 500
#define GIVEDE 5        //query for history vectors from all voters
#define SENDDE 8        //response message to GIVEDE message

#define VOTER_VALUE 1
#define POLL 2
#define VOTER_DECISION 3
#define DECISION 4
#define TERMINATE 555

#define HV_QUERY 10 //selective solicitation message for history vector
#define HV_REPLY 11 //answer to selective solicitation message for history vector

#define VALUETO 6
#define VOTETO 7

//TIMER EXPIRATION TYPES

#define ITERATION_TIMEOUT 44  //new stuff
#define GIVEDE_TIMEOUT 88     //new stuff

#define ROUND_TIMEOUT 55
#define DISSENT_TIMEOUT 66  //dissent time out, voting has to end
#define VOTE_TIMEOUT 99
#define MSG_READY 77
#define SLEEP_TIMEOUT 111

#define DATA_READY 11 //data ready on voter
#define CLEAR_TO_SEND 22 //clear to send, epsl expired
#define START_TIMEOUT 33 //START messages can't get any response

//END TYPES OF MSGS

//VOTER STATUS
#define UNKNOWN -1
#define FALSE 0
#define TRUE 1

//When solicited for votes, voters send one of the following two values
#define AGREE 1
#define DISAGREE 0

//VOTER TYPES, GOOD VOTER OR INFILTRATOR
#define BAD 0
#define GOOD 1

#define GOOD_VALUE 23
#define BAD_VALUE -73

//HOST_ID
#define SERVER_ID 100

//switches
#define POLL_FOR_ID 1 //poll for id or poll for value
#define VOTING_TRACE_ENABLED 0//voting trace enabled by default

#define PACKET_LOSS_RATE 0 //percentage of packets that are going to be dropped

#define COMP_DELAY 100000 //the time spent on checking values (microseconds)

//Following stucture is used for all the communication among the
//different processes. i.e. voters and manager.
//This very structure is also used for IPC of different layers,
//e.g. timer and voting layer.

struct vote_msg_str
{
	int type;  // START, VOTER_VALUE, POLL, VOTER_DECISION, DECISION, etc....
	int id;    // voter id
	int mode;  // ICED, IDEC, EDEC, HYBRID
	int value; // the voting value
	int round; // the voting round, a round might be finished in a number of iterations.
	int iteration;//iteration, as long as no agreement is reached, revote.

	//parameters used for automation
	int loss_rate;
	int sprd;
	int epsl;//from the voter side, the number of data messages dropped	due to sleeping
	int bvn;//from the voter side, the number of control messages dropped due to sleeping
	int size; //size of data messages

	int v_trace[TRACE_SIZE];
	int payload_size; //the size of the payload if the packet is a data packet
};

//
//Following stucture is used to store voter information
//which includes wether the voter has voted
//what the voted value is
//

struct voter_info_str
{
	int init;   // whether the voter has initiated a voting
	int voted;  // whether the voter has voted 1 true, 0 false
	int value;  // voted value, AGREE or DISAGREE
};

//
//history vector data structure that is going to be used by the voters to store voting decision history
//
struct hv_struct
{
	int position;//the position that stores the decision of last round
	int last_round;//the last round of decision stored
	int count;//total number of decisions stored
	int buffer[HV_SIZE]; //decision buffer 0 dissent, -1 unknown, 1 consent
};

//Global Variables

//global variable to indicate whether start timer for a iteration has expired.
int start_timeout_flag = FALSE;

//global variable to indicate whether round timer has expired.
int round_timeout_flag = FALSE;

//global variable used to test whether the current voting round should be ended.
int dissent_timeout_flag=FALSE;

//global variable used to test whether the current voter has the value ready.
int data_ready_flag=FALSE;

//global variable used to test whether the current voter can propose the voter value since epsl timer has expired.
int clear_to_send_flag=FALSE;

//global variable used to test whether the current voter can send out the decision.
int sleep_timeout_flag = FALSE;


/********************************************/
/*             Function Definitions         */
/********************************************/

/*fill in message content*/

void build_message(struct vote_msg_str * message, int type, int id, int mode, int value, int round, int iteration, int packet_loss_rate, int sprd, int epsl, int bvn, int size, int v_trace[], int payload_size)
{
	int i;
	message->type  = type;
	message->id    = id;
	message->mode  = mode;
	message->value = value;
	message->round = round;
	message->iteration = iteration;
	message->loss_rate = packet_loss_rate;
	message->sprd  = sprd;
	message->epsl  = epsl;
	message->bvn   = bvn;
	message->size  = size;

	if(v_trace!=NULL)//v_trace is going to be used to store voting trace, history vector, history vector solicitation list and decision message solicitation list, the size in this version is 20
	{
		for(i=0; i<TRACE_SIZE; i++)
			message->v_trace[i]=v_trace[i];
	}

	if(message->type == VOTER_VALUE)//if it's a data packet, the payload_size field will not be -1.
	{
		message->payload_size = payload_size;
	}
	else
	{
		message->payload_size = -1; //unknown if it's just a control message
	}

}

/*print out the content of a message*/

void print_msg(struct vote_msg_str msg)
{
	int i;
	printf("Message type: %d\n", msg.type);
	printf("Voter id: %d\n", msg.id);
	printf("Voting mode: %d\n", msg.mode);
	printf("Voting value: %d\n", msg.value);
	printf("Voting round: %d\n", msg.round);
	printf("Voting iteration: %d\n", msg.iteration);
	printf("Packet loss rate: %d%%\n", msg.loss_rate);
	printf("Sprd value: %d\n", msg.sprd);
	printf("Epsl value: %d\n", msg.epsl);
	printf("Bvn value: %d\n", msg.bvn);
	printf("Size: %d\n", msg.size);
	for(i=0; i<TRACE_SIZE; i++)
		printf("%2d ", msg.v_trace[i]);

	printf("Payload size is: %d\n", msg.payload_size);
}
/*print out the type and sender of a message*/
void print_msg_info(struct vote_msg_str msg)
{
	int i;
	printf("It's a ", msg.type);
	switch(msg.type)
	{
		case START: printf("START message"); break;
		case GIVEDE: printf("GIVEDE message"); break;
		case SENDDE: printf("SENDDE message"); break;
		case VOTER_VALUE: printf("VOTER_VALUE message"); break;
		case POLL: printf("POLL message"); break;
		case VOTER_DECISION: printf("VOTER_DECISION message"); break;
		case DECISION: printf("DECISION message"); break;
		case HV_QUERY: printf("HV_QUERY message"); break;
		case HV_REPLY: printf("HV_REPLY message"); break;
	}

	printf(" from voter %d\n", msg.id);

}

 /* The signal handler just clears the flag and re-enables itself. */
void catch_alarm(int sig)
{
   //printf("Alarm caught\n");
   signal (sig, catch_alarm);
}


/*print hello, used only for debugging*/
void print_hello(int x)
{
	//printf("Hello 1!\n");
    /* Establish a handler for SIGALRM signals. */
    signal (SIGALRM, catch_alarm);


}

/*set start_timeout_flag, START message get no response*/
void set_start_timeout(int x)
{
	//printf("START TIMEOUT!\n");
	start_timeout_flag=TRUE;

      /* Establish a handler for SIGALRM signals. */
       signal (SIGALRM, catch_alarm);

}

/*set round timeout timer*/

void set_round_timeout(int x)
{
	//printf("ROUND TIMEOUT!(HV timer expired)\n");
	round_timeout_flag=TRUE;
      /* Establish a handler for SIGALRM signals. */
       signal (SIGALRM, catch_alarm);


}

/*set dissent_timeout*/
void set_dissent_timeout(int x)
{
	//printf("Iteration Timeout!\n");
	dissent_timeout_flag=TRUE;
      /* Establish a handler for SIGALRM signals. */
       signal (SIGALRM, catch_alarm);

}

/*set data_ready_flag, tm time expired*/
void set_data_ready(int x)
{
	//printf("VOTER Data Ready!\n");
	data_ready_flag=TRUE;
      /* Establish a handler for SIGALRM signals. */
       signal (SIGALRM, catch_alarm);


}

/*set sleep_time_out_flag to true*/
void set_sleep_timeout(int x)
{
	//printf("Voter wake up!\n");
	sleep_timeout_flag=TRUE;
      /* Establish a handler for SIGALRM signals. */
       signal (SIGALRM, catch_alarm);


}

/*set clear_to_send_flag, epsl time expired*/
void set_clear_to_send(int x)
{
	printf("Clear To Send!\n");
	clear_to_send_flag=TRUE;
      /* Establish a handler for SIGALRM signals. */
       signal (SIGALRM, catch_alarm);


}

/*reset the timer*/
void reset_timer()
{
    //printf("resetting timer\n");
    setitimer(ITIMER_REAL, 0, (struct itimerval *)0);
}




/* timeout for collecting writing messages */


void set_timer(int micros,int type)
{
    struct itimerval it;

    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    it.it_value.tv_sec = micros/1000000;
    it.it_value.tv_usec = micros%1000000;

    //first reset the timer
    reset_timer();

    //printf("setting timer for %d us \n", micros);

    if(type==DISSENT_TIMEOUT)
    {
    	signal(SIGALRM, set_dissent_timeout);
    	//printf("Timer set to %d us for ITERATION_TIMEOUT!\n", micros);
	}

    else if(type==DATA_READY)
    {
    	signal(SIGALRM, set_data_ready);
    	//printf("Timer set to %d us for data_ready!\n", micros);
	}

    else if(type==CLEAR_TO_SEND)
    {
     	signal(SIGALRM, set_clear_to_send);
     	//printf("Timer set to %d us for clear_to_send!\n", micros);
	}

    else if(type==START_TIMEOUT)
    {
    	signal(SIGALRM, set_start_timeout);
    	//printf("Timer set to %d us for START_TIMEOUT!\n", micros);
	}

    else if(type==ROUND_TIMEOUT)
    {
    	signal(SIGALRM, set_round_timeout);
    	//printf("Timer set to %d us for ROUND_TIMEOUT!\n", micros);
	}
    else if(type==SLEEP_TIMEOUT)
    {
    	signal(SIGALRM, set_sleep_timeout);
    	//printf("Timer set to %d us for SLEEP_TIMEOUT!\n", micros);
	}
	else
	{
		printf("Unsupported alarm type!\n");
		exit(0);
	}

    setitimer(ITIMER_REAL, &it, (struct itimerval *)0);/*one timer*/
}

unsigned long check_timer()
{
	struct itimerval it;
	getitimer(ITIMER_REAL, &it);
	//printf("%.d seconds and %ld milliseconds to go!\n", it.it_value.tv_sec, it.it_value.tv_usec);
	return it.it_value.tv_usec;

}


struct timeval current_time()
{
	struct timeval time;
    gettimeofday(&time,0);
    return time;
}

int findID(int x, int y[], int length)
{
	int i;
	for (i=0; i<length; i++)
	{
        if (x==y[i]) return 1;
    }
    return 0;
}

//functions defined to process the history vector.

int reset_hv(struct hv_struct * hvp, int round)
{
	int i;
	hvp->position = -1;//last position storing a record is not valid, -1 here for convenience
	hvp->count=0;
	hvp->last_round = round;
	for(i=0; i<HV_SIZE; i++)
		hvp->buffer[i]=-1;

	return 1;
}

int print_hv(struct hv_struct hv)
{
	int count=0;
	int i;

	printf("Last decision recorded in history vector is for round %d. There are totally %d decisions recorded:\n", hv.last_round, hv.count);
	for(i=0; i<HV_SIZE; i++)
		printf("%d ", hv.buffer[i]);
	printf("\n");

	return 1;

}

int update_hv(struct hv_struct * hvp, int round, int decision)
{
	int distance;
	distance = round - hvp->last_round;

	hvp->count += distance;

	if(hvp->count>20)
	{
		//reset_hv(hvp, round-1);
		printf("History Vector overflows at round %d.\n", round);
		exit(0);
	}

	hvp->position += distance;

	hvp->last_round = round;

	if(decision == 0 || decision == 1)//0 disagree, 1 agree
		hvp->buffer[hvp->position]=decision;
	else
	{
		printf("The decision input to the update_hv function is not legal!");
		exit(0);
	}
	return 1;
}

//functions to read the latest "size" number of records from the history vector.
int read_hv(struct hv_struct hv, int round_to_end, int * buffer, int size)
{

	int i, j;

	int position = hv.position;//position of last record

	assert(size<=HV_SIZE);

	j=0;

	for(i=0; i< size; i++, j++)
	{
		buffer[i]=hv.buffer[j];
	}

	return 1;

}

//print an history vector or integer array of size 20
void print_array(int array[20])
{
	int index;
	for(index=0; index<20; index++)
		printf("%d ", array[index]);
	printf("\n");
}

double exp_random(double theta)
{

	long precision = 1000000;
	double u = ((double)(rand()%precision))/precision;

	//printf(" %f\n", u);

	return (-1)*theta*log(u);

}




