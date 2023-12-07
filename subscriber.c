#define MQTTSN_GW_URL "/dev/udp/192.168.10.56/1883"
#define MQTTSN_GW_TIMEOUT 30
#define MQTTSN_GW_CONN_TIMEOUT 3
#define MQTTSN_GW_DISCONN_TIMEOUT 1
#define LISTEN_PORT "/dev/udp//9902"
#define PUBLISH_PORT "/dev/udp/127.0.0.1/9903"
#define CLIENT_ID "LoxoneMQTTSNClient_Subscriber"
#define BUFF_SIZE 500

#define MAX_TOPICS 50
#define MAX_TOPIC_SIZE 50

int gRegisteredTopics = 0;
char *gTopics[MAX_TOPICS];
int gTopicsIDs[MAX_TOPICS];

// Prepare stream to publish data received from MQTT-SN GW
STREAM *pInStream, *pOutStream;
sleep(500); // Wait a bit before opening stream, it seems that Loxone sometimes ignore the request if it comes too early

pOutStream = stream_create(PUBLISH_PORT,0,0);// create udp stream
pInStream = stream_create(LISTEN_PORT,0,0);// create udp stream

// PicoC on Loxone does not support 2 dimensions arrays :(
int k;
for (k=0; k<MAX_TOPICS; k++) {
  gTopics[k] = malloc (MAX_TOPIC_SIZE);
}


STREAM* pStream;


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
         topicID = gTopicsIDs[i];
         return topicID;
     }
   }

   i = 1;
   // We'll update message length afterwards
   szBuffer[i++] = 0x0A; // Register
   szBuffer[i++] = 0x00; // TopicID - 0
   szBuffer[i++] = 0x00; // TopicID - 0
   szBuffer[i++] = 0x00; // MsgID - 0
   szBuffer[i++] = 0x01; // MsgID - 1
   strcpy(&szBuffer[i], topic);
   szBuffer[0] = i + strlen(topic);
   stream_write (pStream, szBuffer, szBuffer[0]); // write to output buffer
   stream_flush (pStream);

   // Wait for answer
   nCnt = stream_read(pStream,szBufferIn,BUFF_SIZE,MQTTSN_GW_DISCONN_TIMEOUT*1000); // read stream, will either reply with 0x18 (DISCONNECT OK) or not reply (no ongoing connection), both are ok

   if ((szBufferIn[1] != 0x0B)|| (szBufferIn[6] != 0x00)) {
      // ERROR
      sprintf (status, "Topic registration Error: %d - %s %d", gRegisteredTopics, topic, szBufferIn[6]);
      setoutputtext (1, status);
   } else {
      // OK, topic registered on Gateway
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
   stream_write (pStream, szBuffer, 2); // write to output buffer
   stream_flush (pStream);
   // Wait for answer
   nCnt = stream_read(pStream,szBufferIn,BUFF_SIZE,MQTTSN_GW_DISCONN_TIMEOUT*1000);
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
      pStream = stream_create(MQTTSN_GW_URL,0,0); // create udp stream
	  if (pStream != NULL)
	     break;
      // If connection fails, sleep 1s and retry
      sleep (1000); 
   }
   // Send Connect message
   stream_write(pStream, szBuffer, i); // write to output buffer
   stream_flush(pStream); 
   // Listen to messages
   nCnt = stream_read(pStream,szBufferIn,BUFF_SIZE,MQTTSN_GW_CONN_TIMEOUT*1000); // read stream
   setoutput (0, nCnt);
   setoutput (1, szBufferIn[1]);
   setoutput (2, szBufferIn[2]);
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
      pStream = stream_create(MQTTSN_GW_URL,0,0); // create udp stream
	  if (pStream != NULL)
	     break;
      // If connection fails, sleep 1s and retry
      sleep (1000); 
   }

   // Send Disconnect message
   stream_write (pStream, szBuffer, 2); // write to output buffer
   stream_flush (pStream);

   // Wait for answer
   nCnt = stream_read(pStream,szBufferIn,BUFF_SIZE,MQTTSN_GW_DISCONN_TIMEOUT*1000); // read stream, will either reply with 0x18 (DISCONNECT OK) or not reply (no ongoing connection), both are ok

   // Close stream
   stream_close (pStream);
   
   return 1;

}


int processPublishMessage(int nCnt, char *_message) {

    int topic = 0;
    int i;

    char message[300];
    char status[300];

	// Process message
    if (_message[1] != 0x0c) {
	   setoutputtext(1, "Unexpected message");
       return -1;
    }
   
    // Get topic
    topic = (_message[3] << 8) + _message[4]; 

    // Get topic name
    for (i=0; i<gRegisteredTopics; i++) {
      if (gTopicsIDs[i] == topic) {
         strncpy (message, &_message[7], nCnt-7);
         sprintf (status, "%s/%s", gTopics[i], message);
         setoutputtext (1, status);
         stream_write (pOutStream, status, strlen(status)); // write to output buffer
         stream_flush (pOutStream);

         break;
     }
  }
   return 1;
}

int processIncomingRequest(int nCnt, char *message) {

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
       szBuffer[i++] = 0x12; // Subscribe
       szBuffer[i++] = 0x01; // Flag
       szBuffer[i++] = 0x20; // MsgID (let's use 0x20 for Subscribe Messages)
       szBuffer[i++] = (topicID % 256); // MsgID
       szBuffer[i++] = (topicID >> 8); // Topic ID
       szBuffer[i++] = (topicID % 256); // MsgID
       szBuffer[0] = i;
       stream_write (pStream, szBuffer, szBuffer[0]); // write to output buffer
       stream_flush (pStream);
	   // And wait for answer
       nCnt = stream_read(pStream,szBufferIn,BUFF_SIZE,MQTTSN_GW_DISCONN_TIMEOUT*1000); // read stream, will either reply with 0x18 (DISCONNECT OK) or not reply (no ongoing connection), both are ok
       // Process the answer

       sprintf (status, "SUBSCRIBED: %s", topic); 
       setoutputtext(1, status);
    
   return 1;
}



int s;

disconnect(); // Flush pending connection, if any
int nCnt;
char szBufferIn[BUFF_SIZE];

while (1) {

   while (1) {
      setoutput (12, 0);
      s = connect();
      if (s == -1) {
         setoutputtext(0,"Connection failed");
         sleep (MQTTSN_GW_CONN_TIMEOUT);
      }
      else {
         setoutputtext(0,"CONNECTED");
         gRegisteredTopics = 0;
         // Subscribe topics by sending a pulse on Output 13
         setoutput (12, 1);
		 sleep (100);
         break;
      }
   }
   
   while (1) {

      // Check if we received data from Loxone MQTT-SN OUT
      nCnt = stream_read(pInStream,szBufferIn,BUFF_SIZE,100);
      if (nCnt > 0) {
         szBufferIn[nCnt] = 0;
         processIncomingRequest(nCnt, szBufferIn);
      } //else {
         //setoutputtext (2, "");
      //}
	  sleep (50);
      // Check if we received data from MQTT-SN GW
      nCnt = stream_read(pStream,szBufferIn,BUFF_SIZE,5000);
      if (nCnt > 0) {
         setoutputtext (1, "");
         //setoutputtext (2, "GOT DATA FROM MQTT-SN GW!");
         szBufferIn[nCnt] = 0;
         processPublishMessage (nCnt, szBufferIn);
      } else //{
         //setoutputtext (2, "");
      //}
      sleep (50);

      if (keepalive() == 1) {
         // Keep alive ok
         sleep (200); // Wait until next keepalive
      }
      else {
         // Connection dead, reconnect
         setoutputtext(0,"CONNECTION DEAD");
         sleep (1000);
         break;
      }
  }
}
