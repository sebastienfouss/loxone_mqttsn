// IP and Port of the MQTT SN Gateway
#define MQTTSN_GW_URL "/dev/udp/192.168.10.56/1884"
// Client ID
#define CLIENT_ID "LoxoneMQTTSNClient_Publisher"

// Port of Loxone Virtual Output used to listen to Publish requests
#define LISTEN_PORT "/dev/udp//9904"

// Couple of timeouts
#define MQTTSN_GW_TIMEOUT 30
#define MQTTSN_GW_CONN_TIMEOUT 3
#define MQTTSN_GW_DISCONN_TIMEOUT 1

#define BUFF_SIZE 500
#define MAX_TOPICS 50
#define MAX_TOPIC_SIZE 50

// Following variables are used to manage topic registration
int gRegisteredTopics = 0;
char *gTopics[MAX_TOPICS]; // Loxone does not support 2 dimensional array, so let's go for an array of pointers
int gTopicsIDs[MAX_TOPICS];

// Prepare stream to receive data from Loxone to publish on MQTT-SN Gateway
STREAM *pInStream;
sleep(500); // Wait a bit before opening stream, it seems that Loxone sometimes ignore the request if it comes too early
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
         setoutputtext (2, "Topic Registered");
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
      setoutputtext (2, status);
   } else {
      // OK, topic registered on Gateway
      strcpy (&gTopics[gRegisteredTopics][0], topic);
      gTopicsIDs[gRegisteredTopics] = (szBufferIn[2] << 8) + szBufferIn[3];
      topicID = gTopicsIDs[gRegisteredTopics];
      sprintf (status, "Topic registration OK: %d - %d - %s", gRegisteredTopics, topicID, topic);
      gRegisteredTopics++;
      setoutputtext (2, status);
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


int processIncomingRequest(int nCnt, char *message) {

   int i;
   char* pos;
   char topic[200];
   char payload[200];
   char szBuffer[BUFF_SIZE];
   int topicID;
   char status[200];
      
   // Get topic and payload
   i = 0;
   while (1) {
      pos = strstr (&message[i], "/");
      if (pos == NULL)
        break;
      i = strlen (message) - strlen (pos) + 1;
   }
   strncpy (topic, &message[0], i-1);
   strcpy (payload, &message[i]);
        
   topicID = getTopicID (topic);

   // Prepare message
   i = 1;
   // We'll update message length afterwards
   szBuffer[i++] = 0x0C; // Publish
   szBuffer[i++] = 0x00; // Flag
   szBuffer[i++] = (topicID >> 8); // Topic ID
   szBuffer[i++] = (topicID % 256); // Topic ID
   szBuffer[i++] = 0x00; // MsgID
   szBuffer[i++] = 0x01; // MsgID
   strcpy(&szBuffer[i], payload);
   szBuffer[0] = i + strlen(payload);
        
   stream_write (pStream, szBuffer, szBuffer[0]); // write to output buffer
   stream_flush (pStream);

   sprintf (status, "PUBLISHED: %s : %s", topic, payload);
   setoutputtext (2, "");
   setoutputtext (1, status);

   return 1;
}

//
// Main loop starts here
//

// Close pending connection, if any
disconnect();

int nCnt;
char szBufferIn[BUFF_SIZE];

while (1) {

   while (1) {
      setoutput (12, 0);
      if (connect() == -1) {
         setoutputtext(0,"Connection failed");
         sleep (MQTTSN_GW_CONN_TIMEOUT);
      }
      else {
         setoutputtext(0,"CONNECTED");
         // Force re-registration of topics, if this results from a server disconnection
         gRegisteredTopics = 0;
	 sleep (100);
         break;
      }
   }

  // Ok, we are connected
  while (1) {

      // Check if we received data from Loxone MQTT-SN OUT
      nCnt = stream_read(pInStream,szBufferIn,BUFF_SIZE,5000);
      if (nCnt > 0) {
         szBufferIn[nCnt] = 0;
         setoutputtext (2, "GOT DATA !");
         processIncomingRequest(nCnt, szBufferIn);
      } else {
	 setoutputtext (2, "TIMEOUT !");
      }

      // Send keepalive packet
      if (keepalive() == 1) {
          // Keep alive ok
          sleep (50); // Wait until next keepalive
      } else {
          // Connection dead, reconnect
          setoutputtext(0,"CONNECTION DEAD");
          sleep (1000);
          break;
      } 
  }
}
