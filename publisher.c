// MQTT-SN broker settings
#define MQTTSN_GW_URL "/dev/udp/192.168.10.56/1883"
#define CLIENT_ID "LoxoneMQTTSNClient_Subscriber"

// Loxone port settings
#define LISTEN_PORT "/dev/udp//9902"
#define PUBLISH_PORT "/dev/udp/127.0.0.1/9903"

// Stream reading/writing timeout, in seconds
#define MQTTSN_GW_TIMEOUT 1
#define MQTTSN_GW_MSG_TIMEOUT 10

// Max buffer size
#define BUFF_SIZE 1000

// Sizing
#define MAX_TOPICS 50
#define MAX_TOPIC_SIZE 50

// Topics vars
int gRegisteredTopics = 0;
char *gTopics[MAX_TOPICS];
int gTopicsIDs[MAX_TOPICS];

// Prepare stream to publish data received from MQTT-SN GW
STREAM *pLoxoneInStream, *pLoxoneOutStream;

// Wait a bit before opening stream, it seems that Loxone sometimes ignore the request if it comes too early
sleep(500); 
pLoxoneOutStream = stream_create(PUBLISH_PORT,0,0);// create udp stream
pLoxoneInStream = stream_create(LISTEN_PORT,0,0);// create udp stream

// PicoC on Loxone does not support 2 dimensions arrays :(
int k;
for (k=0; k<MAX_TOPICS; k++) {
  gTopics[k] = malloc (MAX_TOPIC_SIZE);
}

// MQTT-SN stream, will be created in connect() function
STREAM* pMQTTSNStream;


// Get topic ID, and register it if it does not exist
int getTopicID (char *topic) {

	char szBuffer[BUFF_SIZE], szBufferIn[BUFF_SIZE];
	int nCnt;
	int i;
	int topicID = -1;
	char status[300];
	// Check if topic is registered
	for (i=0; i<gRegisteredTopics; i++) {
		if (strcmp (topic, gTopics[i]) == 0) {
			// Topic already registered
			topicID = gTopicsIDs[i];
			return topicID;
		}
	}

	// Topic not registered, register it
	i = 1;
	// We'll update message length afterwards
	// TODO: handle length > 255
	szBuffer[i++] = 0x0A; // Register
	szBuffer[i++] = 0x00; // TopicID - 0
	szBuffer[i++] = 0x00; // TopicID - 0
	szBuffer[i++] = 0x00; // MsgID - 0
	szBuffer[i++] = 0x01; // MsgID - 1
	strcpy(&szBuffer[i], topic);
	szBuffer[0] = i + strlen(topic);
	// Write to output buffer
	stream_write (pMQTTSNStream, szBuffer, szBuffer[0]); 
	stream_flush (pMQTTSNStream);

	// Wait for answer
	nCnt = stream_read(pMQTTSNStream,szBufferIn,BUFF_SIZE,MQTTSN_GW_TIMEOUT*1000); // read stream, will either reply with 0x18 (DISCONNECT OK) or not reply (no ongoing connection), both are ok

	if ((szBufferIn[1] != 0x0B)|| (szBufferIn[6] != 0x00)) {
		// ERROR
		sprintf (status, "Topic registration Error: %d - %s %d", gRegisteredTopics, topic, szBufferIn[6]);
		setoutputtext (1, status);
	} else {
		// OK, topic registered on MQTT-SN Gateway
		strcpy (&gTopics[gRegisteredTopics][0], topic);
		gTopicsIDs[gRegisteredTopics] = (szBufferIn[2] << 8) + szBufferIn[3];
		topicID = gTopicsIDs[gRegisteredTopics];
		gRegisteredTopics++;
	}
	return topicID;
}

// Keepalive function
int keepalive() {

	char szBuffer[3], szBufferIn[BUFF_SIZE];
	int nCnt;
	szBuffer[0] = 0x02; // Length
	szBuffer[1] = 0x16; // Ping

	// Send Keepalive message
	stream_write (pMQTTSNStream, szBuffer, 2);
	stream_flush (pMQTTSNStream);
	// Wait for answer
	nCnt = stream_read(pMQTTSNStream,szBufferIn,BUFF_SIZE,MQTTSN_GW_TIMEOUT*1000);
	if (nCnt == 0)
		return -1;
	return 1;

}

// Connect function
int connect() {

	char szBuffer[BUFF_SIZE], szBufferIn[BUFF_SIZE];
	int duration = 1000;
	int i;
	int nCnt;
	i = 1; // Skip first byte (length), we will fill it later
	// TODO: handle length > 255
	szBuffer[i++] = 0x04; // MsgType: Connect
	szBuffer[i++] = 0x04; // Flags: set CleanSession to true
	szBuffer[i++] = 0x01; // ProtocolId: 0x01 (only allowed value)
	szBuffer[i++] = duration >> 8;
	szBuffer[i++] = duration & 0xFF;
	strcpy (&szBuffer[i], CLIENT_ID);
	i+= strlen(CLIENT_ID);
	szBuffer[0] = i;

	// Connect to MQTT-SN Gateway
	while (1) {
		pMQTTSNStream = stream_create(MQTTSN_GW_URL,0,0); // create udp stream
		if (pMQTTSNStream != NULL)
		   break;
		// If connection fails, sleep 1s and retry
		sleep (1000); 
	}
	// Send Connect message
	stream_write(pMQTTSNStream, szBuffer, i);
	stream_flush(pMQTTSNStream); 
	// Wait and read reply from MQTT-SN gateway
	nCnt = stream_read(pMQTTSNStream,szBufferIn,BUFF_SIZE,MQTTSN_GW_TIMEOUT*1000);
	if (nCnt == 0)
	   return -1;
	return 1;
}

// Disconnect function
int disconnect() {

	char szBuffer[3], szBufferIn[BUFF_SIZE];
	int nCnt;

	szBuffer[0] = 0x02; // Length
	szBuffer[1] = 0x18; // Disconnect

	// Connect to MQTT-SN Gateway
	while (1) {
		pMQTTSNStream = stream_create(MQTTSN_GW_URL,0,0); // create udp stream
		if (pMQTTSNStream != NULL)
			break;
		// If connection fails, sleep 1s and retry
		sleep (1000); 
	}

	// Send Disconnect message
	stream_write (pMQTTSNStream, szBuffer, 2); // write to output buffer
	stream_flush (pMQTTSNStream);

	// Wait for answer
	nCnt = stream_read(pMQTTSNStream,szBufferIn,BUFF_SIZE,MQTTSN_GW_TIMEOUT*1000); // read stream, will either reply with 0x18 (DISCONNECT OK) or not reply (no ongoing connection), both are ok

	// Close stream
	stream_close (pMQTTSNStream);
   
	return 1;
}

// Process publish message received from MQTT-SN gateway
int processPublishMessage(int nCnt, char *_message) {

	int topic = 0;
	int i;
	int l;

	char message[BUFF_SIZE];
	char status[20 + BUFF_SIZE];

	// Length can be encoded on 1 or 3 bytes
	l = 0;
	if (_message[0] == 0x01)
	  l = 2; // length encoded on 3 bytes

	// Skip PINGRESP (not expected but can happen)
	if (_message[1+l] == 0x17) {
	   return 1;
	}
	
	// Process message
	if (_message[1+l] != 0x0c) {
	   sprintf (status, "Unexpected message: %d %d %d", _message[0+l],_message[1+l],_message[2+l]);
	   setoutputtext(1, status);
	   return -1;
	}

	// Get topic
	topic = (_message[3+l] << 8) + _message[4+l]; 

	// Get topic name
	for (i=0; i<gRegisteredTopics; i++) {

		if (gTopicsIDs[i] == topic) {
			// Publish the topic + payload message to Loxone listener
			stream_write (pLoxoneOutStream, status, strlen(status)); // write to output buffer
			stream_flush (pLoxoneOutStream);
			// Set status
			strncpy (message, &_message[7+l], nCnt-7-l);
			sprintf (status, "Published: %s/%s", gTopics[i], message);
			setoutputtext (1, status);
			break;
		}
	}
	return 1;
}

// Process subscription message received from Loxone
int processSubscriptionRequest(int nCnt, char *message) {

	int i;
	char* pos;
	char topic[200];
	char payload[200];
	char szBuffer[BUFF_SIZE];
	int topicID;
	char status[200];
   
	topic = &message[0];
	topicID = getTopicID (topic);

	// Prepare message
	i = 1;
	// We'll update message length afterwards
	// TODO: handle length > 255
	szBuffer[i++] = 0x12; // Subscribe
	szBuffer[i++] = 0x01; // Flag
	szBuffer[i++] = 0x20; // MsgID (let's use 0x20 for Subscribe Messages)
	szBuffer[i++] = (topicID % 256); // MsgID
	szBuffer[i++] = (topicID >> 8); // Topic ID
	szBuffer[i++] = (topicID % 256); // MsgID
	szBuffer[0] = i;
	stream_write (pMQTTSNStream, szBuffer, szBuffer[0]); // write to output buffer
	stream_flush (pMQTTSNStream);
	// And wait for answer
	nCnt = stream_read(pMQTTSNStream,szBufferIn,BUFF_SIZE,MQTTSN_GW_TIMEOUT*1000); // read stream, will either reply with 0x18 (DISCONNECT OK) or not reply (no ongoing connection), both are ok
	sprintf (status, "SUBSCRIBED: %s", topic); 
	setoutputtext(1, status);
	
	return 1;
}

// Main 

// Flush pending connection, if any
disconnect(); 
int nCnt;
char szBufferIn[BUFF_SIZE];
char status[300];

while (1) {

	// Connect
	while (1) {
		if (connect() == -1) {
			setoutputtext(0,"Connection failed");
			sleep (MQTTSN_GW_CONN_TIMEOUT);
		}
		else {
			setoutputtext(0,"CONNECTED");
			gRegisteredTopics = 0;
			// Subscribe topics by sending a pulse on Output 13
			setoutput (12, 1);
			sleep (300);
			setoutput (12, 0);
			break;
		}
	}

	while (1) {

		// Process all subscription requests   
		while (1) {
			nCnt = stream_read(pLoxoneInStream,szBufferIn,BUFF_SIZE,1000);
			if (nCnt > 0) {
				szBufferIn[nCnt] = 0;
				processSubscriptionRequest(nCnt, szBufferIn);
			} else {
				break;
			}
		}
		sleep (50);

		// Process data received from MQTT-SN gateway (should be Publish messages)
		setoutputtext (2, "");
		nCnt = stream_read(pMQTTSNStream,szBufferIn,BUFF_SIZE,MQTTSN_GW_MSG_TIMEOUT*1000);
		if (nCnt > 0) {
			setoutputtext (1, "");
			szBufferIn[nCnt] = 0;
			processPublishMessage (nCnt, szBufferIn);
			continue;
		}

		// If no message received within (MQTTSN_GW_MSG_TIMEOUT*1000) seconds, send a PING request
		setoutputtext (2, "KEEPALIVE");
		if (keepalive() == 1) {
			// Keep alive ok
			sleep (100);
		}
		else {
			// Connection dead, reconnect
			setoutputtext(0,"CONNECTION DEAD");
			sleep (1000);
			break;
		}
	}
}

