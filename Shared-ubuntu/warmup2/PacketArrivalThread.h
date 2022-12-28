#ifndef _PACKETARRIVALTHREAD_H_
#define _PACKETARRIVALTHREAD_H_

#include "Packet.h"

unsigned int createPacketByLine(char * reader, packet * p, struct timeval * last);

extern void * PacketArrivalThread(void * arg);

#endif