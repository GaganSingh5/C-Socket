#define _GNU_SOURCE

// below are the c libaries which are being used in the code.
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <tar.h>

// below I have defined macros which I am using throughout the code to keep consistent values.
#define SERVER_PORT 6000
#define SERVER_ADDRESS "127.0.0.1"
#define DOMAIN AF_INET
#define SOCKET_TYPE SOCK_STREAM
#define MAX_PACKET_SIZE 1024
#define MAX_BUFFER_SIZE 8192

// Here I am defining a struct named Parsed command which will store the command request received from client.
// it has commandName, arguments array and number of arguments as its fields.
typedef struct
{
  char *commandName;
  char **arguments;
  int numArguments;
} ParsedCommand;

//  Here I am defining a struct which is used to store the result of command validation
// it has message and valiation status.
typedef struct
{
  char *message;
  int isValid;
} ValidatedCommand;

// this is a global variable which I am using to store the home path.
char *HOME;


int stopClient = 0;
void sigIntHandler()
{
  stopClient = 1;
}

// this is the function to show the prompt on the termial to before taking
// input from the user.
void clientw24Prompt()
{
  printf("\033[0;31m");
  printf("c");
  printf("\033[0;32m");
  printf("l");
  printf("\033[0;33m");
  printf("i");
  printf("\033[0;34m");
  printf("e");
  printf("\033[0;35m");
  printf("n");
  printf("\033[0;36m");
  printf("t");
  printf("\033[0;31m");
  printf("w");
  printf("\033[0;31m");
  printf("2");
  printf("\033[0;32m");
  printf("4");
  printf("\033[0;37m");
  printf("\U0001F41A");
  printf(" $ ");
  printf("\033[0m");
}

// This is a utility function which is used to conver the command string received from client
// to a structure defined by struct ParsedCommand.
// It has commandName, arguments and numberOfArguments are fields.
// Basically what it does is it converts the command request to tokens using strtok function.
ParsedCommand parseCommand(const char *commandString)
{
  // Created the struct and initalized it with default values.
  ParsedCommand parsedCommand;
  parsedCommand.commandName = NULL;
  parsedCommand.arguments = NULL;
  parsedCommand.numArguments = 0;

  // futher I am duplicating the request to process it
  char *commandDup = strdup(commandString);

  // then I am tokenizing the request at space.
  char *token = strtok((char *)commandDup, " ");

  // first token will be command.
  if (token != NULL)
  {
    parsedCommand.commandName = strdup(token);
  }

  // next all tokens will be arguments.
  while ((token = strtok(NULL, " ")))
  {
    // here I am dynamically increasing the size of the array by one.
    parsedCommand.arguments = realloc(parsedCommand.arguments, (parsedCommand.numArguments + 1) * sizeof(char *));
    if (parsedCommand.arguments == NULL)
    {
      fprintf(stderr, "Memory allocation failed\n");
      exit(EXIT_FAILURE);
    }
    // and then storing the arguments.
    parsedCommand.arguments[parsedCommand.numArguments++] = strdup(token);
  }

  return parsedCommand;
}

// this is the function used to get the prompt from the user using the terminal
// it uses fgets to read the input given by user on the terminal
ssize_t clientw24ReadPrompt(char **prompt)
{
  // I am getting the input from terminal and storing it in prompt char pointer
  fgets(*prompt, sizeof(char) * MAX_BUFFER_SIZE, stdin);

  // here i am checking if the length of provided input is greater
  // than zero and replace the last newLine character by null character.
  ssize_t lengthOfPrompt = strlen(*prompt);
  if (lengthOfPrompt > 0 && (*prompt)[lengthOfPrompt - 1] == '\n')
  {
    (*prompt)[lengthOfPrompt - 1] = '\0';
  }

  return lengthOfPrompt;
}

// this is the utility function use which is being used by the validator
// function to validate the date formats of w24fda and w24fdb commands.
int validateDateFormat(char *date)
{
  // here I am checking that date is of fixed length which is 10.
  if (strlen(date) != 10)
    return 1;

  // checking if the valid date seperator is being used.
  if (date[4] != '-' || date[7] != '-')
    return 1;

  // for each value checking it is valid number or not.
  for (int i = 0; i < 10; i++)
  {
    if (i == 4 || i == 7)
      continue;
    if (date[i] < '0' || date[i] > '9')
      return 1;
  }

  // for month and days checking if the value lies between correct range.
  int year, month, day;
  sscanf(date, "%d-%d-%d", &year, &month, &day);
  if (month < 1 || month > 12 || day < 1 || day > 31)
    return 1;

  return 0;
}

// this is the function used to validate the request provided by the user.
// based on the command and arguments it checks if the command structure
// is correct or not as per the specifications defined in the project.
ValidatedCommand validatePrompt(ParsedCommand command)
{
  ValidatedCommand valiated;

  valiated.isValid = 0;
  valiated.message = strdup("valid");

  // for dirlist check if the arguments are correct
  // and count is correct.
  if (strcmp("dirlist", command.commandName) == 0)
  {
    if (command.numArguments == 0 || strcmp("-a", command.arguments[0]) != 0 && strcmp("-t", command.arguments[0]) != 0)
    {
      valiated.isValid = 1;
      valiated.message = strdup("Unknown/No option passed, Allowed options: -a, -t.");
    }
    else if (command.numArguments > 1)
    {
      valiated.isValid = 1;
      valiated.message = strdup("Only 1 argument is allowed, Allowed options: -a or -t.");
    }
  }
  // for w24fn check if filename is passed and number of arguments are correct.
  else if (strcmp("w24fn", command.commandName) == 0)
  {
    if (command.numArguments > 1)
    {
      valiated.isValid = 1;
      valiated.message = strdup("Only one filename argument allowed.");
    }
    else if (command.numArguments <= 0)
    {
      valiated.isValid = 1;
      valiated.message = strdup("Filename argument is required.");
    }
  }
  // for w24fz check if exactly 2 arugments are passed and both are valid numbers.
  else if (strcmp("w24fz", command.commandName) == 0)
  {
    if (command.numArguments > 2 || command.numArguments < 2)
    {
      valiated.isValid = 1;
      valiated.message = strdup("Exactly two size arguments are allowed i.e. Size1 and Size2");
    }
    else
    {
      long size1;
      long size2;
      char *endptr1;
      char *endptr2;

      size1 = strtol(command.arguments[0], &endptr1, 10);
      size2 = strtol(command.arguments[1], &endptr2, 10);

      if (*endptr1 != '\0' || *endptr2 != '\0' || atoi(endptr1) != 0 || atoi(endptr2) != 0)
      {
        valiated.isValid = 1;
        valiated.message = strdup("Both the arguments should be a valid whole number.");
      }
      else if (size1 > size2)
      {
        valiated.isValid = 1;
        valiated.message = strdup("Size1 should be less than or equal to Size2.");
      }
      else if (size1 < 0 || size2 < 0)
      {
        valiated.isValid = 1;
        valiated.message = strdup("Both Size1 and Size2 should be greater the zero");
      }
    }
  }
  // for w24ft check if the number of arguments are between 1 and 3
  else if (strcmp("w24ft", command.commandName) == 0)
  {
    if (command.numArguments > 3 || command.numArguments < 1)
    {
      valiated.isValid = 1;
      valiated.message = strdup("Atleast 1 or atmost 3 extension arguments are required.");
    }
  }
  // for w24fdb andw24fda check if the argument count is valid and if the date passed
  // is in correct format and is a valid date.
  else if (strcmp("w24fdb", command.commandName) == 0 || strcmp("w24fda", command.commandName) == 0)
  {
    if (command.numArguments > 1 || command.numArguments < 1)
    {
      valiated.isValid = 1;
      valiated.message = strdup("Only one date argument is allowed.");
    }
    else
    {
      int isDateFormatValid = validateDateFormat(command.arguments[0]);
      if (isDateFormatValid == 1)
      {
        valiated.isValid = 1;
        valiated.message = strdup("Date format/value is invalid, Allowed format: YYYY-MM-DD");
      }
    }
  }
  // for quitc check if not arguments should be passed.
  else if (strcmp("quitc", command.commandName) == 0)
  {
    if (command.arguments > 0)
    {
      valiated.isValid = 1;
      valiated.message = strdup("No Arguments are allowed with quitc.");
    }
  }
  // if non of the command Name matches print unknown command error.
  else
  {
    valiated.isValid = 1;
    valiated.message = strdup("Unknown command.");
  }

  return valiated;
}


// this is the function used by the client to send data to the server
// as per the given fileDescriptor. It uses the send function from sockets 
// to send the data.
int sendData(int fileDescriptor, ssize_t sizeOfData, char *data)
{
  int sizeSendStatus = send(fileDescriptor, &sizeOfData, sizeof(ssize_t), 0);
  if (sizeSendStatus == -1)
  {
    return 1;
  }

  int dataSendStatus = send(fileDescriptor, data, sizeOfData, 0);
  if (dataSendStatus == -1)
  {
    return 1;
  }

  return 0;
}

// this is the function used by the client to receive the data
// from the passed file descriptor in the data char array.
// it basically uses the recv socket function to get the data in the buffer.
// first it gets the size of data then it creates an  buffer and receive data
// in that buffer.
int receiveData(int fileDescriptor, char **data)
{
  ssize_t responseSize = 0;

  int receivedSizeStatus = recv(fileDescriptor, &responseSize, sizeof(ssize_t), 0);

  if (receivedSizeStatus == -1)
  {
    return 1;
  }

  char *response = malloc(sizeof(char) * responseSize + 1);
  memset(response, 0, responseSize + 1);

  ssize_t totalDataReceived = 0;
  while (totalDataReceived < responseSize)
  {
    ssize_t dataSize = responseSize - totalDataReceived > MAX_PACKET_SIZE ? MAX_BUFFER_SIZE : responseSize - totalDataReceived;
    int bytesReceived = recv(fileDescriptor, response, dataSize, 0);
    if (bytesReceived == -1)
    {
      return 1;
    }
    totalDataReceived += bytesReceived;
  }

  *data = malloc(sizeof(char) * responseSize + 1);
  memset(*data, 0, responseSize + 1);

  memcpy(*data, response, responseSize);

  free(response);

  return 0;
}

// this is the function used by the client to receive a file
// from the given file descriptor. Basically what it does is it 
// first gets the size of file which will be received then it create a buffer of that 
// size then it creates the directory to store the file if not present.
// then it opens the file and receive the data in file.
int receiveFile(int fileDescriptor, char **path)
{
  ssize_t fileSize = 0;
  int receivedSizeStatus = recv(fileDescriptor, &fileSize, sizeof(ssize_t), 0);
  if (fileSize == 0)
  {
    return 2;
  }

  if (receivedSizeStatus == -1)
  {
    return 1;
  }

  char *filePath = malloc(PATH_MAX);
  strcpy(filePath, HOME);
  strcat(filePath, strdup("/w24project"));

  struct stat dirStatus;
  if (stat(filePath, &dirStatus) == -1)
  {
    if (mkdir(filePath, 0777) == -1)
    {
      perror("Error creating directory");
      return 1;
    }
    printf("Directory created successfully.\n");
  }

  strcat(filePath, "/temp.tar.gz");

  remove(filePath);

  int tarFileDescriptor = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0777);

  if (tarFileDescriptor < 0)
  {
    printf("Error Opening File\n");
    return 1;
  }

  char tarBuffer[MAX_PACKET_SIZE];
  ssize_t totalDataReceived = 0;
  while (totalDataReceived < fileSize)
  {
    memset(tarBuffer, 0, MAX_PACKET_SIZE);
    int bytesReceived = recv(fileDescriptor, tarBuffer, MAX_PACKET_SIZE, 0);
    if (bytesReceived == -1)
    {
      return 1;
    }
    totalDataReceived += bytesReceived;

    int bytesWrittenToFile = write(tarFileDescriptor, tarBuffer, MAX_PACKET_SIZE);
    if (bytesWrittenToFile < 0)
    {
      return 1;
    }
    printf("\rTotal Bytes Received: %ld", totalDataReceived);

    fflush(stdout);
  }
  *path = strdup(filePath);
  return 0;
}


// this is the main driver function which helps to connect to a server
// to handle the client requests. Basically what it does is it creats socket
// then creates an address variable to store the address of the server.
// and it uses the connect function to create a connection with the server
// after that it receives a fileDescriptor from the server to initiate the communication.

int main(int argc, char const *argv[])
{
  HOME = getenv("HOME");

  // create the socket.
  int server_fd = socket(DOMAIN, SOCKET_TYPE, 0);

  // create an address variable to store the Ip and port of the server.
  // to which the client will be connected.
  struct sockaddr_in *server_address = malloc(sizeof(struct sockaddr_in));

  if (server_fd < 0)
  {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  // initilize the server details.
  server_address->sin_family = DOMAIN;
  server_address->sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
  server_address->sin_port = htons(SERVER_PORT);

  socklen_t server_address_length = sizeof(*server_address);

  // call the connect function to create a connection with the client.
  if (connect(server_fd, (struct sockaddr *)server_address, server_address_length) < 0)
  {
    perror("Unable to connect to server");
    return 0;
  }


  // now client first send a WHICH_SERVER request to get the correct ip and port of the
  // server who will be handling the request of the client.
  char serverToConnect[13] = "WHICH_SERVER";
  int sendSuccess = sendData(server_fd, strlen(serverToConnect), serverToConnect);
  char *data;
  int receiveSuccess = receiveData(server_fd, &data);

  // after receiving the request extract the serverName, IP and Port.
  char *serverName = strtok(data, ":");
  char *serverIp = strtok(NULL, ":");
  char *serverPort = strtok(NULL, ":");

  printf("SERVER_NAME: %s\n", serverName);
  printf("SERVER_IP: %s\n", serverIp);
  printf("SERVER_PORT: %s\n", serverPort);

  char currentServerPort[4];
  sprintf(currentServerPort, "%d", SERVER_PORT);
  
  // check if the current connected server is the one who will handle the request
  // if not disconnect from the current server and connect with the new server
  // with the received details.
  if (strcmp(currentServerPort, serverPort) != 0)
  {

    close(server_fd);
    free(server_address);

    server_fd = socket(DOMAIN, SOCKET_TYPE, 0);

    server_address = malloc(sizeof(struct sockaddr_in));

    if (server_fd < 0)
    {
      perror("socket creation failed");
      exit(EXIT_FAILURE);
    }
    long sPort = strtol(serverPort, NULL, 10);
    server_address->sin_family = DOMAIN;
    server_address->sin_addr.s_addr = inet_addr(serverIp);
    server_address->sin_port = htons(sPort);

    server_address_length = sizeof(*server_address);

    if (connect(server_fd, (struct sockaddr *)server_address, server_address_length) < 0)
    {
      perror("Unable to connect to server");
      return 0;
    }
  }

  // after connecting the client goes into a loop
  // to get the request and process it through the server.
  printf("I am connected to %s\n", serverName);

  while (stopClient == 0)
  {
    fflush(0);
    // show the prompt to user to tell to end some input.
    clientw24Prompt();

    // get the request from the user.
    char *prompt = malloc(sizeof(char) * MAX_BUFFER_SIZE);
    ssize_t sizeOfPrompt = clientw24ReadPrompt(&prompt);

    if (strlen(prompt) <= 0 || prompt == NULL)
    {
      free(prompt);
      continue;
    }

    // parse the request to get the commandName and the arguments.
    ParsedCommand command = parseCommand(prompt);

    // next validate the request to check if it meets the specification of the 
    // project and show error message if there is any issue.
    ValidatedCommand validation = validatePrompt(command);

    if (validation.isValid == 1)
    {
      printf("%s\n", validation.message);
      free(prompt);
      continue;
    }

    // send the request to the server to handle the request.
    int sendSuccess = sendData(server_fd, sizeOfPrompt, prompt);

    if (sendSuccess == 1)
    {
      printf("Error While Sending Data to server. Please try again.\n");
      free(prompt);
      continue;
    }

    // based on the command passed in th requestion receieve the data 
    // and show it to the user on the client.
    if (strcmp("dirlist", command.commandName) == 0 || strcmp("w24fn", command.commandName) == 0)
    {
      char *data;
      int receiveSuccess = receiveData(server_fd, &data);

      if (receiveSuccess == 1)
      {
        printf("Error While Receiving Data from server. Please try again.\n");
        free(prompt);
        continue;
      }

      printf("%s\n", data);

      free(data);
      free(prompt);
    }
    else if (strcmp("w24fz", command.commandName) == 0 || strcmp("w24fda", command.commandName) == 0 || strcmp("w24fdb", command.commandName) == 0 || strcmp("w24ft", command.commandName) == 0)
    {
      char *path;
      int receiveSuccess = receiveFile(server_fd, &path);

      if (receiveSuccess == 2)
      {
        printf("Tar Not Created as no files satisfies the condition\n");
        free(prompt);
        continue;
      }

      if (receiveSuccess == 1)
      {
        printf("Error While Receiving File from server. Please try again.\n");
        free(prompt);
        continue;
      }

      printf("\nTar Received: %s\n", path);

      free(path);
      free(prompt);
    }
    else if (strcmp("quitc", command.commandName) == 0)
    {
      free(prompt);
      break;
    }
  }
  free(server_address);
  close(server_fd);

  return 0;
}
