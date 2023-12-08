# loxone_mqttsn
MQTT-SN client for Loxone.

MQTT-SN specifications available [here](https://www.oasis-open.org/committees/download.php/66091/MQTT-SN_spec_v1.2.pdf).

### Why a MQTT-SN client for Loxone ?
Loxone is a great platform, but for there is no possibility to ingegrate it natively with a MQTT broker.
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
1. Create 2 listeners on the MQTT broker (1 for Loxone publisher, 1 for Loxone subscriber)
2. On Loxone, create 2 Virtual Outputs ("MQTT-SN Publisher" with port UDP X, and "MQTT-SN Subscriber" with port UDP Y), and a Virtual Input "MQTT-SN Publisher" with port UDP Z.
3. On Publisher Application, workflow will be:
   1. Create a UDP connection on port X
   2. Connect to MQTT broker
   3. Monitor port X: as soon as data is available (that is, data user is willing to publish to MQTT), parse it and forward it to MQTT broker
4. On Subscriber Application, workflow will be:
   1. Create a UDP connection on port Y
   2. Connect to MQTT broker
   3. On every subscription request, parse it and forward it to MQTT Broker
   4. Monitor MQTT Broker: as soon as data available (that is, data published on a monitored MQTT topic, parse it and forward it to Loxone on port Z

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

![image](https://github.com/sebastienfouss/loxone_mqttsn/assets/14035269/e8768069-99b2-475b-a25e-9acffdd316bb)





  
