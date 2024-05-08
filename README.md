# $NETSHELL
![Static Badge](https://img.shields.io/badge/C-90D26D?style=flat-square) ![Static Badge](https://img.shields.io/badge/Socket%20Programming-FFDB5C?style=flat-square) ![Static Badge](https://img.shields.io/badge/Load%20Balancer-5AB2FF?style=flat-square)


NetShell is a conceptualization of client server architecture written using sockets programming in C language. It helps to execute Linux shell commands over the network.
Multiple clients can connect to the server and can execute the commands. Each client request is handled separately by the server by forking a separate process. It also has a load balancer to redirect requests to mirror servers based on the number of clients connected for efficient use of the server resources.
