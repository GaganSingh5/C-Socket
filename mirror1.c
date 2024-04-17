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
#include <sys/mman.h>
#include <sys/wait.h>

// below I have defined macros which I am using throughout the code to keep consistent values.
#define PORT 7000
#define ADDRESS "127.0.0.1"
#define DOMAIN AF_INET
#define SOCKET_TYPE SOCK_STREAM
#define MAX_PACKET_SIZE 1024
#define INITIAL_ARRAY_CAPACITY 10

long clientsCount = 0;
    // Here I have defined an enum type which I am using to track the success or failure response of an operation.
    typedef enum {
      RESPONSE_SUCCESS,
      RESPONSE_FAILURE,
    } ResponseStatus;

// Here I am defining a struct named Parsed command which will store the command request received from client.
// it has commandName, arguments array and number of arguments as its fields.
typedef struct
{
  char *commandName;
  char **arguments;
  int numArguments;
} ParsedCommand;

// Here I have defined a struct Array which will store elements.
// it has capacity and track the number of elements using size.
typedef struct
{
  char **elements;
  long capacity;
  long size;
} Array;

// Here I have defined a struct to store the response of the operation after execution of the command.
// it store the response as byte streams and has a status to denote failure or success.
typedef struct
{
  char *response;
  ResponseStatus responseStatus;
} CommandResponse;

// this is a global variable which I am using to store the home path.
char *HOME_PATH;

// this is a utility function which is used to initialize the server by providing correct Address and Port.
// it opens a socket and then creates an address and binds the address values to the created socket.
// at last it returns the created socket descriptor.
int initServer()
{
  // created Socket.
  int server_fd = socket(DOMAIN, SOCKET_TYPE, 0);

  // create an address element of type internal socket address to store the port, domain and address.
  struct sockaddr_in *server_address = malloc(sizeof(struct sockaddr_in));

  // checking if the socket creating was success or not
  if (server_fd < 0)
  {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }
  // initializing the address element with serverIP address and port.
  server_address->sin_family = DOMAIN;
  server_address->sin_port = htons(PORT);
  server_address->sin_addr.s_addr = inet_addr(ADDRESS);

  // getting the size of the addres element created above.
  socklen_t server_socklen = sizeof(*server_address);

  // here I am binding the socket created above to the initialized address and port above.
  if (bind(server_fd, (const struct sockaddr *)server_address, server_socklen) < 0)
  {
    perror("Error! Bind has failed");
    exit(EXIT_FAILURE);
  }
  // at last I am returing the socket file descriptor to use.
  return server_fd;
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

  // then I am returning the parseCommand
  return parsedCommand;
}

// this is the utility function I am using to execute a linux command using execvp
// this function takes two arguments one is the argsVector where the command is stored.
// and second is the pointer to response which will store the result after the execution of the command.
// In this function I am using temp.buff file as a buffer to redirect the stdout to this file and
// then reading the file to get the response.
void executeCommand(char *argsVector[], CommandResponse *response)
{

  // here I am opeing the temp buffer file with correct flags and permission.
  int tempFileDescriptor = open("./temp.buff", O_APPEND | O_RDWR | O_CREAT, 0600);
  if (tempFileDescriptor < 0)
  {
    response->response = "Error while execution please try again.";
    response->responseStatus = RESPONSE_FAILURE;
    return;
  }
  // duplicating the stdout so I am reset it later.
  int ogStdOut = dup(STDOUT_FILENO);

  // forking a child to execute the command.
  int childProcessID = fork();

  // checking if child wa created or not.
  if (childProcessID < 0)
  {
    response->response = "Error while execution please try again.";
    response->responseStatus = RESPONSE_FAILURE;
    return;
  }

  // in the child process  I am redirecting the stdout to the buffer file
  // and then executing the command using execvp passing the argsVector.
  if (childProcessID == 0)
  {
    dup2(tempFileDescriptor, STDOUT_FILENO);

    execvp(argsVector[0], argsVector);

    response->response = "Error while execution please try again.";
    response->responseStatus = RESPONSE_FAILURE;

    dup2(ogStdOut, STDOUT_FILENO);

    close(tempFileDescriptor);
    return;
  }
  // In the else part which is the parent process it is waiting for
  // child to complete, after the child process is done processing it resets the stdout
  // then is gets the size of the file buffer using the lseek
  // after that it sets the pointer to start of file. and copies the content of the buffer file to
  // CommandResponse varibale. At last it closes the fd and removes the file buffer.
  else
  {
    wait(NULL);
    // reset the stdout.
    dup2(ogStdOut, STDOUT_FILENO);

    // get size of file.
    long bufferSize = lseek(tempFileDescriptor, 0, SEEK_CUR);

    if (bufferSize == 0)
    {
      response->response = "";
      response->responseStatus = RESPONSE_SUCCESS;

      return;
    }
    // points to start of file
    lseek(tempFileDescriptor, 0, SEEK_SET);

    // initialize the memory and loop over the buffer to extract the content.
    response->response = (char *)malloc(sizeof(char) * bufferSize + 1);
    memset(response->response, 0, bufferSize + 1);

    ssize_t bytesRead;
    ssize_t totalByteRead = 0;
    while ((bytesRead = read(tempFileDescriptor, response->response + totalByteRead, sizeof(4096))) > 0)
    {
      totalByteRead += bytesRead;
    }

    // closeing the descirptor and removing the buffer file.
    close(tempFileDescriptor);

    remove("./temp.buff");

    response->responseStatus = RESPONSE_SUCCESS;
    return;
  }
}

// this is the utility function used to get the permission of the file
// defined in the struct stat argument.
// it generate a formatted string by getting all types of permissions.
char *getFilePermissions(struct stat *fileStat)
{
  static char permissions[10];

  memset(permissions, '-', sizeof(permissions));

  permissions[0] = '-';
  permissions[1] = (fileStat->st_mode & S_IRUSR) ? 'r' : '-';
  permissions[2] = (fileStat->st_mode & S_IWUSR) ? 'w' : '-';
  permissions[3] = (fileStat->st_mode & S_IXUSR) ? 'x' : '-';
  permissions[4] = (fileStat->st_mode & S_IRGRP) ? 'r' : '-';
  permissions[5] = (fileStat->st_mode & S_IWGRP) ? 'w' : '-';
  permissions[6] = (fileStat->st_mode & S_IXGRP) ? 'x' : '-';
  permissions[7] = (fileStat->st_mode & S_IROTH) ? 'r' : '-';
  permissions[8] = (fileStat->st_mode & S_IWOTH) ? 'w' : '-';
  permissions[9] = (fileStat->st_mode & S_IXOTH) ? 'x' : '-';

  return permissions;
}

// this is the utility function used to create the tar file based on the file paths passed in arguments
// it uses executeCommand function internally to execute the linux tar command with path arguments.
// to create the temp.tar.gz which will passed to client.
void createTar(Array *filePaths)
{
  CommandResponse tarResponse;
  char *argsVector[filePaths->size + 6];

  argsVector[0] = "tar";
  argsVector[1] = "czf";
  argsVector[2] = "./temp.tar.gz";
  argsVector[3] = "--transform";
  argsVector[4] = "s|.*/||";
  for (int iterator = 0; iterator < filePaths->size; iterator++)
  {
    argsVector[iterator + 5] = filePaths->elements[iterator];
  }
  argsVector[filePaths->size + 5] = NULL;

  printf("Creating Tar File...\n");
  executeCommand(argsVector, &tarResponse);
}

// This is a utility function used to initilize the array structure defined above.
// basically is initialize the array passed as pointer argument with default size and set capacity.
void initArray(Array *array)
{
  array->elements = malloc(INITIAL_ARRAY_CAPACITY * sizeof(char *));
  if (array->elements == NULL)
  {
    fprintf(stderr, "Memory allocation failed\n");
  }
  array->size = 0;
  array->capacity = INITIAL_ARRAY_CAPACITY;
}

// This is the utility function used by array to add element into the array.
// it also checks for the capacity of the array and if array is full it dynamically increases the size
// of the array.
void addElement(Array *array, char *element)
{
  // checking if the size is greater then the capacity then double the capacity
  if (array->size >= array->capacity)
  {
    array->capacity *= 2;
    array->elements = realloc(array->elements, array->capacity * sizeof(char *));
    if (array->elements == NULL)
    {
      fprintf(stderr, "Memory allocation failed\n");
    }
  }

  // insert the element in the array at given size.
  array->elements[array->size] = strdup(element);
  if (array->elements[array->size] == NULL)
  {
    fprintf(stderr, "Memory allocation failed\n");
  }
  array->size++;
}

// this is the utility function used by array to free the allocated memory
// after operation is done.
void freeArray(Array *array)
{
  for (size_t iterator = 0; iterator < array->size; iterator++)
  {
    free(array->elements[iterator]);
  }
  free(array->elements);
}

// this is a utility function used by one of the command w24ft to check if the file has
// any of the given extension. It basically get the baseName of the file from the path then extract
// the extension, iterate over each extension comparing it with the given extension.
int checkExtension(char *filePath, char **extensions, int numOfExtensions)
{
  const char *baseName = basename(filePath);
  const char *extension = strrchr(baseName, '.');

  if (extension != NULL)
  {
    extension++;
    for (int i = 0; i < numOfExtensions; i++)
    {
      if (strcmp(extension, extensions[i]) == 0)
      {
        return 0;
      }
    }
  }

  return 1;
}

// this is function use to handle the command dirlist -a
// It basically executes ls command over home directory using executeCommand function
//  and then for each path it check if the its a valid directory or not.
// and stores the directory in the response buffer.
CommandResponse dirlist_a()
{
  // create response Struct.
  CommandResponse response;

  // execute ls command.
  char *argsVector[] = {"ls", HOME_PATH, NULL};
  executeCommand(argsVector, &response);

  // get the size of response and create a buffer of that size.
  long bufferSize = strlen(response.response);
  char processedBuffer[bufferSize];
  memset(processedBuffer, 0, bufferSize);

  // tokensize the response from executeCommand and checks for valid directory
  // using the stat system call
  char *token = strtok(response.response, "\n");

  if (token == NULL)
  {
    response.response = "No Directory Found";
    response.responseStatus = RESPONSE_SUCCESS;
    return response;
  }

  char fullPath[PATH_MAX];
  sprintf(fullPath, "%s/%s", HOME_PATH, token);
  struct stat fileStat;

  if (stat(fullPath, &fileStat) != -1)
  {
    if (S_ISDIR(fileStat.st_mode))
    {
      strcpy(processedBuffer, token);
      strcat(processedBuffer, "\n");
    }
  }

  while (token = strtok(NULL, "\n"))
  {
    sprintf(fullPath, "%s/%s", HOME_PATH, token);

    if (stat(fullPath, &fileStat) != -1)
    {
      if (S_ISDIR(fileStat.st_mode))
      {
        strcat(processedBuffer, token);
        strcat(processedBuffer, "\n");
      }
    }
  }

  response.response = strdup(processedBuffer);
  return response;
}

// this is the function which handles the logic of dirlist -t command
// It basically executes ls command with t option over home directory using executeCommand function
//  and then for each path it check if the its a valid directory or not.
// if the directory is valid it adds the directory name to the stack created
// then it iterates over the stack in reverse order and create formatted string of directories ordered by
// creation date.
CommandResponse dirlist_t()
{
  // Create response Struct and execute the ls with -t
  CommandResponse response;
  char *argsVector[] = {"ls", "-t", HOME_PATH, NULL};

  executeCommand(argsVector, &response);

  // create a stack to store response
  char *stack[1000];
  int top = -1;

  // gets the size of response and create a buffer.
  long bufferSize = strlen(response.response);
  char processedBuffer[bufferSize];
  memset(processedBuffer, 0, bufferSize);

  // tokenize the response and checks if each path is valid and is a dirtory or not.
  // further it adds the directory to the stack.
  char *token = strtok(response.response, "\n");

  if (token == NULL)
  {
    response.response = "No Directory Found";
    response.responseStatus = RESPONSE_SUCCESS;
    return response;
  }

  char fullPath[PATH_MAX];
  sprintf(fullPath, "%s/%s", HOME_PATH, token);
  struct stat fileStat;

  if (stat(fullPath, &fileStat) != -1)
  {
    if (S_ISDIR(fileStat.st_mode))
    {
      stack[++top] = strdup(token);
    }
  }

  while (token = strtok(NULL, "\n"))
  {
    sprintf(fullPath, "%s/%s", HOME_PATH, token);

    if (stat(fullPath, &fileStat) != -1)
    {
      if (S_ISDIR(fileStat.st_mode))
      {
        stack[++top] = strdup(token);
      }
    }
  }

  // then it creates a formatted string out the directories in stack
  // in reverse order.
  while (top > -1)
  {
    strcat(processedBuffer, stack[top--]);
    strcat(processedBuffer, "\n");
  }

  response.response = strdup(processedBuffer);
  return response;
}

// this is the function used to handle the logic for command w24fn filename
// basically what it does is it executes the find command using the executeCommand function
// passing the filename as an option to find.
// after getting the response. It gets the first path of the filename
// using strtok and then using stat system call it gets the details regarding the file.
CommandResponse w24fn_filename(char *filename)
{
  // defining the response Struct and calling the find command with filename option
  // usint the executeCommand function.
  CommandResponse response;

  char *argsVector[] = {"find", HOME_PATH, "-type", "f", "-name", filename, NULL};

  executeCommand(argsVector, &response);

  // after getting the response, get the first file if there are multiple
  // using strtok.
  char *token = strtok(response.response, "\n");
  if (token == NULL)
  {
    response.response = strdup("File Not Found");
    response.responseStatus = RESPONSE_SUCCESS;
    return response;
  }

  // defining the stat struct and calling that stat syscall passing the file
  // to get details of the file.
  struct stat fileStat;
  if (stat(token, &fileStat) == -1)
  {
    perror("stat");
    response.response = strdup("Execution Failed");
    response.responseStatus = RESPONSE_FAILURE;
    return response;
  }

  // getting permission string.
  char *permissions = getFilePermissions(&fileStat);

  // getting creation date.
  char formattedDate[100];

  strftime(formattedDate, sizeof(formattedDate), "%Y-%m-%d %H:%M:%S", localtime(&fileStat.st_ctime));

  // creating a formatted output out of all the details using sprintf.

  char *formattedDetails = malloc(sizeof(char) * 500);
  memset(formattedDetails, 0, 500);
  sprintf(formattedDetails, "Filename: %s\nSize(Bytes): %ld bytes\nCreatedOn: %s\nPermissions: %s\nPath: %s\n",
          filename, fileStat.st_size, formattedDate, permissions, token);
  response.response = strdup(formattedDetails);

  return response;
}

// this is the function to handle logic of command w24fz size1 size2
// basically what it does is it gets the file paths which has size between size1 and size2 passed as arugments.
// using the find command. Then it creates an array to store th paths.
// further it tokenizes the response from execute and adds each path to array.
// this it calls the createTar function passing the paths array as argument to create the tar file to
// be send to the client.
CommandResponse w24fz_size1_size2(char *minSize, char *maxSize)
{
  char **argsVector;
  CommandResponse response;

  int minSizeInt = atoi(minSize);
  int maxSizeInt = atoi(maxSize);

  if (strcmp(minSize, maxSize) == 0)
  {
    char minSizeArg[20];
    sprintf(minSizeArg, "%sc", minSize);

    argsVector = malloc(7 * sizeof(char *));
    if (argsVector == NULL)
    {
      response.response = strdup("Error while execution");
      response.responseStatus = RESPONSE_SUCCESS;
      return response;
    }
    argsVector[0] = "find";
    argsVector[1] = HOME_PATH;
    argsVector[2] = "-type";
    argsVector[3] = "f";
    argsVector[4] = "-size";
    argsVector[5] = minSizeArg;
    argsVector[6] = NULL;
  }
  else
  {
    minSizeInt--;
    maxSizeInt++;

    // getting the size to be passed to find.
    char minSizeArg[20];
    char maxSizeArg[20];
    sprintf(minSizeArg, "+%dc", minSizeInt);
    sprintf(maxSizeArg, "-%dc", maxSizeInt);

    argsVector = malloc(10 * sizeof(char *));
    if (argsVector == NULL)
    {
      response.response = strdup("Error while execution");
      response.responseStatus = RESPONSE_SUCCESS;
      return response;
    }

    argsVector[0] = "find";
    argsVector[1] = HOME_PATH;
    argsVector[2] = "-type";
    argsVector[3] = "f";
    argsVector[4] = "-size";
    argsVector[5] = minSizeArg;
    argsVector[6] = "-a";
    argsVector[7] = "-size";
    argsVector[8] = maxSizeArg;
    argsVector[9] = NULL;
  }

  // calling the find linux command.
  executeCommand(argsVector, &response);

  // Creating array to store the paths.
  Array paths;
  initArray(&paths);

  // tokenizing the response to get paths and process it.
  char *token = strtok(response.response, "\n");
  if (token == NULL)
  {
    response.response = strdup("NO_TAR");
    response.responseStatus = RESPONSE_SUCCESS;
    return response;
  }

  addElement(&paths, token);

  while ((token = strtok(NULL, "\n")))
  {
    addElement(&paths, token);
  }

  if (paths.size == 0)
  {
    printf("NO TAR: %ld\n", paths.size);
    response.response = strdup("NO_TAR");
    response.responseStatus = RESPONSE_SUCCESS;
    return response;
  }

  // creating the tar file with the given paths.

  createTar(&paths);

  response.response = strdup("./temp.tar.gz");
  response.responseStatus = RESPONSE_SUCCESS;
  return response;
}

// this is the function used to handle the logic for the command w24ft extension
// basically what it does is it calls the find linux command to get all the files present in the system
// then it tokenizes the response string and add paths to array. Further for each path
// it checks if the file extension is one of the extension passed as arguments.
// then it passes all the valid paths to createTar function to create a tar file.
CommandResponse w24ft_extension(char **arguments, int argumentsCount)
{
  // execute the find command to get all files.
  CommandResponse response;
  char *argsVector[] = {"find", HOME_PATH, "-type", "f", NULL};

  executeCommand(argsVector, &response);

  // create array to store paths.
  Array paths;
  initArray(&paths);

  // tokenize the response to get individual paths.
  char *token = strtok(response.response, "\n");

  if (token == NULL || strlen(token) == 0)
  {
    response.response = strdup("NO_TAR");
    response.responseStatus = RESPONSE_SUCCESS;
    return response;
  }

  struct stat st;

  // check if the file is a of valid extension and also a valid path.
  if (checkExtension(token, arguments, argumentsCount) == 0 && stat(token, &st) != -1)
  {
    addElement(&paths, token);
  }

  // repeate for each path
  while ((token = strtok(NULL, "\n")))
  {
    if (checkExtension(token, arguments, argumentsCount) == 0 && stat(token, &st) != -1)
    {
      addElement(&paths, token);
    }
  }

  // create the tar file with the valid paths.
  createTar(&paths);

  printf("Tar Created\n");
  response.response = strdup("./temp.tar.gz");
  response.responseStatus = RESPONSE_SUCCESS;
  return response;
}

// this is the function used to handle logic for command w24fdb date
// basically what it does is it executes the find command with newermt option with a not passing the
// date to get files whose creation date is less then given date.
// then it creates and array add all the paths to array after tokenizing it
// and then pass it to the createTar function to create the tar file
CommandResponse w24fdb_date(char *date)
{
  // execute the find command with date.
  CommandResponse response;
  char *argsVector[] = {"find", HOME_PATH, "-type", "f", "!", "-newermt", date, NULL};

  executeCommand(argsVector, &response);

  // create an array to store paths
  Array paths;
  initArray(&paths);

  // tokenize all the paths from response.
  char *token = strtok(response.response, "\n");

  if (token == NULL || strlen(token) == 0)
  {
    response.response = strdup("NO_TAR");
    response.responseStatus = RESPONSE_SUCCESS;
    return response;
  }

  // and add it to the array.
  addElement(&paths, token);

  while ((token = strtok(NULL, "\n")))
  {
    addElement(&paths, token);
  }

  // if (paths.size == 0)
  // {
  //   printf("NO TAR: %ld\n", paths.size);
  //   response.response = strdup("NO_TAR");
  //   response.responseStatus = RESPONSE_SUCCESS;
  //   return response;
  // }

  // create the tar file out of all the paths.
  createTar(&paths);

  response.response = strdup("./temp.tar.gz");
  response.responseStatus = RESPONSE_SUCCESS;
  return response;
}

// this is the function used to handle logic for command w24fdb date
// basically what it does is it executes the find command with newermt option passing the
// date to get files whose creation date is less then given date.
// then it creates and array add all the paths to array after tokenizing it
// and then pass it to the createTar function to create the tar file
CommandResponse w24fda_date(char *date)
{
  // execute the find command with date.
  CommandResponse response;
  char *argsVector[] = {"find", HOME_PATH, "-type", "f", "-newermt", date, NULL};

  executeCommand(argsVector, &response);

  // create an array to store the paths.
  Array paths;
  initArray(&paths);

  // tokenize the response to get individual paths.
  char *token = strtok(response.response, "\n");

  if (token == NULL || strlen(token) == 0)
  {
    response.response = strdup("NO_TAR");
    response.responseStatus = RESPONSE_SUCCESS;
    return response;
  }

  // add it to array
  addElement(&paths, token);

  while ((token = strtok(NULL, "\n")))
  {
    addElement(&paths, token);
  }

  // if (paths.size == 0)
  // {
  //   printf("NO TAR: %ld\n", paths.size);
  //   response.response = strdup("NO_TAR");
  //   response.responseStatus = RESPONSE_SUCCESS;
  //   return response;
  // }

  // create tar out of all paths in array.
  createTar(&paths);

  printf("Tar Created:\n");
  response.response = strdup("./temp.tar.gz");
  response.responseStatus = RESPONSE_SUCCESS;
  return response;
}

// this is a utility function use to receive data from the fileDescriptor passed. in the data array.
// it basically uses the recv funtion to first read the size of data which will be received.
// then it creates a buffer of that size and then receive the data from the client.
// further it copies the data to the data variable.
int receiveData(int fileDescriptor, char **data)
{
  ssize_t responseSize = 0;

  int receivedSize = recv(fileDescriptor, &responseSize, sizeof(ssize_t), 0);

  if (receivedSize == -1 || receivedSize == 0)
  {
    return -1;
  }

  char *response = malloc(sizeof(char) * responseSize);

  int bytesReceived = recv(fileDescriptor, response, responseSize, 0);

  if (bytesReceived == -1 || bytesReceived == 0)
  {
    return -1;
  }

  if (bytesReceived < responseSize)
  {
    return 1;
  }

  *data = malloc(sizeof(char) * responseSize);

  memcpy(*data, response, responseSize);

  free(response);

  return 0;
}

// this is a utility function to send data to the given fileDescriptor.
// basically it takes the file descriptor, size of data and data itself and calls the send function
// to send the data to client. First it sends the size of the data to tell the client how much data will be
// received and then it send the data in a buffered manner.
int sendData(int fileDescriptor, ssize_t sizeOfData, char *data)
{
  int sizeSendStatus = send(fileDescriptor, &sizeOfData, sizeof(ssize_t), 0);
  if (sizeSendStatus == -1)
  {
    return 1;
  }

  char *response = malloc(sizeof(char) * sizeOfData);

  memcpy(response, data, sizeOfData);

  ssize_t totalDataSent = 0;
  while (totalDataSent < sizeOfData)
  {
    ssize_t dataSizeToSend = sizeOfData - totalDataSent > MAX_PACKET_SIZE ? MAX_PACKET_SIZE : sizeOfData - totalDataSent;
    int bytesSent = send(fileDescriptor, data, dataSizeToSend, 0);
    if (bytesSent == -1)
    {
      return 1;
    }
    totalDataSent += bytesSent;
  }

  free(response);

  return 0;
}

// this is a utiltity function used to send file type to the given fileDescriptor.
// it takes the fileDescriptor of the client and the filePath to send.
// it uses stat to check if the file is valid or not and then uses the send command
// to send the data in a buffered manner 1000 bytes at a time.
int sendFile(int fileDescriptor, char *filePath)
{
  ssize_t fileSize = 0;
  struct stat fileStat;
  if (stat(filePath, &fileStat) == -1)
  {
    perror("stat");
    return 1;
  }

  fileSize = fileStat.st_size;
  send(fileDescriptor, &fileSize, sizeof(size_t), 0);

  char fileBuffer[MAX_PACKET_SIZE];
  memset(fileBuffer, 0, MAX_PACKET_SIZE);
  ssize_t totalBytesSent = 0;

  int tempFileDescriptor = open(filePath, O_RDONLY);
  while (totalBytesSent < fileSize)
  {
    memset(fileBuffer, 0, MAX_PACKET_SIZE);
    read(tempFileDescriptor, fileBuffer, MAX_PACKET_SIZE);
    totalBytesSent += send(fileDescriptor, fileBuffer, MAX_PACKET_SIZE, 0);
    printf("\rTotal Bytes Sent To Client: %ld", totalBytesSent);
    fflush(stdout);
  }

  printf("\n");
  close(tempFileDescriptor);
  remove(filePath);
}

// this function is used to handle the client request. based on the command passed
// it basically checks the command and calls the specific function to handle the request.
void handleRequest(ParsedCommand command, int clientFD)
{
  if (strcmp("dirlist", command.commandName) == 0)
  {
    if (strcmp("-a", command.arguments[0]) == 0)
    {
      CommandResponse resp = dirlist_a();

      size_t responseSize = strlen(resp.response);

      sendData(clientFD, responseSize, resp.response);
    }
    else if (strcmp("-t", command.arguments[0]) == 0)
    {
      CommandResponse resp = dirlist_t();

      size_t responseSize = strlen(resp.response);

      sendData(clientFD, responseSize, resp.response);
    }
  }
  else if (strcmp("w24fn", command.commandName) == 0)
  {
    CommandResponse resp = w24fn_filename(command.arguments[0]);

    size_t responseSize = strlen(resp.response);

    sendData(clientFD, responseSize, resp.response);
  }
  else if (strcmp("w24fz", command.commandName) == 0)
  {
    CommandResponse response = w24fz_size1_size2(command.arguments[0], command.arguments[1]);

    ssize_t tarSize = 0;

    if (strcmp("NO_TAR", response.response) == 0)
    {
      send(clientFD, &tarSize, sizeof(size_t), 0);
      return;
    }

    sendFile(clientFD, "./temp.tar.gz");
  }
  else if (strcmp("w24ft", command.commandName) == 0)
  {
    CommandResponse response = w24ft_extension(command.arguments, command.numArguments);
    ssize_t tarSize = 0;

    if (strcmp("NO_TAR", response.response) == 0)
    {
      send(clientFD, &tarSize, sizeof(size_t), 0);
      return;
    }

    sendFile(clientFD, "./temp.tar.gz");
  }
  else if (strcmp("w24fdb", command.commandName) == 0)
  {
    CommandResponse response = w24fdb_date(command.arguments[0]);

    ssize_t tarSize = 0;

    if (strcmp("NO_TAR", response.response) == 0)
    {
      send(clientFD, &tarSize, sizeof(size_t), 0);
      return;
    }

    sendFile(clientFD, "./temp.tar.gz");
  }
  else if (strcmp("w24fda", command.commandName) == 0)
  {
    CommandResponse response = w24fda_date(command.arguments[0]);
    ssize_t tarSize = 0;

    if (strcmp("NO_TAR", response.response) == 0)
    {
      send(clientFD, &tarSize, sizeof(size_t), 0);
      return;
    }

    sendFile(clientFD, "./temp.tar.gz");
  }
}

// this is the request loop which is forked by a client so that the server can process each client in
// a seperate child process. basically what it does is it gets the request from the client.
// parse the request using parseCommand function then passed the processed request to the
// handleRequest function. It stays in an infinite loop it the client is connect to the server.
void crequest(int client_socket_fd, struct sockaddr_in *client_sockaddr)
{

  printf("Child Forked\n");

  // listen to client requests till the client is connected.
  while (client_socket_fd > -1)
  {
    // wait for the client to send the request and receive the request using receive data function.
    char *request;
    int receiveDataStatus = receiveData(client_socket_fd, &request);
    // check if the client is connect or not.
    if (receiveDataStatus == -1)
    {
      printf("\nConnection lost with Client: %d\n", clientsCount);
      exit(EXIT_SUCCESS);
    }

    // parse the request.
    ParsedCommand command = parseCommand(request);

    // check if the request is quitc to close the connect with the client
    if (strcmp("quitc", command.commandName) == 0)
    {
      printf("Client %d Logged Off\n", clientsCount);
      break;
    }

    // call the handle function to handle the request.
    handleRequest(command, client_socket_fd);
  }
  return;
}

// this is the main driver function for the server.
// basically what it does is it listen for connections from clients and if a client is
// connected it forks a child to handle the request of the connect client.
int main(int argc, char const *argv[])
{
  // set the home path in globle variable.
  HOME_PATH = getenv("HOME");

  // call the initServer function to initialize the server.
  int server_fd = initServer();

  // listen for any connections
  // in this only 50 clients can connect.
  if (listen(server_fd, 50) < 0)
  {
    printf("Error! Can't listen\n");
    exit(EXIT_FAILURE);
  }
  else
  {
    printf("Mirror1 Listening at Port: %d and IP: %s\n", PORT, ADDRESS);
  }

  // start the server loop to accept connections.
  while (1)
  {
    printf("Looking for clients...\n");

    // create a address struct variable to store the details of the connected client.
    struct sockaddr_in *client_sockaddr = malloc(sizeof(struct sockaddr_in));
    socklen_t client_socklen = sizeof(*client_sockaddr);

    // wait at the accept function it get a connection from the client and move further if there is a valid connection
    // received. it also returns a file descriptor connected to the client for communication
    int client_socket_fd = accept(server_fd, (struct sockaddr *)client_sockaddr, &client_socklen);

    if (client_socket_fd == -1)
    {
      perror("Unable to create client file descriptor.");
      exit(EXIT_FAILURE);
    }
    printf("Client %d Connected\n", ++clientsCount);

    // fork a child to handle the requests of the child in a async manner using a different process.
    printf("Forking Child to Handle Request\n");
    int childPID = fork();
    if (childPID < 0)
    {
    }
    if (childPID == 0)
    {
      // in the child process call the crequest function to handle the clients requests.
      close(server_fd);

      crequest(client_socket_fd, client_sockaddr);

      close(client_socket_fd);

      return 0;
    }
  }
  return 0;
}
