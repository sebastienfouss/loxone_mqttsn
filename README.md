# loxone_mqttsn
MQTT-SN client for Loxone.

MQTT-SN specifications available [here](https://www.oasis-open.org/committees/download.php/66091/MQTT-SN_spec_v1.2.pdf).

### Disclaimers
- I am not affiliated with Loxone Electronics GmbH
- Use this code at your own risk
  
### Why a MQTT-SN client for Loxone ?
Loxone is a great platform, but for there is no possibility to integrate it natively with a MQTT broker.
Solutions exist (for example, a Node Red gateway between MQTT broker and Loxone), but I wanted a solution that does not imply an intermediate layer between Loxone and the broker.

In order to implement this, a couple of limitations have to be considered:

### Loxone limitations 1: Virtual Inputs and Outputs
1. <b>Virtual Output:</b> the source port of an UDP connection is unknown to the user, making it impossible to monitor the replies from the server in a bidirectional communication
2. <b>Virtual Input:</b> can only be a UDP connection

So using only Virtual Inputs and Outputs is not enough to manage a MQTT connection. How can we proceed ?

A couple of ways:

1. Run a pair of socat, for example on your router:
   - Socat 1 accept UDP messages on Port A, and redirect them to MQTT broker on Port B with source port X
   - Socat 2 accept UDP messages back from broker on port X, and redirect them to Loxone on Port C

Pretty easy, it does work, but it requires to setup socat somewhere.

2. Run picoC program, more below.

### Loxone limitations 2: picoC applications
1. PicoC is mono threaded only, that is, each picoC block will run in its own thread, but you can't spawn threads within a block. This implies, for bidirectional communication as it is the case with a MQTT server, that we have to create two applications: a publisher and a listener.
2. UDP sockets cannot be reused between two applications
Note: PicoC can open a TCP connection, so therotically we could use TCP instead of UDP, at least to communicate with the broker, but this opens another level of complexity: PicoC is not really user friendly for debugging.

### Implementation

![image](https://github.com/sebastienfouss/loxone_mqttsn/assets/14035269/6beb3953-6c7e-427b-9563-885bebff3805)

1. Create 2 listeners on the MQTT-SN gateway (1 for Loxone publisher, 1 for Loxone subscriber)
2. On Loxone, create a Virtual Input "MQTT-SN Publisher" with port UDP X, and 2 Virtual Outputs ("MQTT-SN Publisher" with target port UDP Y, and "MQTT-SN Subscriber" with target port UDP Z)
3. On Publisher application, workflow will be:
   1. Open an ingress UDP connection on port Z
   2. Open an egress UDP stream to MQTT-SN gateway
   3. Monitor port Z: as soon as data is available (that is, data user is willing to publish to MQTT), parse it and forward it to MQTT-SN gateway
   4. Keepalive is also managed from the Publisher application
4. On Subscriber application, workflow will be:
   1. Open an ingress UDP connection on port Y
   2. Open an egress UDP stream to MQTT-SN gateway
   3. Open an egress UDP stream to Loxone on port X
   3. On every subscription request, parse it and forward it to MQTT-SN gateway
   4. Monitor MQTT Broker: as soon as data available (that is, data published on a monitored MQTT topic, parse it and forward it to Loxone on port X

### Choice of a broker
Next question is: what broker to chose, or more precisely: what gateway to choose to interact with a MQTT broker, considering that we will have to use UDP and MQTT protocol is TCP base ?
Couple of options here, the main ones:
- MQTT-SN: a lightweight, UDP-base, MQTT protocol variant
- COAP: another MQTT-like protocol.
This project is implemented using MQTT-SN protocol. COAP is a bit too complex to manage it in the limited environment of a PicoC application.

This brings us to the main topic: which broker ?
Considering that a MQTT-SN "broker" is in essence a gateway to an actual MQTT broker, I decided to use EMQX (emqx.io) that embeds in a single server a MQTT broker, as well as a MQTT-SN (and COAP, etc) gateway.



# Setup

## EMQX Broker

Install EMQX Broker.

On Cluster Settings > Gateways, enable MQTT-SN gateway.

Make sure to add a second (UDP) listener, as per the picture below.

![image](https://github.com/sebastienfouss/loxone_mqttsn/assets/14035269/9f142efc-3c39-44f5-9ef0-2c8e31c9e806)


## Loxone

### Virtual Input
Add a Virtual Input, name it for example MQTT-SN IN, and set the port (for example, 9903).

### Virtual Output
Add 2 Virtual Outputs:
1. MQTT-SN OUT PUBLISH
 - As address, enter /dev/udp/127.0.0.1/9904
 - Make sure to enable "Close connection after sending"
 - Leave the 2 remaining fields (separator, connection command) empty
4. MQTT-SN OUT SUBSCRIBE
- As address, enter /dev/udp/127.0.0.1/9902
- Make sure to enable "Close connection after sending"
- Leave the 2 remaining fields (separator, connection command) empty

## Pico C code
Add on an existing or a new page, two picoC blocks:
- First one named "Subscribe"
- Second one named "Publish

The output O13 from the "Subscribe" program shall be linked to a Front Detector, then linked to your subscriptions (see below).

In the "Subscribe" block, copy and paste the content from the file subscriber.c.
In the "Publish" block, copy and paste the content from the file publish.c.

Make sure to modify the URL address of your MQTT-SN gateway, and to adapt the ports if needed.

End result should be similar to this:

![image](https://github.com/sebastienfouss/loxone_mqttsn/assets/14035269/c474a96a-5e08-42cc-b6b0-14491cc0694a)

### Publish data to MQTT
In Virtual Output "MQTT-SN OUT PUBLISH", add a Virtual Output Command.
![image](https://github.com/sebastienfouss/loxone_mqttsn/assets/14035269/db6de496-2537-4266-9d38-852b04d604b0)

In the Virtual Output Command, set "Command with ON" as a typical MQTT publication, ie topic/payload.
![image](https://github.com/sebastienfouss/loxone_mqttsn/assets/14035269/8cbbbde2-535d-40b1-8ac1-c59c72c74343)
You can use <v> etc for payload, as usual for a Virtual Output.

### Subscribe to a topic
In Virtual Output "MQTT-SN OUT SUBSCRIBE", add a Virtual Output Command.
![image](https://github.com/sebastienfouss/loxone_mqttsn/assets/14035269/cb6add79-5ac8-46d7-9f92-0e3b2c5015aa)

In the Virtual Output Command, fill "Command with ON" with the requested MQTT topic.
![image](https://github.com/sebastienfouss/loxone_mqttsn/assets/14035269/a491ea88-e3c1-4478-9fca-30c590570373)

Finally, make sure to connect the Virtual Output Command to the Front Detection linked to Publisher bloc.

![image](https://github.com/sebastienfouss/loxone_mqttsn/assets/14035269/6ca6fe49-e6a6-4b17-831a-8b34d1beba06)

### Receive data from a subscribed topic
In Virtual Input "MQTT-SN IN", add a Virtual Input Command.
Fill the command detection as usual, for example:

![image](https://github.com/sebastienfouss/loxone_mqttsn/assets/14035269/3f8075be-8f04-4dcb-9290-5d1c9caf28b8)

  
