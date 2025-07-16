/*********************************************/
/*              multicast.h v4.0             */
/*           Selective Solicitation          */
/*    with variable sized data messages      */
/*********************************************/


/*library for multicasting utilities*/

#ifndef MULTICAST_H
#define MULTICAST_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>

//#include <linux/in.h>
//#include <linux/net.h>

#define MULTICAST_ADDRESS_BASE  "225.0.0."
#define MULTICAST_PORT_BASE  30000
#define DEFAULT_MULTICAST_TTL_VALUE  32 /* to send datagrams to any host anywhere */

#ifndef INADDR_NONE
#define INADDR_NONE     0xffffffff
#endif

#define NAME_LEN 100
#define MAX_LEN 1024

#define TRUE 1
#define FALSE 0



/*
 *  This function sets the socket option to make the local host
 *  join the mulicast group
 */

void joinGroup(int s, char *group)
{
  	// multicast group info structure
  	struct ip_mreq mreq;

  	mreq.imr_multiaddr.s_addr = inet_addr(group);
  	mreq.imr_interface.s_addr = INADDR_ANY;
  	if ( setsockopt(s,IPPROTO_IP,IP_ADD_MEMBERSHIP,(char *) &mreq,
                  sizeof(mreq)) == -1 )
  	{
  		printf("error in joining group \n");
    	exit(-1);
  	}
}


/*
 *  This function removes a process from the group
 */

void leaveGroup(int recvSock,char *group)
{
  	struct ip_mreq dreq;  /* multicast group info structure */

  	dreq.imr_multiaddr.s_addr = inet_addr(group);
  	dreq.imr_interface.s_addr = INADDR_ANY;
  	if( setsockopt(recvSock,IPPROTO_IP,IP_DROP_MEMBERSHIP,
                (char *) &dreq,sizeof(dreq)) == -1 )
  	{
    	printf("error in leaving group \n");
    	exit(-1);
  	}

  	printf("process quitting multicast group %s \n",group);
}


/*
 *  This function sets a socket option that allows multipule processes
 *  to bind to the same port, this is imporatant if you want to
 *  run the software on single machine.
 */

void reusePort(int s)
{
  	int one=1;

  	if( setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(char *) &one,sizeof(one)) == -1 )
  	{
    	printf("error in setsockopt,SO_REUSEPORT \n");
    	exit(-1);
  	}
}


/*
 *  This function sets the Time-To-Live value
 */

void setTTLvalue(int s,u_char *ttl_value)
{
  	if( setsockopt(s,IPPROTO_IP,IP_MULTICAST_TTL,(char *) ttl_value,
                 sizeof(u_char)) == -1 )
  	{
    	printf("error in setting ttl value\n");
  	}
}


/*
 *
 *  By default, messages sent to the multicast group are looped
 *  back to the local host. this function disables that.
 *  loop = 1 means enable loopback
 *  loop = 0 means disable loopback
 *  NOTE : by default, loopback is enabled
*/

void setLoopback(int s,u_char loop)
{
  	if( setsockopt(s,IPPROTO_IP,IP_MULTICAST_LOOP,(char *) &loop,
                 sizeof(u_char)) == -1 )
  	{
    	printf("error in disabling loopback\n");
  	}
}

/*
 *  Function used to send msgs
 *  modified to send binary memory blocks instead of strings
 */
void send_info(int ssock, char * buf, int size, char * group_name, int udp_port)
{
  	struct sockaddr_in dest;//destination information

  	/* send the message to the group */
  	dest.sin_family = AF_INET;
  	dest.sin_port =  htons(udp_port);//destination port number
  	dest.sin_addr.s_addr = inet_addr(group_name);//destination address, a multicast address

  	if ( sendto(ssock, buf, size ,0 ,(struct sockaddr *) &dest, sizeof(dest)) < 0 )
    {
      	printf("error in sendto \n");
      	exit(-1);
    }
    else
    {
		//printf("A packet sent to address %s, udp port %d.\n", group_name, htons(dest.sin_port));
	}
}

/*
 *  Function used to send unicast msgs
 *  modified to send binary memory blocks instead of strings
 */
void send_unicast_packet(int ssock, char * buf, int size, char * dest_addr, int udp_port)
{
  	struct sockaddr_in dest;//destination information

  	/* send the message to the group */
  	dest.sin_family = AF_INET;
  	dest.sin_port =  htons(udp_port);//destination port number
  	dest.sin_addr.s_addr = inet_addr(dest_addr);//destination address, a multicast address

  	if ( sendto(ssock, buf, size ,0 ,(struct sockaddr *) &dest, sizeof(dest)) < 0 )
    {
      	printf("error in sendto \n");
      	exit(-1);
    }
    else
    {
		//printf("A packet sent to address %s, udp port %d.\n", group_name, htons(dest.sin_port));
	}
}

/*
 *  Function used to send data messsages, the packet can have a variable payload
 *  buf contains everything besides the payload in the packet
 *  a big buffer should be created to send the message
 *  modified to send binary memory blocks instead of strings
 */
void send_data_msg(int ssock, char * buf, int size, int payload_size, char * group_name, int udp_port)
{
  	struct sockaddr_in dest;//destination information

  	char * buffer = new char[size+payload_size];
  	memset(buffer, 0, size+payload_size);
  	memcpy(buffer, buf, size); //copy the header part of the data message, leave the payload area blank

  	/* send the message to the group */
  	dest.sin_family = AF_INET;
  	dest.sin_port =  htons(udp_port);//destination port number
  	dest.sin_addr.s_addr = inet_addr(group_name);//destination address, a multicast address

  	if ( sendto(ssock, buffer, size+payload_size ,0 ,(struct sockaddr *) &dest, sizeof(dest)) < 0 )
    {
      	printf("error in sendto \n");
      	exit(-1);
    }
    else
    {
		//printf("A packet with %d bytes of payload sent to address %s, udp port %d.\n", payload_size, group_name, htons(dest.sin_port));
	}

	delete buffer;
}


/*
 *Function that creates a multi-cast socket and binds it to a port
 *
 */
int CreateMcastSocket(int port)//input is a port number
{
	int multicast_socket;
	struct sockaddr_in group_host; /* multicast group host info structure */

	/* Set up the multicast group host information*/
	group_host.sin_family=AF_INET;
	group_host.sin_port=htons(port);

	/* wildcard address: means it may receive any unicast
     or multicast pkts destined to this port */
    group_host.sin_addr.s_addr = htonl(INADDR_ANY); /*unicast ot mcast packets*/
    /* OR (this is the only part missing in passiveUDP())==>
       groupHost.sin_addr.s_addr = htonl(inet_addr(GroupIPaddress1));
     this way this server is restricted to listen for mcast packets ONLY*/

  	/* Allocate a UDP socket and set the multicast options */

  	if ((multicast_socket = socket(PF_INET,SOCK_DGRAM, 0)) < 0)
  	{
    	printf("Function CreateMcastSocket: Can't create socket\n");
    	exit(-1);
  	}

  	/* allow multipule processes to bind to same multicast port */
  	reusePort(multicast_socket);

  	/* bind the UDP socket to the mcast address to recv messages from the group */
  	if((bind(multicast_socket,(struct sockaddr *) &group_host, sizeof(group_host))== -1))
  	{
    	printf("error in bind\n");
    	exit(2);
  	}

  	return multicast_socket;
}


/*
 *Function that creates a udp socket and binds it to a port
 *
 */

int CreateUdpSocket(int port)//input is a port number
{
	int udp_sock;
  	struct sockaddr_in source_addr;//source address data
  	source_addr.sin_family = AF_INET;
    //source_addr.sin_addr.s_addr = inet_addr(local_host);//use the computer's address
	source_addr.sin_addr.s_addr = htonl(INADDR_ANY);//use the computer's address
    source_addr.sin_port = htons((u_short)(port));

    if((udp_sock=socket(AF_INET,SOCK_DGRAM,0))<0)
    {
		printf("Creating UDP Socket Failed!\n");
    	exit(-1);
	}

  	/* allow multipule processes to bind to same multicast port */
  	//reusePort(udp_sock);

    if(bind(udp_sock,(struct sockaddr *)&source_addr, sizeof(source_addr))<0)
    {
      	perror("Bind failed");
      	exit(1);
  	}

  	return udp_sock;

}

#endif

