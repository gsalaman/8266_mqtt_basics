# 8266_mqtt_basics

## MQTT Overview

MQTT stands for Message Queue Telemetry Transport.  It is a lightweight messaging protocol based on a "Publish/Subscribe" model (rather than "Client/Server").

[mqtt_broker_client.pdf](https://github.com/gsalaman/8266_mqtt_basics/files/11737146/mqtt_broker_client.pdf)

The central entity in an MQTT system is the Broker.  The broker is responsible for managing all the messages in the system; think of it as the "post office".

Users define Clients to talk with the broker.  Each client can:
* "Publish" messages (making them available to other clients)
* "Subscribe" to messages (reading things that other clients have published)

The beauty of MQTT is that the client doesn't need to know the details of how to deliver or recieve messages from other clients in the system...the broker deals with that.

Each MQTT message has a topic and a payload.

Clients can subscribe or publish to topics; they will only recieve messages that they've subscribed to.  Topics are structred like old-school directory listings...which we'll explain in the example below.

### Messaging Example
Consider a simple home automation system.  We'll have two clients: a "light controller" for the living room lights, and a "phone app" to control those lights.

Our main topic will be "living_room_lights". 

We'll say that our light_controller is connected to a switch than can turn on and off the lights.  Whenever someone flips the switch, it will publish a message:
| topic | payload |
| ----- | ------- |
| living_room_lights | "on" or "off" |

When the phone-app (or anyone else) wants to remotely turn the lights on or off, they can also send a message with the topic "living_room_lights" and payload "on" or "off".

The light controller client will **SUBSCRIBE** to the message; when it sees it, it will set the light either on or off.

One other nifty thing here:  the phone client can ALSO subscribe to that message so that it knows when someone flips the switch at the lights themselves.


## Getting the 8266 on WiFi

## Sample MQTT exchange
 
