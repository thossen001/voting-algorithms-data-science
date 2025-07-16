/*********************************************/

/*         VOTING CLIENT PROGRAM v5.2        */

/*           Selective Solicitation          */

/*    with switching data message sizes      */

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









#define POWER_SAVE_ENABLED FALSE	 //wether power save mode is enabled or not





/********************************************/

/*             Function Declarations        */

/********************************************/





/********************************************/

/*             Global Variables             */

/********************************************/







/********************************************/

/*             Main function                */

/********************************************/



 main(int argc, char * argv[])

{



	struct vote_msg_str msg_out, msg_in;//clients's messages

	char local_host[16]="127.0.0.1";//loopback ip address

	char group[16]="224.12.12.0";//multicast group id



	int voter_id=0; //the voter's id

	int num_voters=3;//default number of voters.

	int vote_mode=ICED;//default voting mode



	int sprd=100;//default calculation time.

	int epsl=50;//default epsilon value

	int bvn=2;//default number of bad voters



	int voter_status;//GOOD or BAD

	int voter_value; //GOOD_VALUE or BAD_VALUE



	int udp_port;//control port



	int vote_round=UNKNOWN;//the current round of voting, start with -1, unknown

	int vote_iteration=UNKNOWN;//the current iteration of voting, start with -1, unknown



	int voter_msg_count=0;//messages sent out by this voter

	int total_msg_count=0;//messages received by this voter



	int silent_flag=FALSE;



	int voting_flag=FALSE; //flag showing whether it's now during a voting process

	int received_flag=FALSE; //flag showing whether there are voter values received

	int epsl_timer_started_flag=FALSE; //flag showing whether the epsl timer has been started.





	int value_voted_flag=FALSE; //For a single voting round, each voter's value can be voted upon at most once. This flag shows for the current round, whether the voter's value has been voted upon.



	int v_trace[TRACE_SIZE];



	int my_vote=UNKNOWN; //three possible values, AGREE or DISAGREE or UNKNOWN



	int packet_loss_rate=0; //default packet loss rate. Is going to be reset by every new message received from the server



	int jump_start=FALSE; //flag showing whether a jump start is necessary since START messages can be lost for some rounds.



	int data_size = 0; //default payload size



	int sleeping_flag = FALSE;



	sleep_timeout_flag = FALSE;



	int power_save_mode_enabled=0;



	int data_msg_dropped = 0;	//data message dropped due to sleeping



	int all_msg_dropped = 0;    //all messages dropped due to sleeping



	double fault_sev=0.0;
	double random_r;
	double fsev=0.0;


	/////////////////////////////////////////////

	//    read command line arguments          //

	/////////////////////////////////////////////

	if(argc!=5)

 	{

		printf("Usage: ./client num_voters voter_id powersave_mode_enabled(0 to disable) fault_severity\n");

		exit(0);

	}

	else

	{

		sscanf(argv[1], "%d", &num_voters);

		sscanf(argv[2], "%d", &voter_id);

		sscanf(argv[3], "%d", &power_save_mode_enabled);

		sscanf(argv[4], "%lf", &fsev);

		if(voter_id >= num_voters || voter_id < 0 || num_voters <=0)

		{

			printf("Illegal input, the voter id should be greater than -1 and less than the total number of voters. Total number of voters should be greater than 0.\n");

			exit(0);

		}

	}


	fault_sev=fsev;
	srand(time(0)+voter_id);//seed the PRNG for the first time



	signal (SIGALRM, catch_alarm);



	////////////////////////////////////////

	// Data structure for history vector  //

	////////////////////////////////////////



	struct hv_struct hv;





	//////////////////////////////////////

	//       Input the voter id         //

	//////////////////////////////////////

	/*do

	{

		printf("Input the voter id:");

		scanf("%d", &voter_id);

	}while(voter_id<0 || voter_id>=num_voters);*/



	if(power_save_mode_enabled==1)

	{

		printf("Voter %d among the %d voters started with powersave mode turned on !\n",voter_id, num_voters);

	}

	else

	{

		printf("Voter %d among the %d voters started with powersave mode turned off !\n",voter_id, num_voters);

	}



	udp_port=UDP_PORT+10+voter_id;//use a different port for each different voter





	///////////////////////////////////////////////////

	//  Data Structure storing the voter information //

	///////////////////////////////////////////////////



	struct voter_info_str * voter;

	voter = new struct voter_info_str[num_voters];//data structure to store voter infomation



	for(int i=0; i< num_voters; i++)

	{

		voter[i].init=FALSE; //not initiated yet

		voter[i].voted=FALSE; //not voted

		voter[i].value=UNKNOWN; //unknown value

	}





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

  	udp_sock=CreateUdpSocket(udp_port);//use the voter specific port



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

    /*  Socket preparation ended         */

    /*************************************/

	int started_flag=FALSE;



    while(1)

    {

		if(epsl_timer_started_flag == FALSE && data_ready_flag == TRUE && (vote_iteration==1 || (vote_iteration!=1 && jump_start == TRUE)))//set epsl timer during the first iteration when the voter data is ready.

		{

			sleeping_flag = FALSE;

			clear_to_send_flag=FALSE;

			set_timer((int)((1-WAKEUP_POINT)*sprd)+rand()%epsl, CLEAR_TO_SEND); //wait for only epsl milliseconds

			epsl_timer_started_flag=TRUE;//epsl timer started, data_ready_flag will not be checked again

			if(jump_start==TRUE) jump_start=FALSE;

		}



		if(epsl_timer_started_flag == TRUE && clear_to_send_flag==TRUE)//ready to send the message out

		{

			//epsl_timer_started_flag =FALSE;//epsl timer ended.

			if( received_flag == FALSE && value_voted_flag == FALSE)//if values from other voters are not seen and this voter hasn't participated in any vote before.

			{

				//send a VOTER_VALUE message

				//  msg_pointer, msg_type, voter_id, vote_mode, value=0, vote_iteration, sprd, epsl, bvn, history vector

				build_message(&msg_out, VOTER_VALUE, voter_id, vote_mode, voter_value, vote_round, vote_iteration, packet_loss_rate, data_msg_dropped, all_msg_dropped, bvn, data_size, v_trace, data_size);

				send_data_msg(msock, (char *)&msg_out, sizeof(struct vote_msg_str), data_size, group, COMM_PORT);//now call this function for sending data messages

				printf("VOTER_VALUE message sent to %s on port %d for voter %d!\n", group, COMM_PORT, voter_id);





				voter_msg_count++; //server message counter increase by 1



				voter[voter_id].init=TRUE; //this voter has proposed a value

				voter[voter_id].value=voter_value; //this voter's propsed value



			}



			clear_to_send_flag=FALSE;//only one VOTER_VALUE need to be sent for each iteration. sent already. so, disable the flag

		}



		if(sleep_timeout_flag == TRUE )//send decison when computation time is expired.

		{

			sleep_timeout_flag = FALSE;

			sleeping_flag = FALSE;

			send_unicast_packet(udp_sock, (char *)&msg_out, sizeof(struct vote_msg_str), group, UDP_PORT);

			printf("VOTER_DECISION message sent to %s on port %d from voter %d!\n", group, COMM_PORT, voter_id);

			voter_msg_count++; //server message counter increase by 1

		}





        memcpy (&rfds, &afds, sizeof (rfds));

        // check if there is any activity on any of the descriptors in rdfd

        int ret_count = select(nfds, &rfds, NULL, NULL, NULL);//test if there is any event

        if (ret_count < 0)

        {

          	continue;//move on, nothing received yet.

        }



        /*



        if(data_ready_flag==FALSE)

        {

			check_timer();

		}



		*/



		if (FD_ISSET (msock, &rfds))//multicast messages arriving

		{

			int bytes_received = recv(msock,(char*)&msg_in, sizeof(msg_in),0);

			if(bytes_received == sizeof(msg_in))

			{

				//printf("One Multicast message received by voter %d!\n", voter_id);

				//print_msg(msg_in);

			}

			else if(bytes_received == sizeof(msg_in)+data_size)

			{

				//printf("Data message received!\n");

			}

			else

			{

				printf("Multicast Package Receiving failed!\n");

				continue;

			}

		}





		if(msg_in.id == SERVER_ID)

		{

			packet_loss_rate=msg_in.loss_rate; //update to the recent packet loss rate

			data_size = msg_in.size; //payload size determined by the server

		}









		total_msg_count++;//the number of messages received by the voter increased by 1



		////////////////////////////////////////////////////////////////////

		//   Drop a message with a preset possibility: PACKET_LOSS_RATE   //

		////////////////////////////////////////////////////////////////////



		if((rand()%1000)<packet_loss_rate)//drop a packet with PACKET_LOSS_RATE/100 chance

		{

			printf("One message dropped with Loss rate %d/1000", packet_loss_rate);

			print_msg_info(msg_in);

			continue;

		}

		else

		{

			//printf("One message recieved.");

			//print_msg_info(msg_in);

		}



		//drop out-of-date messages

		if(vote_round!=UNKNOWN && vote_iteration != UNKNOWN)

		{

			if(msg_in.type != HV_QUERY)

			{

				if(msg_in.type !=START && msg_in.round != vote_round)

				{

					//printf("A packet belongs to other round discarded!\n");

						continue;

					}

				else if(msg_in.type != START && msg_in.iteration != vote_iteration)

				{

					//printf("A packet belongs to other iteration discarded!\n");

						continue;}

			}



		}



		//drop message due to sleep



		if(sleeping_flag == TRUE && power_save_mode_enabled == TRUE)

		{

			printf("One message dropped because the voter is sleeping");

			print_msg_info(msg_in);

			if(msg_in.type == VOTER_VALUE)

			{

				data_msg_dropped ++;

			}



			if(msg_in.type != VOTER_DECISION)

			{

				all_msg_dropped ++;

			}

			continue;



		}



		////////////////////////////////////////////////////////////

		// Do different jobs according to the type of the message //

		////////////////////////////////////////////////////////////



		if(msg_in.type == START && msg_in.id == SERVER_ID)//only the server is allowed to send START message

		{



			printf("\n\n* * * * * * * * * * * * * * * * * *\n\n");



			printf("START message for iteration %d of round %d received.\n", msg_in.iteration, msg_in.round);



			started_flag=TRUE;



			vote_mode=msg_in.mode;



			//setting up parameters for the pending writing and voting

			voting_flag   = FALSE;//voting not started yet

			received_flag = FALSE;//There are no VOTER_VALUE messages received.



			silent_flag = FALSE;





			vote_iteration  = msg_in.iteration;



			jump_start = FALSE;



			if(vote_iteration !=1 && msg_in.round != vote_round)

				jump_start = TRUE;


			//printf("The previous round is %d\n", vote_round);


			int round_base;

			if(msg_in.round%TRACE_SIZE!=0)

			{

				round_base = msg_in.round-msg_in.round%TRACE_SIZE;//the closest multiple of TRACE_SIZE

			}

			else

			{

				round_base = msg_in.round-TRACE_SIZE;

			}


			printf("Iteration %d of round %d started!\n", msg_in.iteration, msg_in.round);



			if(vote_iteration == 1 || (vote_iteration!=1 && jump_start == TRUE))//iteration 1 in a voting, need to set the data ready timer first, then epsl timer

			{

				if(vote_round != msg_in.round-1 && msg_in.round!=1)

					printf("WARNING! Some rounds might be skipped due to message loss!\n");



				if(vote_round != msg_in.round-1 && msg_in.round ==1)

				{

					//the start of a new round 1.

					printf("Starting of a new test.\n");



					reset_hv(&hv, 0);

					printf("History Vector Reset case 1 .\n");

					//print_hv(hv);



				}

				else if(vote_round > msg_in.round)

				{

					 printf("Starting of a new test. However, some previous rounds in this test might have been missed.\n");

					 reset_hv(&hv, round_base);

					printf("History Vector Reset case 2 .\n");

				}

				//else if(vote_round == msg_in.round-1 && (msg_in.round-1) % TRACE_SIZE == 0)//vector not useful anymore after every 20 rounds.

				else if(vote_round == msg_in.round-1 && (msg_in.round-1) % TRACE_SIZE == 0)//vector not useful anymore after every 20 rounds.

				{

					reset_hv(&hv, msg_in.round-1);
					printf("History Vector Reset case 3.\n");
					//print_hv(hv);


				}

				else if(vote_round < msg_in.round-1 && round_base>=vote_round)//history vector sent and some rounds skipped..

				{

					reset_hv(&hv, round_base);

					printf("History Vector Reset case 4.\n");

					//print_hv(hv);

				}



				my_vote=UNKNOWN;//initialize the last vote status to unknown



				vote_round = msg_in.round;



				srand(time(0)+voter_id+vote_round);//seed the PRNG



				value_voted_flag=FALSE;//reset this flag to false at iteration 1.



				memset(v_trace, 0, TRACE_SIZE*sizeof(int));//clear the vote trace;



				vote_mode=msg_in.mode;

				//printf("Vote mode is %d.\n", vote_mode);

				bvn=msg_in.bvn;//bad voter number


				if(bvn>0)
				{

					if(voter_id < bvn)//This voter is going to be a bad voter

					{
						random_r=((double)rand()/(RAND_MAX));
						if(random_r<=fault_sev)
						{					

							printf("I am going to be a bad voter for this vote!\n");

							voter_status=BAD;//good voter, 0

							voter_value=BAD_VALUE;//good voter value, -73
						}
						else

						{

							printf("I am going to be a good voter for this vote!\n");

							voter_status=GOOD;//bad voter, 1

							voter_value=GOOD_VALUE;//bad voter value, 23

						}

					}
						else

						{

							printf("I am going to be a good voter for this vote!\n");

							voter_status=GOOD;//bad voter, 1

							voter_value=GOOD_VALUE;//bad voter value, 23

						}
				}

				else

				{

					printf("I am going to be a good voter for this vote!\n");

					voter_status=GOOD;//bad voter, 1

					voter_value=GOOD_VALUE;//bad voter value, 23

				}



				epsl = msg_in.epsl;//waiting time

				sprd = msg_in.sprd;//computation time



				for(int i=0; i < num_voters; i++)//cleaning up the voter information data structure for a new vote

				{

					voter[i].init  = FALSE;

					voter[i].value = UNKNOWN;

				}



				data_ready_flag = FALSE;//data not ready for this vote yet.

				clear_to_send_flag = FALSE;//no message to send

				epsl_timer_started_flag =FALSE;//epsl not started yet.



				//srand(time(0)+voter_id);//seed the PRNG




				//set_timer((int)(WAKEUP_POINT*sprd)+(int)(0.2*(rand()%sprd)), DATA_READY);//wait for epsl plus sprd milliseconds
				set_timer((int)(WAKEUP_POINT*sprd)+(int)(0.2*(rand()%epsl)), DATA_READY);//wait for epsl plus sprd milliseconds

				sleeping_flag = TRUE;



			}

			else

			{

				//srand(time(0)+voter_id+vote_round+vote_iteration);//seed the PRNG

				//printf("epsl_flag is %d, value_voted_flag is %d, data_ready_flag is %d\n", epsl_timer_started_flag, value_voted_flag, data_ready_flag);

				if(value_voted_flag == FALSE && data_ready_flag == TRUE)//for later iterations, if no value has been sent yet, only setting up the epsl timer

				{



					epsl = msg_in.epsl;//waiting time

				    sprd = msg_in.sprd;//computation time



					clear_to_send_flag=FALSE;



					//epsl = msg_in.epsl;//waiting time before sending
				        int random = rand();
					int wait =	(int)((1-WAKEUP_POINT)*sprd)+random%epsl;
			                printf("sprd %d, epsl %d , random %d,wait  %d\n", sprd,epsl,random,wait);

					set_timer(wait, CLEAR_TO_SEND); //wait for only epsl milliseconds



					epsl_timer_started_flag = TRUE;//epsl started.



				}

				else

				{

					printf("value_voted_flag is %d, data_ready_flag is %d\n", value_voted_flag, data_ready_flag);
                                        printf("voters id is %d ",voter_id);

					if(value_voted_flag == FALSE && check_timer()==0)

					{

						epsl = msg_in.epsl;//waiting time

				        sprd = msg_in.sprd;//computation time

						sleeping_flag = FALSE;



						sleep_timeout_flag = FALSE;



						clear_to_send_flag=FALSE;



						data_ready_flag=TRUE; //the timer actually expired in this case.



						//epsl = msg_in.epsl;//waiting time before sending



						set_timer((int)((1-WAKEUP_POINT)*sprd)+rand()%epsl, CLEAR_TO_SEND); //wait for only epsl milliseconds



						epsl_timer_started_flag = TRUE;//epsl started.

					}



				}



			}



		}

		else if(msg_in.type == POLL && msg_in.id == SERVER_ID)

		{

			//printf("POLL message for iteration %d from voter %d received!", msg_in.iteration, msg_in.id);

			//printf("%d, %d\n", msg_in.value, started_flag);

			if(msg_in.id == SERVER_ID&&msg_in.value<num_voters && msg_in.iteration == vote_iteration && started_flag == TRUE && msg_in.v_trace[voter_id]==FALSE)//id should be less than number of voters

			{



				//poll for id, msg_in.value stores the id to vote on.

				int id_to_vote=msg_in.value;



				voting_flag=TRUE;//voting started



				if(id_to_vote != voter_id)//response to poll for other voters

				{

					printf("POLL message for iteration %d for voter %d received!\n", msg_in.iteration, id_to_vote);





					int send_flag=FALSE;//don't send by default



					///////////////////////////////////////////

					//            Decision Making            //

					///////////////////////////////////////////

					if(check_timer()==0 && data_ready_flag==FALSE)//dealing with the timer bug that the timer never send an alarm before it expires

					{

						data_ready_flag=TRUE;

					}



					if( voter_status == BAD )//if the voter is a bad voter, it can make a decision simply using the id of the voter that is put to vote

					{

						if(id_to_vote < msg_in.bvn)//the id belongs to a bad voter

						{

							my_vote = AGREE;

						}

						else

						{

							my_vote = DISAGREE;

						}

					}

					else //play as a good voter

					{

						if(data_ready_flag==FALSE)//local data not ready

						{

							printf("Local data not ready!\n");

							my_vote=DISAGREE;

						}

						else if(voter[id_to_vote].init == FALSE)//value of the voter not received yet.

						{

							printf("Value of voter %d not on file.");

							my_vote=DISAGREE;

						}

						else

						{

							if(voter_value == voter[id_to_vote].value)//values match

								my_vote=AGREE;

							else//values don't match

								my_vote=DISAGREE;

						}

					}



					if(my_vote==AGREE)

					{

						printf("My decision on this poll is AGREE!\n");



						v_trace[vote_iteration-1]=1;



						update_hv(&hv, vote_round, 1);

					}

					else if(my_vote==DISAGREE)

					{

						printf("My decision on this poll is DISAGREE!\n");



						update_hv(&hv, vote_round, 0);

					}

					else

					{

						printf("My decision on this poll is UNKNOWN!\n");



						update_hv(&hv, vote_round, -1);

					}



					///////////////////////////////////////////

					//             Send or not               //

					///////////////////////////////////////////



					if(vote_mode == ICED && my_vote== DISAGREE )//processing for ICED mode

					{

						send_flag=TRUE;

					}

					else if(vote_mode == IDEC && my_vote== AGREE )

					{

						send_flag=TRUE;

					}

					else if(vote_mode == EDEC && my_vote != UNKNOWN)

					{

						send_flag=TRUE;

					}

					else

					{

						send_flag=FALSE;

					}



					///////////////////////////////////////////

					//             Send the vote             //

					///////////////////////////////////////////

					if(send_flag==TRUE)

					{

							//send a VOTER_DECISION message

							//  msg_pointer, msg_type, voter_id, vote_mode, value=0, vote_iteration, sprd, epsl, bvn, history vector

							build_message(&msg_out, VOTER_DECISION, voter_id, vote_mode, my_vote, vote_round, vote_iteration, packet_loss_rate, data_msg_dropped, all_msg_dropped, bvn, data_size, v_trace, data_size);

							if(voter_status==GOOD)

							{

								sleeping_flag = TRUE;

								sleep_timeout_flag = FALSE;

								set_timer(COMP_DELAY, SLEEP_TIMEOUT);

							}

							else //bad voter don't compute

							{

								send_unicast_packet(udp_sock, (char *)&msg_out, sizeof(struct vote_msg_str), group, UDP_PORT);

								printf("VOTER_DECISION message sent to %s on port %d from voter %d!\n", group, UDP_PORT, voter_id);

								voter_msg_count++; //server message counter increase by 1



							}//delay before sending if it's a good voter

								//usleep(COMP_DELAY);}

							//send_info(msock, (char *)&msg_out, sizeof(struct vote_msg_str), group, COMM_PORT);

							//printf("VOTER_DECISION message sent to %s on port %d from voter %d!\n", group, COMM_PORT, voter_id);

							//voter_msg_count++; //server message counter increase by 1

					}



				}

				else //ignore polls for this voter itself

				{

					printf("POLL message for MY value for iteration %d received!\n", msg_in.iteration);

					v_trace[vote_iteration-1] = 1;//update the voting trace.

					value_voted_flag = TRUE;// this voter's values has been voted upon.



					my_vote=AGREE;//agree by default



					//print_msg(msg_in);

				}



					update_hv(&hv, vote_round, my_vote);//upgrade the hv

			}



		}

		else if(msg_in.type == VOTER_VALUE && msg_in.id<num_voters && msg_in.iteration == vote_iteration && started_flag == TRUE)//id should be less than number of voters

		{

			received_flag=TRUE;//There are VOTER_VALUE messages received.





			if(msg_in.id != voter_id)//record VOTER_VALUE message from other voters

			{

				printf("VOTER_VALUE message for iteration %d for voter %d received!\n", msg_in.iteration, msg_in.id);

				voter[msg_in.id].init = TRUE; //proposed

				voter[msg_in.id].value = msg_in.value; //value proposed by the voter



			}

			else //ignore polls for this voter itself

			{

				printf("My VOTER_VALUE message for iteration %d received!\n", msg_in.iteration);

				//print_msg(msg_in);

			}

		}

		else if(msg_in.type == DECISION && msg_in.id== SERVER_ID)//has to be the server

		{

			printf("Decision for vote round %d has been reached after %d iterations. DD %d, AD %d\n", msg_in.round, vote_iteration, data_msg_dropped, all_msg_dropped);

			print_hv(hv);

			started_flag=FALSE;

		}

		else if(msg_in.type == GIVEDE && msg_in.id== SERVER_ID)//has to be the server

		{

			printf("GIVEDE received from the server asking for history vector upto round %d.\n", msg_in.value);



			int hv_buffer[TRACE_SIZE];

			read_hv(hv, msg_in.value, hv_buffer, TRACE_SIZE);





			//reset_hv(&hv, vote_round);//reset the hv



			/*for(int j=0; j<TRACE_SIZE; j++)

			{

				printf("%d ", hv_buffer[j]);

			}

			printf("\n");*/



			//send a SENDDE message

			//  msg_pointer, msg_type, voter_id, vote_mode, value=0, vote_iteration, sprd, epsl, bvn, history vector

			build_message(&msg_out, SENDDE, voter_id, vote_mode, msg_in.value, vote_round, vote_iteration, packet_loss_rate, data_msg_dropped, all_msg_dropped, bvn, data_size, hv_buffer, data_size);

			send_info(msock, (char *)&msg_out, sizeof(struct vote_msg_str), group, COMM_PORT);

			printf("SENDDE message sent to %s on port %d from voter %d!\n", group, COMM_PORT, voter_id);



			voter_msg_count++; //voter message counter increase by 1

		}

		else if(msg_in.type == TERMINATE && msg_in.id == SERVER_ID)//program quit when the server ends operation

		{

			printf("Server stopped working! Program ends!\n");

			break;

		}

		else if(msg_in.type == HV_QUERY && msg_in.id== SERVER_ID)//has to be the server

		{

			printf("HV_QUERY message received from the server asking for history vector upto round %d.\n", msg_in.value);

			//print_array(msg_in.v_trace);



			if(msg_in.v_trace[voter_id]==0)//hv not received by the server yet.

			{

				int hv_buffer[TRACE_SIZE];

				read_hv(hv, msg_in.value, hv_buffer, TRACE_SIZE);



				//send a HV_REPLY message

				//  msg_pointer, msg_type, voter_id, vote_mode, value=0, vote_iteration, sprd, epsl, bvn, history vector

				build_message(&msg_out, HV_REPLY, voter_id, vote_mode, msg_in.value, msg_in.round, vote_iteration, packet_loss_rate, data_msg_dropped, all_msg_dropped, bvn, data_size, hv_buffer, data_size);

				send_info(msock, (char *)&msg_out, sizeof(struct vote_msg_str), group, COMM_PORT);

				printf("HV_REPLY message sent to %s on port %d from voter %d!\n", group, COMM_PORT, voter_id);

				voter_msg_count++; //voter message counter increase by 1

			}

			else

			{

				printf("No reply sent because the server already got my history vector.\n");

			}

		}

		else

		{

			//messages ignored.

		}



    }



	//cleaning up

	close(msock);

	close(udp_sock);





    return 0;

}


















