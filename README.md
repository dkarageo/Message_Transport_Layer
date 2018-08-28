# **Message Transport Layer (MTL)**

## A TCP-based message exchanging system for local area networks targeting low power embedded systems.


Developed by *Dimitrios Karageorgiou*,\
during the course *Embedded and Realtime Systems*,\
*Aristotle University Of Thessaloniki, Greece,*\
*2017-2018.*


**Message Transport Layer (MTL)** is a message exchanging system on local area networks (LANs). It is based on TCP protocol and provides reliable message transfer. It consists of a server module and a client service. Server module -the core service of MTL- is expected to continuously run on low-power embedded devices such as routers or other low-power computers. Client service can be used by any client application that wants to communicate with MTL server for exchanging messages.

Server acts as a forwarder for the messages. It receives messages from connected clients and forwards them to their destinations. If a destination is offline, message is NACKed through a simple Negative Acknowledging mechanism. MTL-level error correction capabalities are solely based on this Negative Acknowledging mechanism, thus saving bandwidth compared to a possible Positive Acknowledging mechanism. In any case, error handling on MTL-level is expected to be something rare, if at all needed by the applications that make use of it.

Original target of the server was *ZSun Wi-Fi card reader*, running an *OpenWRT* port as its OS.  


### How to compile:

`make` : Builds both a server and a demo client for testing.

`make server` : Builds only server.

`make client` : Builds only demo client.

Executables are located inside `bin` folder under project's root.

In order to successfully compile, a compiler that supports GNU-11 C standard is required.


### How to run server:

```
./bin/server <port> [<log_file> [<min_rate> <step> <max_rate> <period>]]
```

where:
- port : Port number to be used by server.
- log_file [optional] : Path to a file that will be used for storing log data.
- min_rate [optional] : Minimum sending rate of MTL in messages/sec.
- step [optional] : Step of reduction for sending rate of MTL in messages/sec.
- max_rate [optional] : Max sending rate of MTL in messages/sec.
- period [optional]: Period of rate limiter in milliseconds (ms) to reduce rate by given step.

*min_rate*, *step*, *max_rate* and *period* provides a way to setup a rate limiter that periodically reduces sending rate of MTL server. It starts from *max_rate* and at each *period* reduces sending rate by *step*. When rate drops below *min_rate* it starts again from *max_rate*. Normally, rate limiter is expected to be turned-off (i.e. none of the last four args provided). Though, it's useful for conducting various tests.


### Demo client:

In order to experiment with MTL and conduct a series of tests, a demo client is implemented. Demo client can operate either in **interactive mode** or in **testing mode**.

#### Interactive mode:
At this mode, demo client reads messages from *stdin* in the form of `destIP:destPort message` and sends them. Client in interactive mode can be invoked as following:

```
./bin/demo_client <server_hostname> <server_port> -mode=i <port>
```

where:
- server_hostname : IPv4 address in dot format or hostname of server.
- server_port : Port number on server where MTL service is running.
- port : Port which will be used by demo client.

#### Testing mode:

In testing mode, demo client is able to start multiple clients at once. It is also able to generate and exchange a given amount of messages between started clients, while verifying their correct receipt. Testing mode can be invoked with two different destination selecting options. It can either be invoked with `random` argument, which selects as destination for each started client a random client from the rest, or with `all` argument, where each client sends the specified amount of messages to all other clients. The executable can be invoked as following:

```
./bin/demo_client <server_hostname> <server_port> -mode=t <clients_num> <send_mode> <messages_num> <if_ip>
```
where:
- server_hostname : IPv4 address in dot format or hostname of server.
- server_port : Port number on server where MTL service is running.
- clients_num : Number of clients to be started.
- send_mode : 'all' for send to all, 'random' for send to random
- messages_num : Number of messages to be send by a client to each of its targets.
- if_ip : IP assigned to the interface which will be used for communicating with the server. It should be the IP visible to the server. If device is behind a NAT, the public IP of the NAT should be provided.


### Licensing:

This project is licensed under GNU GPL v3.0 license. A copy of this license is contained in current project.
