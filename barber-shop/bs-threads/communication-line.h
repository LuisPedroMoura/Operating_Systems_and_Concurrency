#ifndef COMMUNICATION_LINE_H
#define COMMUNICATION_LINE_H

#include "global.h"
#include "service.h"

typedef struct _Message_
{
	Service service;
	int newMessage = 0;
}Message;

typedef struct _CommunicationLine_
{
	Message commArray[global->NUM_CLIENTS];
}CommunicationLine;

Message write_message(Service service);
int read_message(CommunicationLine* commLine, int clientID);
int send_message(CommunicationLine* commLine, Message message);
int new_message_available(CommunicationLine* commLine, int clientID);
int no_message_available(CommunicationLine* commLine, int clientID);

#endif
