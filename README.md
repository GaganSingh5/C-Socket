# $NETSHELL
![Static Badge](https://img.shields.io/badge/C-90D26D?style=flat-square) ![Static Badge](https://img.shields.io/badge/Socket%20Programming-FFDB5C?style=flat-square) ![Static Badge](https://img.shields.io/badge/Load%20Balancer-5AB2FF?style=flat-square)

NetShell is a client-server architecture implemented in C using sockets programming. It facilitates the execution of Linux shell commands over a network. The server can handle multiple clients concurrently by forking separate processes for each client request. Additionally, a load balancer is incorporated to distribute requests to mirror servers, optimizing server resource utilization based on the number of connected clients.
