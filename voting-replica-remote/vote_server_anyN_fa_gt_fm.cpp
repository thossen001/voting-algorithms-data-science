/*********************************************/
/*         VOTING SERVER PROGRAM v5.2        */
/*           Selective Solicitation          */
/*    with switching data message sizes      */
/*    version 5.1 is a stable version        */
/*********************************************/


/********************************************/
/*            Header Files                  */
/********************************************/
//#include <iostream>
#include <stdio.h>
#include "multicast.h"
#include "vote1.h"
#include <unistd.h>


/********************************************/
/*              Macros                      */
/********************************************/



#define TOTAL_NUMBER_ROUNDS 30// total number of tests for each set of parameters

#define TIMEOUT_VALUE_ITERATION 75*1000 //voting timer

#define TIMEOUT_VALUE_START 500*1000 //value timer

#define REDO 5//repeat each test REDO times

#define ITERATION_RETRY_LIMIT 3 //maximum number of resolications on decision message

#define RANGE 16 //maximum loss rate + 1

#define THRESHOLD_TO_EDEC 1  //minimum average number of errors to switch to EDEC

#define THRESHOLD_TO_ICED 2.97 //minimum average receival rate to switch to ICED






/********************************************/
/*             Function Declarations        */
/********************************************/
int evaluate_voting_traces(int (* v_trace)[TRACE_SIZE], int num_voters, int iteration);//function used to check the voting history during one voting round, if an agreement can be found for any previous iteration, the voting ends. This one is for ICED mode only.
int print_voting_traces(int (* v_trace)[TRACE_SIZE], int num_voters, int iteration);//Display the voting traces database
int update_voting_traces(int (* voting_traces)[TRACE_SIZE], int id, int v_trace[TRACE_SIZE], int iteration);//function to updata the voting trace database

int update_history_vectors(int (* history_vectors)[TRACE_SIZE], int id, int v_trace[TRACE_SIZE], int size);
int print_history_vectors(int (* history_vectors)[TRACE_SIZE], int num_voters, int size);
int evaluate_history_vectors(int (* history_vectors)[TRACE_SIZE], int num_voters, int size, int round, int bvn, int * valid_count);//round: last round in the vector
/********************************************/
/*             Global Variables             */
/********************************************/



/********************************************/
/*             Main function                */
/********************************************/

int main(int argc, char * argv[])
{
	struct vote_msg_str msg_out, msg_in;//server's messages
	char local_host[16]="127.0.0.1";//loopback ip address
	char group[16]="224.12.12.0";//multicast group id

	int num_voters=3;//default number of voters.
	int vote_mode=IDEC;//default voting mode

	int sprd=100000;//default calculation time.
	int epsl=30000;//default epsilon value
	int bvn=1;//default number of bad voters
	int FA=3;
	int FM=4;
	int SPRD=50;
	int redo=1;

	int total_number_rounds;

	int bad_decisions=0;//counter for bad decisions.
	int corrected_decisions=0;//decisions that can be corrected by hv
	int voting_for=-1; //a voter id, the voter whom is voted for the current round

	int power_save_enabled=0;

	struct timeval cur_time;

	cur_time=current_time();

	//printf("Current time is: %ld:%ld\n", cur_time.tv_sec, cur_time.tv_usec);


	//long wait_time=0;
	long wait_until=0;

	double theta1 = 300.0; //theta for attacking inter-arrival time
	double theta2 = 40.0; //theta for attacking time



	char buffer[PAYLOAD_SIZE+200]; //receiving buffer for messages, very big



	/////////////////////////////////////////////
	//    read command line arguments          //
	/////////////////////////////////////////////
	if(argc!=9)
	{
		printf("Usage: ./server num_voters redo vote_mode(0: ICED 0: IDEC: 1 EDEC: 2) total_rounds powersave_mode fm fa(bvn) sprd\n");
		exit(0);
	}
	else
	{
		sscanf(argv[1], "%d", &num_voters);
		sscanf(argv[2], "%d", &redo);
		sscanf(argv[3], "%d", &vote_mode);
		sscanf(argv[4], "%d", &total_number_rounds);

		sscanf(argv[5], "%d", &power_save_enabled);

		sscanf(argv[6], "%d", &FM);
		if(FM >= num_voters/2) FM=num_voters/2-1;
		sscanf(argv[7], "%d", &FA);
		sscanf(argv[8], "%d", &SPRD);
		SPRD= SPRD*1000;

/*		if(bvn >= (float)num_voters/2 || bvn < 0 || num_voters <=0)
		{
			printf("Illegal input, bvn should be greater than -1 and less than half the total number of voters. Total number of voters should be greater than 0.\n");
			exit(0);
		}*/
		if(vote_mode >= 3 || vote_mode < 0)
		{
			printf("Unsupported voting mode!\n");
			exit(0);
		}
		else if(redo <= 0)
		{
			printf("Redo need to be greater than 0.\n");
			exit(0);
		}
		else if(total_number_rounds <= 0)
		{
			printf("Invalid round number!\n");
			exit(0);
		}
	}



	///////////////////////////////////////////////////
	//   data structure initializations              //
	///////////////////////////////////////////////////



	struct voter_info_str * voter;
	voter = new struct voter_info_str[num_voters];//data structure to store voter infomation

	int decision_received[TRACE_SIZE];//data structure storing decision receipt information for one iteration.


	//This is a dynamic sized FIFO queue implemented by an array to store VOTER_VALUE messages
	//the queue_pointer will be the postion of next message in the queue in the array
	//(queue_pointer+queue_length)%num_voters is the position of the end of the queue

	struct vote_msg_str * msg_queue;
	msg_queue = new struct vote_msg_str[num_voters];//a queue to store the VOTER_VALUE messages
	int queue_length=0;//length of the queue
	int queue_pointer=0;//next message in the VOTER_VALUE queue

	/*************************************/
	/*   Output file preparation         */
	/*************************************/

	FILE *fp_out;/*file pointer for the output file*/

	if((fp_out=fopen("result.csv", "w"))==NULL)/*output file open failed*/
	{
		printf("Output file opening failed\n");
		exit(0);
	} 
	// print the header
        fprintf(fp_out, "EDEC,N,iteration,Loss,sprd,fm,bvn,ttc,m,dm,fail,rrate,pl,ratio,dd,ad,w,t,c\n");

	fclose(fp_out);


	/*************************************/
	/*  Socket preparation start         */
	/*************************************/

	/////////////////////////////////////////////////////
	//    Creating a multicast socket for the voting   //
	/////////////////////////////////////////////////////
	int msock;//multicast socket
	msock=CreateMcastSocket(COMM_PORT);
	joinGroup(msock, group);//join the group

	/////////////////////////////////////////////////////
	//    Creating a unicast socket for the control    //
	/////////////////////////////////////////////////////
	int udp_sock;//unicast socket
	udp_sock=CreateUdpSocket(UDP_PORT);

	//////////////////////////////////////
	//      Select call set up          //
	//////////////////////////////////////

	fd_set rfds, afds;//afds is the active one, rfds is a static copy of afds
	int nfds = getdtablesize();//table size

	FD_ZERO(&afds);//cleaning up the records
	FD_ZERO(&rfds);

	//add all socket descriptors to afds//
	if(msock!= -1)
		FD_SET (msock, &afds);
	if(udp_sock!= -1)
		FD_SET (udp_sock, &afds);


	/*************************************/
	/*  Socket preparation ends          */
	/*************************************/


	///////////////////////////////////////////////
	//    variables for voting rounds             //
	///////////////////////////////////////////////

	int vote_round=1;//the index of the current round.

	int (*voting_traces)[TRACE_SIZE];//database for voting traces from each voter

	voting_traces = (int (*)[TRACE_SIZE])malloc(sizeof(int)*TRACE_SIZE*num_voters);

	int voting_trace_flag; //flag showing whether voting trace is going to be used.


	int (*history_vectors)[TRACE_SIZE];//database for history vectors from each voter

	history_vectors = (int (*)[TRACE_SIZE])malloc(sizeof(int)*TRACE_SIZE*num_voters);

	int * old_data_drops = (int *) malloc(sizeof(int)*num_voters);
	int * old_all_drops = (int *) malloc(sizeof(int)*num_voters);
	int * new_data_drops = (int *) malloc(sizeof(int)*num_voters);
	int * new_all_drops = (int *) malloc(sizeof(int)*num_voters);
	int data_drops=0;
	int all_drops=0;

	for(int i=0; i<num_voters; i++)
	{
		old_data_drops[i]=0;
		old_all_drops[i]=0;
		new_data_drops[i]=0;
		new_all_drops[i]=0;
	}

	/*********************************************************************/
	/*  Server repeat voting rounds with different parameter sets        */
	/*********************************************************************/
	int iter=0;
	int test_id = 1;
	int ascending_flag = TRUE; //flag showing whether the loss rate should increase or decrease

	int f_max;
	int packet_loss_rate=0;

	int total_messages; //messages received in one test
	int total_data_messages; //data messages received in one test
	int total_hv_messages; //total number of messages.

	//for edec, calculating the total number of decision messages received as an indicator for packet loss rate.
	int total_decision_messages=0;
	int total_iterations=0;

	int total_errors;//errors found by hv in one test
	int total_bad_decisions;
	long total_delay=0;//total delay in one test
	int failed_rounds;

	int data_size=0; //data packet size

	int voting_history[TRACE_SIZE];


	int attacked = FALSE;


	//setting up the paramters before each test.

	long starting_point = cur_time.tv_sec; //when the test starts.

	srand(cur_time.tv_usec);

	long wait = (long)exp_random(theta1);

	wait_until = cur_time.tv_sec + wait;
	packet_loss_rate = 0;



	while(1){


		//before each test, check the status
		cur_time = current_time();
		printf("Current time is: %ld:%ld\n", cur_time.tv_sec, cur_time.tv_usec);

		/* if(cur_time.tv_sec >= wait_until) //time reached
		{
		printf("---------------------------------- Timer Expired ------------------------------\n");

		if(attacked == FALSE)
		{
		attacked = TRUE;

		packet_loss_rate = 50+rand()%50;//%5 to %10 attack

		srand(cur_time.tv_usec);

		wait = (long)exp_random(theta2);


		wait_until = cur_time.tv_sec + wait;
		printf("Attacking starts\n");
		}
		else if(attacked == TRUE)
		{
		attacked = FALSE;
		packet_loss_rate = rand()%10;

		srand(cur_time.tv_usec);

		wait = (long)exp_random(theta1);
		wait_until = cur_time.tv_sec + wait;
		printf("Attacking ends\n");
		}
		}

		*/
		//for(packet_loss_rate=0; packet_loss_rate<=50; packet_loss_rate+=10){//packet_loss_rate

		for( data_size = 20000; data_size <=20000; data_size += 10000){//data packet size

			for( test_id = 1; test_id <= 1; ){ //packet_loss_rate

				// printf("Test %d started.\n", test_id);

				for(sprd=SPRD; sprd<=SPRD; sprd+=SPRD){//sprd

					//for(f_max=4; f_max<((float)num_voters/2); f_max++){ //f_max

					// printf("Spread: %d .\n", sprd);

					for(f_max=FM; f_max<=FM; f_max++){ //f_max

						//for(f_max=0; f_max<=4; f_max++){ //f_max

						printf("f_max: %d .\n", f_max);

						printf("FA: %d .\n", FA);
						
						for(bvn=FA; bvn<=FA; bvn++){//bvn
						// for(bvn=0; bvn<f_max; bvn++){//bvn
						// for(bvn=FA; bvn<FA; bvn++){//bvn
						       //usleep(1000000);

							printf("bvn: %d .\n", bvn);


							total_messages=0;
							total_data_messages=0;
							total_hv_messages=0;

							//for edec, calculating the total number of decision messages received as an indicator for packet loss rate.
							total_decision_messages=0;
							total_iterations=0;

							total_errors=0;
							total_delay=0;
							total_bad_decisions=0;
							failed_rounds=0;




							for(iter=0; iter<redo; iter++){//repeat the same test for a number of times

								printf("iter: %d .\n", iter);



								for(vote_round =1; vote_round<=total_number_rounds; vote_round++){//for each test, do "total_number_rounds" rounds.

									printf("vote_round: %d .\n", vote_round);


									if(VOTING_TRACE_ENABLED == 1 && vote_mode == ICED)
									{
										voting_trace_flag = TRUE;
										//initialize the voting trace database for next round.
										memset(voting_traces, 0, sizeof(int)*TRACE_SIZE*num_voters);
										//print_voting_traces(voting_traces, num_voters, TRACE_SIZE);
									}

									memset(history_vectors, 0, sizeof(int)*TRACE_SIZE*num_voters);//reset the history vectors database


									printf("Voting round %d of iteration %d in test %d starts.....\n", vote_round, iter, test_id);

									srand(time(0)+vote_round);//seed the random number generator for this round


									///////////////////////////////////////////////////////////////////////
									//     variables for one voting iteration of one voting round        //
									///////////////////////////////////////////////////////////////////////

									int vote_iteration=1;
									int server_msg_count=0;//messages sent out by the server
									int total_msg_count=0;
									int total_msg_dropped=0;

									int voting_flag=FALSE; //showing whether it's during a voting process or not

									int agreed_flag=FALSE; //showing whether an agreement has been reached


									//decision counters for 1 iteration in the current round
									int num_consent=0;   // consents
									int num_dissent=0;   // dissents
									int total_votes=0;   // total votes

									//timer variable
									struct timeval time_start, time_end;
									time_start=current_time();
									//printf("Current time is: %ld:%ld\n", time_start.tv_sec, time_start.tv_usec);

									//For the voter value message queue
									//reset the queue length and queue pointer
									//before each iteration
									queue_length=0;//length of the queue
									queue_pointer=0;//next message in the VOTER_VALUE queue


									for(int i=0; i< num_voters; i++)
									{
										voter[i].init=FALSE; //not initiated yet
										voter[i].voted=FALSE; //not voted
									}

									while(1) //revote until an agreement is reached
									{


										//////////////////////////////////////////////////////////
										// Clean up the data structures before each iteration   //
										//////////////////////////////////////////////////////////
										for(int i=0; i< num_voters; i++)
										{
											voter[i].voted=FALSE; //not voted
										}

										num_consent=1;   // consents to 1, at least the voter itself consents
										num_dissent=0;   // dissents to zero
										total_votes=0;   // total votes to zero

										voting_flag=FALSE; //voting not started for this iteration yet
										agreed_flag=FALSE; //no agreement yet
										dissent_timeout_flag=FALSE; //not timed out yet

										/////////////////////////////////////////////
										//      Sending out the first message      //
										/////////////////////////////////////////////


										//  msg_pointer, msg_type, voter_id, vote_mode, value=0, vote round, vote_iteration, sprd, epsl, bvn, history vector
										build_message(&msg_out, START, SERVER_ID, vote_mode, 0, vote_round, vote_iteration, packet_loss_rate, sprd, epsl, bvn, data_size, NULL, data_size);
										send_info(msock, (char *)&msg_out, sizeof(struct vote_msg_str), group, COMM_PORT);
										//send_info(udp_sock, (char *)&msg_out, sizeof(struct vote_msg_str), local_host, UDP_PORT);
										//printf("START message sent to %s on port %d!\n", group, COMM_PORT);

										start_timeout_flag = FALSE;
										set_timer(TIMEOUT_VALUE_START,START_TIMEOUT);//2 seconds waiting for response

										server_msg_count++; //server message counter increase by 1
										//total_msg_count++; //total message counter increased by 1

										int iteration_retry = 0;

										for(int i=0; i< num_voters; i++)
										{
											decision_received[i]=FALSE; //not received
										}


										////////////////////////////////////////////////////////
										//      Waiting for messages and Responding to them   //
										////////////////////////////////////////////////////////


										while(1)//receving messages for a voting iteration
										{
											if(start_timeout_flag==TRUE)//no response received for the START message, break and restart the current iteration
											{
												agreed_flag=FALSE;
												break;
											}

											if( dissent_timeout_flag==TRUE)// && vote_mode != EDEC)//voting ends for ICED and IDEC
											{
												dissent_timeout_flag == FALSE;
												if(voting_flag==TRUE)//during voting
												{
													//printf("ITERATION TIMEOUT for iteration %d received!\n", msg_in.iteration);

													//before ending the loop, check whether an agreement can be reached.
													if(vote_mode==ICED)
													{
														agreed_flag=TRUE;//not break before this, agreed
														break;
													}
													else if(vote_mode==IDEC)
													{
														agreed_flag=FALSE;//not break before this, disagreed
														break;
													}
													else if(vote_mode == EDEC)
													{
														agreed_flag == FALSE;//disagreed
														if(iteration_retry < ITERATION_RETRY_LIMIT && total_votes < num_voters)
														{
															iteration_retry++;
															//printf("\nNot enough voter decision messages received before time out, continue current iteration, resend poll message.\n");
															//printf("Iteration %d resumed for the %dth time!\n", vote_iteration, iteration_retry);
															for(int i=0; i< num_voters; i++)
															{
																if(voter[i].voted==TRUE)
																	decision_received[i]=TRUE; //not received
															}
															build_message(&msg_out, POLL, SERVER_ID, vote_mode, voting_for, vote_round, vote_iteration, packet_loss_rate, sprd, epsl, bvn, data_size, decision_received, data_size);
															send_info(msock, (char *)&msg_out, sizeof(struct vote_msg_str), group, COMM_PORT);
															//printf("POLL message resent to %s on port %d for voter %d!\n", group, COMM_PORT, voting_for);

															//debugging
															//print_array(decision_received);

															server_msg_count++; //server message counter increase by 1

															dissent_timeout_flag=FALSE;
															set_timer(TIMEOUT_VALUE_ITERATION,DISSENT_TIMEOUT);//2 seconds waiting


														}
														else
														{
															break; //retry failed. End the current iteration.
														}
													}

												}
												else
												{
													//printf("ITERATION TIMEOUT for iteration %d received before voting starts! Possible Error!\n", msg_in.iteration);
													exit(1);
												}
											}

											memcpy (&rfds, &afds, sizeof (rfds));
											// check if there is any activity on any of the descriptors in rdfd
											int ret_count = select(nfds, &rfds, NULL, NULL, NULL);//see if there is any event
											if (ret_count < 0)
											{
												continue;//move on, nothing received yet.
											}

											//////////////////////////////////////////////////////
											//       receive one messsage to msg_in             //
											//////////////////////////////////////////////////////

											if (FD_ISSET (msock, &rfds))//multicast messages arriving
											{
												int bytes_received = recv(msock, buffer, sizeof(buffer),0);
												if(bytes_received == sizeof(msg_in))
												{
													memcpy((char *)&msg_in, buffer, sizeof(msg_in));
													//printf("One Multicast message received!\n");
													//print_msg(msg_in);
												}
												else if(bytes_received == sizeof(msg_in)+data_size)
												{
													memcpy((char *)&msg_in, buffer, sizeof(msg_in));
													//printf("Data message received, total size is %d bytes.!\n", bytes_received);
												}
												else
												{
													printf("Multicast Package Receiving failed!\n");
													continue;
												}
											}
											else if (FD_ISSET (udp_sock, &rfds))//unicast messages arriving
											{
												int bytes_received = recv(udp_sock, buffer, sizeof(buffer),0);
												if(bytes_received == sizeof(msg_in))
												{
													memcpy((char *)&msg_in, buffer, sizeof(msg_in));
													//printf("One Multicast message received!\n");
													//print_msg(msg_in);
												}
												else
												{
													//printf("Unicast Package Receiving failed!\n");
													continue;
												}
											}



											if(msg_in.type == VOTER_VALUE)
											{
												total_data_messages++;
												total_msg_count++;
											}
											else if(msg_in.id!=SERVER_ID)
											{
												total_msg_count++;//increase the total control message counter by 1
											}

											////////////////////////////////////////////////////////////////////
											//   Drop a message with a preset possibility: PACKET_LOSS_RATE   //
											////////////////////////////////////////////////////////////////////

											if((rand()%1000)<packet_loss_rate)//drop a packet with PACKET_LOSS_RATE/1000 chance
											{

												//printf("One message dropped! Loss Rate %d/1000. ", packet_loss_rate);
												print_msg_info(msg_in);
												total_msg_dropped++;
												continue;
											}

											/////////////////////////////////////////////////////////////////////
											//   Discard outdated messages                                     //
											/////////////////////////////////////////////////////////////////////

											if(msg_in.round != vote_round) //discard messages doesn't belong to the current round
												continue;

											if(msg_in.iteration != vote_iteration) //discard messages doesn't belong to the current iteration
												continue;


											////////////////////////////////////////////////
											//     update and process the voting trace    //
											////////////////////////////////////////////////

											//printf("voting trace flag is %d, sender id is %d\n", voting_trace_flag, msg_in.id );

											if(VOTING_TRACE_ENABLED == TRUE && msg_in.id != SERVER_ID)
											{
												update_voting_traces(voting_traces, msg_in.id, msg_in.v_trace, vote_iteration);

												//printf("Type %d message received from voter %d. The current trace database is:\n", msg_in.type, msg_in.id);
												//print_voting_traces(voting_traces, num_voters, vote_iteration);

												if(evaluate_voting_traces(voting_traces, num_voters, vote_iteration)==1)
												{
													print_voting_traces(voting_traces,num_voters,vote_iteration);
													agreed_flag = TRUE; break;//an agreement found from the voting traces, stop the round.
												}
											}


											////////////////////////////////////////////////////////////
											// Do different jobs according to the type of the message //
											////////////////////////////////////////////////////////////

											if(msg_in.type == START && msg_in.id == SERVER_ID)//only the server is allow to send START message
											{
												//printf("START message for iteration %d received!\n", msg_in.iteration);
												if(msg_in.iteration == vote_iteration && voting_flag==FALSE && queue_length>=1)//voting not started yet and there are writing messages in the queue
												{

													voting_flag=TRUE;//voting started

													queue_length--;//reduce queue length by 1, use the first message in the queue, can be changed to random picking here


													//send a POLL message
													//  msg_pointer, msg_type, voter_id, vote_mode, value, vote_iteration, sprd, epsl, bvn, history vector
													voting_for = msg_queue[queue_pointer].id;

													build_message(&msg_out, POLL, SERVER_ID, vote_mode, voting_for, vote_round, vote_iteration, packet_loss_rate, sprd, epsl, bvn, data_size, decision_received, data_size);
													send_info(msock, (char *)&msg_out, sizeof(struct vote_msg_str), group, COMM_PORT);
													//printf("POLL message sent to %s on port %d for voter %d!\n", group, COMM_PORT, voting_for);

													server_msg_count++; //server message counter increase by 1
													//total_msg_count++; //total message counter increased by 1

													queue_pointer=(queue_pointer+1)%num_voters;//move the queue_pointer by one position to the right

													dissent_timeout_flag=FALSE;
													//if(vote_mode!=EDEC)
													set_timer(TIMEOUT_VALUE_ITERATION,DISSENT_TIMEOUT);//2 seconds waiting

													voter[voting_for].init=FALSE; //this voter can send more values for voting now.

												}
											}
											else if(msg_in.type == VOTER_VALUE && msg_in.id < num_voters && voting_flag==FALSE)//the voter id should be less than the total number of voters
											{


												if(msg_in.iteration==vote_iteration && voter[msg_in.id].init==FALSE)//new voter_value for current iteration
												{
													//collecting dropping information
													new_data_drops[msg_in.id] =msg_in.sprd;
													new_all_drops[msg_in.id] =msg_in.epsl;

													//printf("VOTER_VALUE message for iteration %d from voter %d received with sprd %d, epsl %d!\n", msg_in.iteration, msg_in.id, msg_in.sprd, msg_in.epsl);
													voter[msg_in.id].init=TRUE; //initiated, not allowed to write in again

													reset_timer();//reset the timer because START_TIMEOUT is not needed anymore

													//print_msg(msg_in);

													if(queue_length >= num_voters)
													{
														printf("No more space in the queue!");
														continue;//queue is full. This message will not trigger anything.
													}

													memcpy(msg_queue+(queue_pointer+queue_length)%num_voters, &msg_in, sizeof(struct voter_info_str));//enqueue, put the new message at the next avaiable position

													queue_length++;

													if(voting_flag==FALSE)//voting not started yet
													{
														voting_flag=TRUE;//voting started

														queue_length--;//reduce queue length by 1, use the last message, can be change to random picking here

														//poll for id


														voting_for = msg_queue[queue_pointer].id;
														//smart wait in order to give time for data ready
														usleep(((sprd/10)*f_max)/num_voters);
														build_message(&msg_out, POLL, SERVER_ID, vote_mode, voting_for, vote_round, vote_iteration, packet_loss_rate, sprd, epsl, bvn, data_size, decision_received, data_size);
														send_info(msock, (char *)&msg_out, sizeof(struct vote_msg_str), group, COMM_PORT);
														//printf("POLL message sent to %s on port %d for voter %d!\n", group, COMM_PORT, voting_for);


														if(voting_trace_flag == TRUE)
															voting_traces[voting_for][vote_iteration-1]=1;

														server_msg_count++; //server message counter increase by 1
														//total_msg_count++; //total message counter increased by 1

														queue_pointer=(queue_pointer+1)%num_voters;//move the queue_pointer by one position to the right

														dissent_timeout_flag=FALSE;
														//if(vote_mode!=EDEC)
														set_timer(TIMEOUT_VALUE_ITERATION,DISSENT_TIMEOUT);//2 seconds waiting for voting decisions of an iteration
														//this also reset the timer for start_timeout_flag because START_TIMEOUT is not needed anymore

														voter[msg_queue[queue_pointer].id].init=FALSE; //this voter can send more values for voting now.
													}
												}
											}
											else if(msg_in.type == POLL)
											{
												//printf("POLL message for iteration %d for voter %d received from voter %d!\n", msg_in.iteration, msg_in.value);
												//poll message received, ignore it.
											}
											else if(msg_in.type == VOTER_DECISION && msg_in.id < num_voters)
											{

												if(msg_in.iteration==vote_iteration && voter[msg_in.id].voted==FALSE)//new vote for current iteration
												{
													//collecting dropping information
													new_data_drops[msg_in.id] =msg_in.sprd;
													new_all_drops[msg_in.id] =msg_in.epsl;

													//printf("VOTER_DECISION for iteration %d from voter %d received with dd %d, da %d!", msg_in.iteration, msg_in.id, new_data_drops[msg_in.id], 	new_all_drops[msg_in.id] );
													voter[msg_in.id].voted=TRUE; //this voter can't vote anymore for this iteration

													if(msg_in.value==AGREE)
													{
														num_consent++;
														total_votes++;
														//printf("It's a consent message!\n");
													}
													else if(msg_in.value==DISAGREE)
													{
														num_dissent++;
														total_votes++;
														//printf("It's a dissent message!\n");
													}


													if(vote_mode == ICED && total_votes>= (float)(num_voters)/2)//disagreed
													{
														//printf("Disagreement reached for iteration %d for ICED.", vote_iteration);
													}
													else if(vote_mode == IDEC && total_votes > f_max)//agreed
													{
														//printf("Agreement reached for iteration %d for IDEC.", vote_iteration);
														agreed_flag=TRUE;
													}
													else if(vote_mode == EDEC && (num_consent > f_max || num_dissent >= (float)(num_voters)/2))
													{
														if(num_consent > f_max)
														{
															agreed_flag=TRUE;
															//printf("Agreement reached for iteration %d for EDEC. consents: %d dissents: %d\n", vote_iteration, num_consent, num_dissent);
														}
														else
														{
															//printf("Disagreement reached for iteration %d for EDEC. consents: %d dissents: %d\n", vote_iteration, num_consent, num_dissent);
															agreed_flag=FALSE;
														}
													}
													else
													{
														continue;//continue the voting
													}
													break;//end the current iteration
												}

											}

										}



										reset_timer();


										if(agreed_flag==TRUE)//An agreement has been reached after this iteration. Quit the whole test.
										{
											printf("After %d iteration(s) of voting, voting round %d has been finished!\n", vote_iteration, vote_round);
											//printf("There are totally %d votes received(%d consents, %d dissents) for voter %d.\n", total_votes, num_consent, num_dissent, voting_for);
											voting_history[(vote_round-1)%TRACE_SIZE]=voting_for;
											if(voting_for<bvn)
											{
												total_bad_decisions++;
												//printf("%4d th bad decision made on value from voter %d!\n", bad_decisions, voting_for);
											}

											////////////////////////////////////////////
											//         send DECISION message          //
											////////////////////////////////////////////

											//send a DECISION message
											//  msg_pointer, msg_type, voter_id, vote_mode, value, vote_iteration, sprd, epsl, bvn, history vector
											build_message(&msg_out, DECISION, SERVER_ID, vote_mode, 0, vote_round, vote_iteration, packet_loss_rate, sprd, epsl, bvn, data_size, NULL, data_size);
											send_info(msock, (char *)&msg_out, sizeof(struct vote_msg_str), group, COMM_PORT);
											//printf("DECISION message sent to %s on port %d!\n", group, COMM_PORT);

											server_msg_count++; //server message counter increase by 1
											//total_msg_count++; //total message counter increased by 1
											total_iterations++;
											total_decision_messages+=num_consent+num_dissent;

											break;
										}
										else if(start_timeout_flag == TRUE)
										{
											//printf("\nNo response received for the START message, restarting iteration without increasing the iteration counter.\n");
											printf("Iteration %d restarted!\n", vote_iteration);
											if(vote_iteration >= num_voters)
											{
												printf("Unlikely to reach an agreement since all voters have voted.");
												voting_history[(vote_round-1)%TRACE_SIZE]=-1;
												failed_rounds +=1;
												break;
											}
										}

										else//not yet, disagree or iteration timeout, continue.
										{
											vote_iteration++;//restart the voting
											total_iterations++;
											total_decision_messages+=num_consent+num_dissent;
											printf("\nIteration %d started!\n", vote_iteration);
										}



									}

									/*get the current time and put it into time_end*/
									time_end=current_time();
									//printf("Current time is: %ld:%ld\n", time_end.tv_sec, time_end.tv_usec);
									long time_elapsed=(time_end.tv_sec-time_start.tv_sec)*1000000+time_end.tv_usec-time_start.tv_usec;
									//printf("Time Elapsed is %ld micro-seconds.\n", time_elapsed);
									printf("Totally the server received %d messages and sent %d messages. %d messages are dropped\n", total_msg_count, server_msg_count, total_msg_dropped);

									//fprintf(fp_out, "round: %d iteration: %d total: %d server: %d dropped: %d time: %ld\n", vote_round, vote_iteration, total_msg_count, server_msg_count, total_msg_dropped, time_elapsed);
									printf("/************************************************/\n\n");

									total_delay+=time_elapsed;


									/*****************END OF A VOTING ROUND*******************************/

									///////////////////////
									//HISTORY VECTOR PART//
									///////////////////////


									//////////////////////////////////////////////////////////
									//        Send Request for History Vector               //
									//////////////////////////////////////////////////////////

									if(HV_ENABLED==TRUE && vote_round%HV_SIZE==0 && vote_mode == ICED)//only to be executed if history vector is enabled
									{

										//create a GIVEDE message, using the current round as the value
										build_message(&msg_out, GIVEDE, SERVER_ID, vote_mode, vote_round, vote_round, vote_iteration, packet_loss_rate, sprd, epsl, bvn, data_size, NULL, data_size);
										send_info(msock, (char *)&msg_out, sizeof(struct vote_msg_str), group, COMM_PORT);
										printf("GIVEDE message sent to %s on port %d for round %d!\n", group, COMM_PORT, vote_round);

										server_msg_count++; //server message counter increase by 1
										//total_msg_count++; //total message counter increased by 1
										//set ROUND_TIME_OUT timer
										round_timeout_flag = FALSE;
										set_timer(1000*1000, ROUND_TIMEOUT);



										//////////////////////////////////////////////////////////////////////
										//        Collect History Vector from each voter and evaluate       //
										//////////////////////////////////////////////////////////////////////

										//wait for messages until all voters has sent in the history vector or the round timeouts
										int hv_count=0;//total number of history vector's received for this round

										int * hv_received = new int[num_voters];//flags showing whether hv has been received from the voters
										memset(hv_received, 0,num_voters*sizeof(int));

										for(int i=0; i<num_voters; i++)
										{
											for(int j=0; j<HV_SIZE; j++)
											{
												history_vectors[i][j]=UNKNOWN;
											}
										}

										while(round_timeout_flag != TRUE && hv_count != num_voters)//not timeout or received hv from all voters.
										{
											memcpy (&rfds, &afds, sizeof (rfds));
											// check if there is any activity on any of the descriptors in rdfd
											int ret_count = select(nfds, &rfds, NULL, NULL, NULL);//see if there is any event
											if (ret_count < 0)
											{
												continue;//move on, nothing received yet.
											}

											//////////////////////////////////////////////////////
											//       receive one messsage to msg_in             //
											//////////////////////////////////////////////////////

											if (FD_ISSET (msock, &rfds))//multicast messages arriving
											{
												if(recv(msock,(char*)&msg_in, sizeof(msg_in),0)==sizeof(msg_in))
												{
													printf("One Multicast message received!\n");
													//print_msg(msg_in);
												}
												else
												{
													//printf("Multicast Package Receiving failed!\n");
													continue;
												}
											}
											if(msg_in.type == SENDDE)
											{
												total_hv_messages++;
											}

											if(msg_in.id != SERVER_ID)
												total_msg_count++; //total message counter increased by 1

											if((rand()%1000)<packet_loss_rate)//drop a packet with PACKET_LOSS_RATE/100 chance
											{

												printf("One message dropped! Loss Rate %d/1000. ", packet_loss_rate);
												print_msg_info(msg_in);
												total_msg_dropped++;
												continue;
											}

											if(msg_in.type == SENDDE && msg_in.round == vote_round && msg_in.id < num_voters && hv_received[msg_in.id] != TRUE)
											{
												//printf("SENDDE message received from voter %d.\n", msg_in.id);
												hv_received[msg_in.id]=TRUE;
												//total_hv_messages++;

												// for(int k=0; k<TRACE_SIZE; k++)
												//	 printf("%d ", msg_in.v_trace[k]);
												// printf("\n");

												update_history_vectors(history_vectors, msg_in.id, msg_in.v_trace, TRACE_SIZE);
												//print_history_vectors(history_vectors, num_voters, TRACE_SIZE);

												hv_count++;
											}

										}


										int error_count;
										int checked_count;//the number of decisions that can be proved correct or wrong by the current set of hv's

										if(hv_count>=bvn+1)
										{
											//check for the decisions made for the previous HV_SIZE(20) rounds
											//printf("Enough HV's received. Evaluating the history vector below....\n");
											//print_history_vectors(history_vectors, num_voters, HV_SIZE);

											//adjustment for possible error in history vector related to voter's own votes

											//printf("Voting history vector is{\n");
											//for(int i=0; i<HV_SIZE; i++)
											//printf("%2d", voting_history[i]);

											printf("\n");

											for(int j=0; j<HV_SIZE; j++)
											{
												if(voting_history[j]!=-1)//valid vote
													history_vectors[voting_history[j]][j]=1;
												else //unsuccessful round
												{
													for(int i=0; i<num_voters; i++)
														history_vectors[i][j]=-1;
												}

											}
											//printf("Finally the history vector is....\n");
											//print_history_vectors(history_vectors, num_voters, HV_SIZE);

											error_count = evaluate_history_vectors(history_vectors, num_voters, HV_SIZE, vote_round, bvn, &checked_count);
										}
										else
										{
											printf("Not enough HV's received. Starting Selective Solicitation....\n");
										}

										int trial_count=0;

										while(checked_count < TRACE_SIZE && hv_count < num_voters && trial_count < 10)//need to get more hv's until hv from everyone has been received
											//try at most 5 times in case the program is stuck.
										{
											trial_count++;
											//creat an HV_QUERY message
											print_array(hv_received);

											build_message(&msg_out, HV_QUERY, SERVER_ID, vote_mode, vote_round, vote_round, vote_iteration, packet_loss_rate, sprd, epsl, bvn, data_size, hv_received, data_size);
											send_info(msock, (char *)&msg_out, sizeof(struct vote_msg_str), group, COMM_PORT);
											printf("HV_QUERY message sent to %s on port %d for round %d!\n", group, COMM_PORT, vote_round);

											server_msg_count++; //server message counter increase by 1
											//total_msg_count++; //total message counter increased by 1


											//set ROUND_TIME_OUT timer to wait for response from all voters
											round_timeout_flag = FALSE;
											set_timer(1000*1000, ROUND_TIMEOUT);

											//wait for messages until all HV received

											while(round_timeout_flag != TRUE && hv_count != num_voters)//not timeout or received hv from all voters.
											{
												memcpy (&rfds, &afds, sizeof (rfds));
												// check if there is any activity on any of the descriptors in rdfd
												int ret_count = select(nfds, &rfds, NULL, NULL, NULL);//see if there is any event
												if (ret_count < 0)
												{
													continue;//move on, nothing received yet.
												}

												//////////////////////////////////////////////////////
												//       receive one messsage to msg_in             //
												//////////////////////////////////////////////////////

												if (FD_ISSET (msock, &rfds))//multicast messages arriving
												{
													if(recv(msock,(char*)&msg_in, sizeof(msg_in),0)==sizeof(msg_in))
													{
														//printf("One Multicast message received!\n");
														//print_msg(msg_in);
													}
													else
													{
														printf("Multicast Package Receiving failed!\n");
														continue;
													}
												}

												if(msg_in.id != SERVER_ID)
													total_msg_count++; //total message counter increased by 1


												////////////////////////////////////////////////////////////////////
												//   Drop a message with a preset possibility: PACKET_LOSS_RATE   //
												////////////////////////////////////////////////////////////////////


												if(msg_in.type == HV_REPLY)
												{
													total_hv_messages++;
												}

												if((rand()%1000)<packet_loss_rate)//drop a packet with PACKET_LOSS_RATE/100 chance
												{

													printf("One message dropped! Loss Rate %d/1000. ", packet_loss_rate);
													print_msg_info(msg_in);
													total_msg_dropped++;
													continue;
												}




												if(msg_in.type == HV_REPLY && msg_in.round == vote_round && msg_in.id < num_voters && hv_received[msg_in.id] != TRUE)
												{

													//printf("HV_REPLY message received from voter %d.\n", msg_in.id);
													hv_received[msg_in.id]=TRUE;

													// for(int k=0; k<TRACE_SIZE; k++)
													//	 printf("%d ", msg_in.v_trace[k]);
													// printf("\n");

													update_history_vectors(history_vectors, msg_in.id, msg_in.v_trace, TRACE_SIZE);
													//print_history_vectors(history_vectors, num_voters, TRACE_SIZE);

													hv_count++;
													//printf("%d hv received.\n", hv_count);
												}

											}

											//check for the decisions made for the previous HV_SIZE(20) rounds
											printf("Evaluating the history vector below....\n");
											//print_history_vectors(history_vectors, num_voters, HV_SIZE);

											//adjustment for possible error in history vector related to voter's own votes

											printf("Voting history vector is\n");
											for(int i=0; i<HV_SIZE; i++)
												printf("%2d", voting_history[i]);

											printf("\n");

											for(int j=0; j<HV_SIZE; j++)
											{
												if(voting_history[j]!=-1)//valid vote
													history_vectors[voting_history[j]][j]=1;
												else //unsuccessful round
												{
													for(int i=0; i<num_voters; i++)
														history_vectors[i][j]=-1;
												}

											}
											printf("Finally the history vector is....\n");
											print_history_vectors(history_vectors, num_voters, HV_SIZE);
											error_count = evaluate_history_vectors(history_vectors, num_voters, HV_SIZE, vote_round, f_max, &checked_count);//here i used f_max instead bvn because that's all the manager can do
										}


										//now, all hv received or all decisions have been proved correct or wrong or can't be proved to be wrong or correct
										corrected_decisions += error_count+(TRACE_SIZE-checked_count);
										printf("Totally %d decision errors are found after round %d!\n", error_count+(TRACE_SIZE-checked_count), vote_round);
										total_errors += error_count+(TRACE_SIZE-checked_count);
										delete hv_received;

									}//end of the if statement to see if hv is supported and 20 rounds has passed

									total_messages+=total_msg_count+server_msg_count;


								}//end of the for loop for voting rounds.


							}//iter, repeat iter times for one set of parameters, calculate the average

							data_drops=0;
							all_drops=0;

							for(int i=0; i<num_voters; i++)
							{
								data_drops += new_data_drops[i]-old_data_drops[i];
								old_data_drops[i]=new_data_drops[i];
								all_drops += new_all_drops[i]-old_all_drops[i];
								old_all_drops[i]=new_all_drops[i];
							}


							//output the result got for one set of parameters





							//printf("Total errors: %ld\n", total_errors);
							if((fp_out=fopen("result.csv", "a"))==NULL)/*output file open failed*/
							{
								printf("Output file opening failed\n");
								exit(0);
							}

							if(vote_mode==ICED)//iced
							{
								fprintf(fp_out, "ICED:%3d Loss:%3.1f%% sprd:%2d fm:%2d bvn:%2d ttc:%8ld m:%5.2f dm:%5.2f bad: %5.2f rbad: %5.3f hv:%6.2f ratio:%5.3f size:%12d w:%4ld t:%4ld c:%4ld\n", test_id, packet_loss_rate/10.0, sprd/1000, f_max, bvn, total_delay/(total_number_rounds*redo), (float)total_messages/(total_number_rounds*redo), (float)total_data_messages/(total_number_rounds*redo), (float)total_errors/redo, (float)total_bad_decisions/redo, (float)total_hv_messages/(total_number_rounds*redo), (float)total_hv_messages/total_messages, data_size, wait, wait_until-cur_time.tv_sec, cur_time.tv_sec-starting_point);
								if((float)total_errors/redo > THRESHOLD_TO_EDEC)
									vote_mode = EDEC;
							}
							else
							{
								float lrate = (float)total_decision_messages/(total_iterations*(num_voters-1));
								if(power_save_enabled)
								{

 printf("EDEC,N,Loss,sprd,fm,bvn,ttc,m,dm,fail,rrate,pl,ratio,dd,ad,w,t,c: iteration:%3d rounds:%3d\n",total_iterations,total_number_rounds);



 printf("%3d,%2d,%3d,%3.1f%%,%2d,%2d,%2d,%8ld,%5.2f,%5.2f,%5.2f,%5.3f,%6d,%5.2f,%5.2f,%5.2f,%4ld,%4ld,%4ld\n", test_id,num_voters, total_iterations,packet_loss_rate/10.0, sprd/1000, f_max, bvn, total_delay/(total_number_rounds*redo), (float)total_messages/(total_number_rounds*redo), (float)total_data_messages/(total_number_rounds*redo), (float)failed_rounds/redo, lrate, data_size, (float)((sprd*WAKEUP_POINT+COMP_DELAY)*100)/(total_delay/(total_number_rounds*redo)), (float)data_drops/(total_number_rounds*redo), (float)all_drops/(total_number_rounds*redo), wait, wait_until-cur_time.tv_sec, cur_time.tv_sec-starting_point);
 
fprintf(fp_out, "%3d,%2d,%3d,%3.1f%%,%2d,%2d,%2d,%8ld,%5.2f,%5.2f,%5.2f,%5.3f,%6d,%5.2f,%5.2f,%5.2f,%4ld,%4ld,%4ld\n", test_id,num_voters, total_iterations,packet_loss_rate/10.0, sprd/1000, f_max, bvn, total_delay/(total_number_rounds*redo), (float)total_messages/(total_number_rounds*redo), (float)total_data_messages/(total_number_rounds*redo), (float)failed_rounds/redo, lrate, data_size, (float)((sprd*WAKEUP_POINT+COMP_DELAY)*100)/(total_delay/(total_number_rounds*redo)), (float)data_drops/(total_number_rounds*redo), (float)all_drops/(total_number_rounds*redo), wait, wait_until-cur_time.tv_sec, cur_time.tv_sec-starting_point);



								}
								else
								{

 printf("EDEC,N,Loss,sprd,fm,bvn,ttc,m,dm,fail,rrate,pl,ratio,dd,ad,w,t,c: iteration:%3d rounds:%3d\n",total_iterations,total_number_rounds);

printf("%3d,%2d,%3d,%3.1f%%,%2d,%2d,%2d,%8ld,%5.2f,%5.2f,%5.2f,%5.3f,%6d,%5.2f,%5.2f,%5.2f,%4ld,%4ld,%4ld\n", test_id,num_voters, total_iterations,packet_loss_rate/10.0, sprd/1000, f_max, bvn, total_delay/(total_number_rounds*redo), (float)total_messages/(total_number_rounds*redo), (float)total_data_messages/(total_number_rounds*redo), (float)failed_rounds/redo, lrate, data_size, (float)((sprd+COMP_DELAY)*100)/(total_delay/(total_number_rounds*redo)), (float)data_drops/(total_number_rounds*redo), (float)all_drops/(total_number_rounds*redo), wait, wait_until-cur_time.tv_sec, cur_time.tv_sec-starting_point);

fprintf(fp_out, "%3d,%2d,%3d,%3.1f%%,%2d,%2d,%2d,%8ld,%5.2f,%5.2f,%5.2f,%5.3f,%6d,%5.2f,%5.2f,%5.2f,%4ld,%4ld,%4ld\n", test_id,num_voters,total_iterations, packet_loss_rate/10.0, sprd/1000, f_max, bvn, total_delay/(total_number_rounds*redo), (float)total_messages/(total_number_rounds*redo), (float)total_data_messages/(total_number_rounds*redo), (float)failed_rounds/redo, lrate, data_size, (float)((sprd+COMP_DELAY)*100)/(total_delay/(total_number_rounds*redo)), (float)data_drops/(total_number_rounds*redo), (float)all_drops/(total_number_rounds*redo), wait, wait_until-cur_time.tv_sec, cur_time.tv_sec-starting_point);

								}
								if( lrate> THRESHOLD_TO_ICED)
									vote_mode = ICED;
							}

							fclose(fp_out);



							if(test_id % RANGE == 0 ) //flip the direction
							{
								if(ascending_flag == FALSE)
									ascending_flag = TRUE;
								else if( ascending_flag == TRUE)
									ascending_flag = FALSE;
							}

							if(ascending_flag == FALSE)
								packet_loss_rate -= 5;
							else if( ascending_flag == TRUE)
								packet_loss_rate += 5;

							if(packet_loss_rate < 0)
							{
								packet_loss_rate =0;

							}

							test_id++;
							if(test_id>1000) goto END;

						}

					}//bvn, changing the number of bad voters

				}//f_max, changing f_max

			}//spread, chaging spread

		}//data packet size



	}//packet_loss_rate, chaging packet loss rate


END:


	//cleaning up
	//printf("Test ended! Results for each round are saved in file result.txt\n");
	//printf("BAD=%d, TOTAL=%d. The success rate is %.2f%%\n", bad_decisions, total_number_rounds, 100-100*((float)bad_decisions/total_number_rounds));
	//printf("Average decision time is %ld microseconds\n", total_delay/total_number_rounds);
	//printf("Totally %d decisions were corrected by history vectors for this test!\n", corrected_decisions);

	close(msock);
	close(udp_sock);

	return 0;
}




////////////////////////////
//    utility functions   //
////////////////////////////

int evaluate_voting_traces(int (* v_traces)[TRACE_SIZE], int num_voters, int iteration)
{
	int i,j;
	int count;
	for(i=0; i<iteration;i++)
	{
		count=0;
		for(j=0;j<num_voters;j++)
			count+=v_traces[j][i];//1 is yes, 0 is no
		if(count>(num_voters/2))
		{
			printf("An agreement found from the voting trace in iteration %d", i);
			return 1;
		}
	}
	return 0;
}

int print_voting_traces(int (* v_traces)[TRACE_SIZE], int num_voters, int iteration)
{
	int i, j;
	for(i=0; i < num_voters; i++)
	{
		for(j=0; j < iteration; j++)
			printf("%d ", v_traces[i][j]);
		printf("\n");
	}
	return 1;
}

int update_voting_traces(int (* voting_traces)[TRACE_SIZE], int id, int v_trace[TRACE_SIZE], int iteration)
{
	int i;
	for(i=0; i < iteration; i++)
	{
		if(v_trace[i]==1)
		{
			voting_traces[id][i]=v_trace[i];
		}
		else if(v_trace[i]==0)
		{
			//do nothing
		}
		else
		{
			//printf("illegal voting value %d found from voter %d's voting trace element %d!\n", v_trace[i], id, i);

			//return 0;
		}
	}
	return 1;
}

int update_history_vectors(int (* history_vectors)[TRACE_SIZE], int id, int v_trace[TRACE_SIZE], int size)
{
	int i;
	for(i=0; i < size; i++)
	{
		if(v_trace[i]==1)
		{
			history_vectors[id][i]=1;
		}
		else if(v_trace[i]==0)
		{
			history_vectors[id][i]=0;
			//do nothing
		}
		else
		{
			//printf("illegal voting value %d found from voter %d's voting trace element %d!\n", v_trace[i], id, i);

			//return 0;
		}
	}
	return 1;
}

int print_history_vectors(int (* history_vectors)[TRACE_SIZE], int num_voters, int size)
{
	int i, j;
	for(i=0; i < num_voters; i++)
	{
		for(j=0; j < size; j++)
			printf("%2d ", history_vectors[i][j]);
		printf("\n");
	}
	return 1;
}

int evaluate_history_vectors(int (* history_vectors)[TRACE_SIZE], int num_voters, int size, int round, int f_max, int * checked_count)
{
	int i, j, current_round=round, bad_count=0, good_count=0, error_count=0, correct_count=0;

	for(j=0; j<size; j++)
	{
		bad_count=0;
		good_count=0;

		current_round = round-size+1+j;

		for(i=0; i<num_voters; i++)
		{
			if(history_vectors[i][j]==0)
			{
				bad_count++;//one dissent
			}
			else if(history_vectors[i][j]==1)
			{
				good_count++;//one consent
			}
		}
		if(bad_count >= (num_voters-f_max))
		{
			error_count++;
			//printf("The decision at round %d is corrected by the history vectors received!\n", current_round);
		}
		else if(good_count > f_max)//at least one good voter consent on that decision
		{
			correct_count++;
			//printf("The decision at round %d is validated by the history vectors received!\n", current_round);
		}
		else
		{
			//printf("The decision at round %d can't be validated by the history vectors received!\n", current_round);
		}
	}

	*checked_count = error_count + correct_count;//the number of records that has been successful checked, proved correct or wrong
	return error_count;//return errors corrected.
}








