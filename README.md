# 8266_mqtt_basics

## MQTT Overview

MQTT stands for Message Queue Telemetry Transport.  It is a lightweight messaging protocol based on a "Publish/Subscribe" model (rather than "Client/Server").

![Slide1](https://github.com/gsalaman/8266_mqtt_basics/assets/43499190/4ef1422d-7050-4fb4-aabd-ed340ee50233)

The central entity in an MQTT system is the Broker.  The broker is responsible for managing all the messages in the system; think of it as the "post office".

Users define Clients to talk with the broker.  Each client can:
* "Publish" messages (making them available to other clients)
* "Subscribe" to messages (reading things that other clients have published)

The beauty of MQTT is that the client doesn't need to know the details of how to deliver or recieve messages from other clients in the system...the broker deals with that.

Each MQTT message has a topic and a payload.

Clients can subscribe or publish to topics; they will only recieve messages that they've subscribed to.  

### Messaging Example
Consider a simple home automation system.  We'll have two clients: a "light controller" for the living room lights, and a "phone app" to control those lights.

Our main topic will be "living_room_lights". 

We'll say that our light_controller is connected to a switch than can turn on and off the lights.  Whenever someone flips the switch, it will publish a message:
| topic | payload |
| ----- | ------- |
| living_room_lights | "on" or "off" |

When the phone-app (or anyone else) wants to remotely turn the lights on or off, they can also send a message with the topic "living_room_lights" and payload "on" or "off".

The light controller client will **SUBSCRIBE** to the "living_room_lights" topic; when it sees it, it will set the light either on or off.

One other nifty thing here:  the phone client can ALSO subscribe to that message so that it knows when someone flips the switch at the lights themselves.


## Getting the 8266 connected
Now that we're using MQTT as our upper-level protocol, we need some way to connect our devices to the internet.  In this example, we'll be using WiFi, courtesy of the "ESP8266Wifi.h" library.

We're going to program the 8266 with the SSID and Password for your wifi network, but we're NOT going to hardcode those into the code for security reasons...instead, we'll program those into Non-volatile memory  Excercise for the reader:  why would hardcoding be bad?

Then, we'll point our client at a broker.  At this point, we're ready to exchange MQTT messages.

### The State Machine
We've got 4 states:
* Offline (where we set SSID, Password, and Broker)
* Disconnected (where we're looking for WiFi)
* Looking for Broker
* Active (where we're ready to exchange MQTT messages)

We've got the following enum to define these:
```
/* state of "how far" we're connected */
typedef enum
{
  STATE_OFFLINE = 0,             // Not looking for network...inputting parameters.
  STATE_DISCONNECT,              // Disconnected, but looking for WiFi
  STATE_LOOKING_FOR_BROKER,      // Connected to WiFi and looking for broker
  STATE_ACTIVE                   // Connected to broker and exchanging MQTT messages.
} state_type;
```

Each state will have an initialization function and a processing function.  Each of the processing functions returns the `state_type` of our next state.

The main loop has a big switch statement...it checks our current state and calls the appropriate processing function.

One side note here:  we're looking for any serial input to kick us into `STATE_OFFLINE` to configure our NV parameters.  This is coded a little klunkilly; bonus points for cleaning this up in your implementation.







 
