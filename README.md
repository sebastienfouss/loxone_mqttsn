# loxone_mqttsn
MQTT-SN client for Loxone

### Why a MQTT-SN client for Loxone ?
Loxone is a great platform, but for there is no possibility to ingegrate it natively with a MQTT broker.
Solutions exist (for example, a Node Red gateway between MQTT broker and Loxone), but I wanted a solution that does not imply an intermediate layer between Loxone and the broker.

In order to implement this, a couple of limitations have to be considered:

### Loxone limitations 1: Virtual Inputs and Outputs
1. <b>Virtual Output:</b> the source port of an UDP connection is unknown to the user, making it impossible to monitor the replies from the server in a bidirectional communication
2. <b>Virtual Input:</b> can only be a UDP connection

Limitation #2 indicates that we will have to rely on an UDP connection, but how to handle limitation #1 ?

A couple of ways:

1. Run a pair of socat, for example on your router:
   - Socat 1 accept UDP messages on Port A, and redirect them to MQTT broker on Port B with source port X
   - Socat 2 accept UDP messages back from broker on port X, and redirect them to Loxone on Port C

Pretty easy, it does work, but it requires to setup socat somewhere.

2. Run picoC program, more below.

### Loxone limitations 2: picoC applications
1. PicoC is mono threaded only, that is, each picoC block will run in its own thread, but you can't spawn threads within a block. This implies, for bidirectional communication as it is the case with a MQTT server, that we have to create two applications: a publisher and a listener.
2. UDP sockets cannot be reused between two applications

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
