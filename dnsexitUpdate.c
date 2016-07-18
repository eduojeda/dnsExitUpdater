#include "dnsexitUpdate.h"

int main(int argc, char *argv[]) {
	struct Param	parameters;
	char			option;
	long			actualIPNumber = 0, dnsIPNumber = 0;
	
	/* Parse the command line arguments (should do it by hand instead of using getopt()) */
	while((option = getopt(argc, argv, "v")) >= 0) {
		switch(option) {
			case 'v':
				verboseFlag = 1;
				break;
				
			default:
				fprintf(stderr, "Error: Invalid Option.\n");
				exit(-1);
				break;
		}
	}

	loadParameters(&parameters);

	/* Open the log file */
	if((logFile = fopen(LOGFILE, "a")) == NULL) {
		fprintf(stderr, "Error trying to open logfile.\n");
		perror("fopen()");
		exit(-1);
	}

	/* Signals */
	signal(SIGCHLD, SIG_IGN);		// Ignore childs
	signal(SIGHUP, signalHandler);	// Catch hangup signal
	signal(SIGTERM, signalHandler);	// Catch kill signal
	signal(SIGINT, signalHandler);	// Catch interrupt signal
	
	/* Daemonize */
	if(!verboseFlag) {
		if(fork() > 0) exit(0);			// Daemonize
		setsid();						// Get a new process group.
		signal(SIGTSTP, SIG_IGN);		// Ignore tty signals
		signal(SIGTTOU, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		if (dup2(fileno(logFile), STDERR_FILENO) < 0) {	// Dump errors to the log file
			perror("dup2");
			exit(-1);
		}
		logMessage("Daemon Started...\n");
	}else logMessage("Verbose Mode...\n");
	
	/* Start the main loop */
	while(1) {
		actualIPNumber = retrieveIP("ppp0");
		if(actualIPNumber != dnsIPNumber) {
			switch(updateIP(&parameters)) {	// Switch on the status code returned by the DNSExit server.
				case DNSEXIT_SUCCESS:
					dnsIPNumber = actualIPNumber;
					logMessage("IP address updated correctly.\n");
					break;

				case DNSEXIT_SAME:
					dnsIPNumber = actualIPNumber;
					logMessage("IP is the same as in the DNS.\n");
					break;

				case DNSEXIT_INVALID_PASS:
					logMessage("Invalid password.\n");
					break;

				case DNSEXIT_INVALID_USER:
					logMessage("User not found.\n");
					break;

				case DNSEXIT_TOO_OFTEN:
					logMessage("Update too often.\n");
					break;

				default:
					logMessage("Error updating IP address.\n");
					break;
			}
		}
		sleep(SLEEP_DELAY);
	}
}

void signalHandler(int signal) {
/* Handles system signals, logging the event */
	switch(signal) {
        case SIGHUP:
			logMessage("Hangup signal recieved.\n");
			break;
			
        case SIGTERM:
			logMessage("Terminate signal recieved. Exiting.\n\n");
			exit(0);
			break;
			
		case SIGINT:
			logMessage("Interrupt signal recieved. Exiting.\n\n");
			exit(0);
			break;
	}
}

void logMessage(char *msg) {
/* Logs a timestamp followed by the message parameter */
	time_t	t = time(NULL);
	struct tm *timer = localtime(&t);

	fprintf(logFile, "[%02d/%02d %02d:%02d:%02d] %s", timer->tm_mday, timer->tm_mon, timer->tm_hour, timer->tm_min, timer->tm_sec, msg);
	fflush(logFile);
}

void loadParameters(struct Param *parameters) {
/* Fill out the parameters structure, based on the configuration file */
	FILE *settingsFile;
	int i = 0;

/*Open the login and hosts settings file*/
	if((settingsFile = fopen(SETTINGSFILE, "r")) == NULL) {
		fprintf(stderr, "Error trying to open settings file.\n");
		perror("fopen()");
		exit(-1);
	}

/*Parse it. The [!-~ ] sequence attempts to include every non-special ASCII character.*/
	fscanf(settingsFile, "Login: %[!-~ ]\n", parameters->login);
	fscanf(settingsFile, "Password: %[!-~ ]\n", parameters->password);	
	while(!feof(settingsFile) && (i < MAX_HOSTS)) {
		fscanf(settingsFile, "Host: %s\n", parameters->hosts[i]);
		i++;
	}
	parameters->hosts[i][0] = '\0';					//Null-terminate the array
	
	if(verboseFlag) {
		printf("Parameters:\n");					//Parameters
		printf("%s\n", parameters->login);
		printf("%s\n", parameters->password);
		i = 0;
		while(parameters->hosts[i][0] != '\0') {	//Detects end of array
			printf("%s\n", parameters->hosts[i]);
			i++;
		}	
	}

	fclose(settingsFile);
}

int updateIP(struct Param *parameters) {
/* Connects to an HTTP server and passes the appropiate parameters to a script using the GET method */
	int					socketFileDesc, i, oldFlags;
	struct sockaddr_in	destAddress;
	struct hostent		*hostEntity;	
	char				urlStr[MAXMSG], answerStr[MAXMSG];
	
/*Fill in the destination address struct*/
	if((hostEntity = gethostbyname(DNSEXIT_URL)) == NULL) {
		perror("gethostbyname()");
		return -1;
	}
	memset(&destAddress, 0, sizeof(struct sockaddr_in));	//Zero the struct, for safety
	destAddress.sin_family = hostEntity->h_addrtype;
	memcpy((char *)&destAddress.sin_addr.s_addr, hostEntity->h_addr, hostEntity->h_length);
	destAddress.sin_port = htons(HTTP_PORT);				//Convert to NBO anyway, for portability to little endian platforms

/*Get a socket and connect to the DNSExit server*/
	if((socketFileDesc = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket()");
		return -1;
	}
	if(connect(socketFileDesc, (struct sockaddr *)&destAddress, sizeof(struct sockaddr)) < 0) {
		perror("connect()");
		return -1;
	}
	else if(verboseFlag)
		printf("Connected to DNSExit.\n\n");
	
/*Build the GET string*/
	memset(urlStr, 0, MAXMSG-1);						
	strcpy(urlStr, "GET /RemoteUpdate.sv?");
	strcat(urlStr, "login=");
	strcat(urlStr, parameters->login);
	strcat(urlStr, "&password=");
	strcat(urlStr, parameters->password);
	strcat(urlStr, "&host=");
	for(i = 0 ; parameters->hosts[i][0] != '\0' ; i++) {
		strcat(urlStr, parameters->hosts[i]);
		strcat(urlStr, ";");
	}urlStr[strlen(urlStr)-1] = '\0';					// Erease that last ';'
	strcat(urlStr, "&myip=");
	strcat(urlStr, " HTTP/1.0");						// Add the HTTP-Version field
	strcat(urlStr, "\r\n\r\n");
	if(verboseFlag)	printf("Will send:\n%s\n", urlStr);	// Output the string for debugging purposes
	
/* Send it out. Checks if everything was sent (won't handle partial sends) */
	if(send(socketFileDesc, urlStr, strlen(urlStr), 0) != strlen(urlStr)) {
		perror("send()");
		return -1;
	}else if(verboseFlag)
		printf("Request sent.\n\n");
	sleep(1);								// Wait for a second
	//shutdown(socketFileDesc, 1);			// No further sends
	
/* Get the response from the server */	
	memset(answerStr, 0, MAXMSG-1);							// Zero the recieve buffer
	oldFlags = fcntl(socketFileDesc, F_GETFL);				// Backup socket flags
	fcntl(socketFileDesc, F_SETFL, oldFlags|O_NONBLOCK);	// Set the socket to non-blocking before recieving
	for(i = 1 ; i <= RECV_TIMEOUT ; i++) {
		if(recv(socketFileDesc, answerStr, MAXMSG, 0) < 0) {
			if(errno == EAGAIN) {
				sleep(1);
			}else {	
				perror("recv()");
				return -1;
			}
		}else {
			if(verboseFlag)
				printf("DNSExit says:\n%s\n\n", answerStr);		// Answer recieved succesfully
			close(socketFileDesc);
			return strtol(&answerStr[17], NULL, 0);				// The 18th char of the reply is the status code (gotta find a nicer way to do this)
		}
		/* If running verbose, print the elapsed time waiting for a response every 5 seconds. */
		if(verboseFlag && i%5 == 0)								
			printf("Elapsed: %d secs. Timeout at %d secs.\n", i, RECV_TIMEOUT);
	}
	logMessage("Timed out waiting for response. \n");
	close(socketFileDesc);
	return -1;
}

long retrieveIP(char *interface) {
/* Gets the IP of the interface connected to the Internet */
	struct ifreq req;
	int sock;
	struct sockaddr_in *addrPtr;
	char *strIP;

	if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {	// Get a TCP/IP socket
		perror("sock()");
		return -1;
	}
	strncpy(req.ifr_name, interface, IF_NAMESIZE);
	if((ioctl(sock, SIOCGIFADDR, (char *)&req)) < 0) {	// Retrieve the IP
		perror("ioctl()");
		return -1;
	}
	close(sock);
	addrPtr = (struct sockaddr_in *)&req.ifr_addr;
	return addrPtr->sin_addr.s_addr;					// Return the IP in its (long) form
}
