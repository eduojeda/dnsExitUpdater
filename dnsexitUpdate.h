#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <time.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <time.h>

/* Constants */
#define MAX_HOSTS				128
#define DNSEXIT_URL				"www.dnsexit.com"
#define SETTINGSFILE			"dnsexitUpdate.conf"
#define LOGFILE					"dnsexitUpdate.log"
#define HTTP_PORT				80
#define MAXMSG					255
#define RECV_TIMEOUT			60
#define SLEEP_DELAY				500
//DNSExit return codes
#define DNSEXIT_SUCCESS			0		
#define DNSEXIT_SAME			1
#define DNSEXIT_INVALID_PASS	2
#define DNSEXIT_INVALID_USER	3
#define DNSEXIT_TOO_OFTEN		4

/* System Variables */
extern char	*optarg;		//getopt() external variables
extern int 	optind;
extern int	optopt;
extern int	opterr;
extern int	optreset;

//extern int	errno;

/* Global Variables */
int	verboseFlag = 0;
FILE *logFile;

/* Structs */
struct Param {
	char login[256];
	char password[256];
	char hosts[MAX_HOSTS][256];
};

/* Definitions */
void logMessage(char *msg);
void signalHandler(int signal);
void loadParameters(struct Param *);
int updateIP(struct Param *);
long retrieveIP(char *);
