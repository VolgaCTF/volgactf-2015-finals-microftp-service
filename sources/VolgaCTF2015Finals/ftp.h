#ifndef _DEFINES_H
#define _DEFINES_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h> 
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <libgen.h>
#include <time.h>
#include <fcntl.h>
#include "libscrypt.h"

//Let's try openssl instead of MIRACLE this time.
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

typedef uint8_t bool;
enum{false, true};
typedef struct cmd_opts {
	unsigned long control_port;
	unsigned long max_conn;
	unsigned long userid; //user, on behalf all file operations are proceeded
	char *chrootdir; //home directory (malloced)
}cmd_opts;

typedef struct ELGAMAL_params{
	BIGNUM* modulus_p;
	BIGNUM* generator_g;
	BIGNUM* secret_x;
	BIGNUM* y;
} ELGAMAL_params;

#define FILE_SEPARATOR "/"
#define DEFAULT_PORT 21
#define DEFAULT_DARA_PORT 20
#define ROOT 0
#define LOCAL_DIR "./"
#define CON_NUMBER 100
#define ANON_USER "anonymous"
#define ADMIN_USER "admin"
#define EOL "\r\n"
#define RETRY_MINUTES 2 
#define RCVBUFSIZE 512
#define CMDBUFSIZE 5
#define DATABUFSIZE 498
#define SENDBUFSIZE 512
#define ERR_CONNECT 301
#define ADDRBUFSIZE 16
#define MAXPATHLEN 256
#define PORTDELIM ","
#define HASHDELIM ":"
#define CMDDELIM " \t\n"
#define IP_BUF_SIZE 16
#define FILE_INFO_BUF_SIZE 300
#define DATA_BUF_SIZE 16
#define PASSPHRASE_SIZE 512
#define SEED_LENGTH 32
#define PRIME_LENGTH 512

typedef struct SS
{	
	char current_dir[MAXPATHLEN];
	char home_dir[MAXPATHLEN]; //we cannot get out from this directory.
	char reply[SENDBUFSIZE];
	char data_buff[DATABUFSIZE];
	char acct_str[DATABUFSIZE]; //password string
	char rename_from[MAXPATHLEN]; //file to rename (for RNFR and RNTO)
	bool is_logged;
	bool is_admin;
	bool login_in_process;
	bool admin_login_in_process;
} SS;

/* Responce messages. */
#define REPL_110 "110 Restart marker reply.\r\n"
#define REPL_120 "120 Try again in 2 minutes.\r\n"
#define REPL_125 "125 Data connection already open; transfer starting.\r\n"
#define REPL_150 "150 File status okay; about to open data connection.\r\n"
#define REPL_151 "151 New file name is: %s.\n\r"
#define REPL_200 "200 Command okay.\r\n"
#define REPL_202 "202 Command not implemented, superfluous at this site.\r\n"
#define REPL_211 "221 System status, or system help reply.\r\n"
#define REPL_211_STATUS "221-status of %s.\r\n"
#define REPL_211_END "221 End of status.\r\n"
#define REPL_212 "212 Directory status.\r\n"
#define REPL_213 "213 File status.\r\n"
#define REPL_214 "214-Help message.\n\r"
#define REPL_214_END "214 End Help message.\r\n"
#define REPL_215 "215 UNIX Type: L8.\r\n"
#define REPL_220 "220 Service ready for new user.\r\n"
#define REPL_221 "221 Service closing control connection.\r\n"
#define REPL_225 "225 Data connection open; no transfer in progress.\r\n"
#define REPL_226 "226 Closing data connection.\r\n"
#define REPL_226_WITH_HASH "226 Closing data connection. To access created item send the following passphrase with ACCT command: %s.\r\n"
#define REPL_227 "227 Entering Passive Mode (%s,%s,%s,%s,%s,%s).\r\n"
#define REPL_230 "230 User logged in, proceed.\r\n"
#define REPL_250 "250 Requested file action okay, completed.\r\n"
#define REPL_250_WITH_HASH "250 Requested file action okay, completed. To access renamed file send the following passphrase with ACCT command: %s.\r\n"
#define REPL_257 "257 %s created.\r\n"
#define REPL_257_WITH_HASH "257 %s created. To access created item send the following passphrase with ACCT command: %s.\r\n"
#define REPL_257_PWD "257 \"%s\" is current working dir.\r\n"
#define REPL_331 "331 Only anonymous user is accepted.\r\n"
#define REPL_331_ANON "331 Anonymous login okay, send your complete email as your password.\r\n"
#define REPL_331_ADMIN "331 Admin login okay, send your password.\r\n"
#define REPL_332 "332 Need account for login.\r\n"
#define REPL_350 "350 Requested file action pending further information.\r\n"
#define REPL_421 "421 Service not available, closing control connection.\r\n"
#define REPL_425 "425 Can't open data connection.\r\n"
#define REPL_426 "426 Connection closed; transfer aborted.\r\n"
#define REPL_450 "450 Requested file action not taken.\r\n"
#define REPL_450_NOT_EXISTED "450 Requested file action not taken: requested file doesn't exist.\r\n"
#define REPL_451 "451 Requested action aborted. Local error in processing.\r\n"
#define REPL_451_NOT_REGULAR "451 Requested action aborted. Requested file is not regular.\r\n"
#define REPL_452 "452 Requested action not taken.\r\n"
#define REPL_500 "500 Syntax error, command unrecognized.\r\n"
#define REPL_501 "501 Syntax error in parameters or arguments.\r\n"
#define REPL_502 "502 Command not implemented.\r\n"
#define REPL_502_TYPE "502 Command not implemented. The only supported type is L8.\r\n"
#define REPL_503 "503 Bad sequence of commands.\r\n"
#define REPL_503_already_logged "503 You are already logged in.\r\n"
#define REPL_504 "504 Command not implemented for that parameter.\r\n"
#define REPL_530 "530 Not logged in.\r\n"
#define REPL_530_NON_ADMIN "530 This action is allowed only for admin users.\r\n"
#define REPL_530_INCORRECT_PASS "530 The password is incorrect.\r\n"
#define REPL_531 "531 Passphrase in ACCT command is not correct.\r\n"
#define REPL_532 "532 Need account for storing files.\r\n"
#define REPL_550 "550 Requested action not taken.\r\n"
#define REPL_550_HOME_DIR "550 Requested action not taken: unable to leave home directory.\r\n"
#define REPL_551 "551 Requested action aborted. Page type unknown.\r\n"
#define REPL_552 "552 Requested file action aborted.\r\n"
#define REPL_553 "553 Requested action not taken.\r\n"

typedef enum {
CMD_USER, CMD_PASS, CMD_ACCT,CMD_CWD, CMD_CDUP, CMD_SMNT, CMD_QUIT, CMD_REIN, CMD_PORT, CMD_PASV, CMD_TYPE, CMD_STRU, CMD_MODE, CMD_RETR, 
	CMD_STOR, CMD_STOU, CMD_APPE, CMD_ALLO, CMD_REST, CMD_RNFR, CMD_RNTO, CMD_ABOR, CMD_DELE, CMD_RMD, CMD_MKD, CMD_PWD, CMD_LIST, CMD_NLST, CMD_SITE, CMD_SYST,
	CMD_STAT, CMD_HELP, CMD_NOOP, CMD_UNKNOWN, CMD_EMPTY, CMD_CLOSE, CMD_MDTM, CMD_SIZE} cmd_command;

bool pars_cmd_args(struct cmd_opts *, int argc, char *argv[]);
void print_help();
bool dir_exists(const char *);
bool create_socket(struct cmd_opts *);
void handle_sigchld(int sig);
bool interract(int conn_fd, cmd_opts *opts);
void close_conn(int sock_fd);
cmd_command get_command(int conn_fd, char *data_buff);
int make_client_connection(int sock_fd, int client_port, struct in_addr* client_addr);
void send_repl_control(int send_sock, char *msg);
void send_repl_control_len(int send_sock, char* msg, int len);
bool send_repl_data_len(int send_sock, char *msg, int len);
bool send_repl_data(int send_sock, char *msg);

cmd_command parse_input(char *input_buff, char *data_buff);
int parse_port_data(char *data_buff, struct in_addr *client_addr);

bool write_list(int sock, int client_sock, const char *current_dir);
bool retrieve_file(int sock, int client_sock, const char * file_name_with_path, char* acct_str);
bool stou_file(int sock, int client_sock, int fpr, char* filename);
bool store_file(int sock, int client_sock, const char * file_name_with_path);
bool change_dir(int sock, char *current_dir, char *target_dir_with_path, char* acct_str, char* home_dir);
bool stat_file(int sock, char *file_name_with_path, char *reply);
bool make_dir(int sock, char *new_dir_with_path, char *reply);
bool remove_dir(int sock, char *removed_dir_with_path, char* acct_str);
bool delete_file(int sock, char *delete_file_with_path, char* acct_str);
bool rename_fr(int sock, char *from_with_path, char *to_with_path, char* acct_str);

bool base64_encode(const unsigned char *input_data, unsigned char *output_buffer, int input_length, int output_buffer_length);
void base64_init();
bool base64_decode(const unsigned char *input_data, unsigned char *output_buffer, int output_buffer_length, int* output_length);
bool check_acct(char* message_with_path, char* sign);
bool generate_passphrase(char* data, char* passphrase);
void print_help_message(int sock);
bool generate_key();
bool check_if_admin(char* pass);

#endif

/*ifeq ($(USE_DEL_TO_CLEAN),1)
	del /S /Q $(BINARYDIR)
else
	rm -rf $(BINARYDIR)
	endif*/

/*ifeq ($(IS_LINUX_PROJECT),1)
	RPATH_PREFIX := -Wl,--rpath='$$ORIGIN/../
	LIBRARY_LDFLAGS += $(EXTERNAL_LIBS)
	LIBRARY_LDFLAGS += -Wl,--rpath='$$ORIGIN'
	LIBRARY_LDFLAGS += $(addsuffix ',$(addprefix $(RPATH_PREFIX),$(dir $(EXTERNAL_LIBS))))*/