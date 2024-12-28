CSMA/CA
=======
This model simulates the Carrier Sense Multiple Access with Collision Avoidance 
(CSMA/CA) protocol. CSMA/CA is widely used in wireless networks, particularly 
in IEEE 802.11 (WiFi) networks. The model demonstrates how nodes:

- Listen to the channel for DIFS before transmitting (carrier sensing)
- Use random backoff times to avoid collisions
- Implement the RTS/CTS handshake mechanism
- Implement the channel contention and backoff time freeze

The supplied omnetpp.ini file contains 3 predefined configurations:

- CSMA/CA at high load (utilization =~ max)
- CSMA/CA at moderate load
- CSMA/CA at low load
