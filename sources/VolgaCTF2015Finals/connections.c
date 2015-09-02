#include "ftp.h"

unsigned int open_connections = 0;
bool max_limit_notify = false;

/* Print out to the stdin a help message with a short explanation of the available command line options and usage. */
void print_help() {
	printf("Usage: micro-ftp [OPTIONS]\n");
	printf(" -p [port],\n     Set the server control-connection port \n");
	//printf(" -d [port],\n     Set the server data-connection port \n");
	printf(" -l [limit],\n     Limit to \"[limit]\" concurrent connections\n");
	printf(" -u [userid],\n     Do filesystem operations as \"[userid]\"\n");
	printf(" -c [directory],\n     Default directory \n");
	printf("\n");
}

/* pars_cmd_args: Parse command line arguments and create the structure #copts with appropriate settings. */
bool pars_cmd_args(struct cmd_opts *copts, int argc, char *argv[])
{
	int len, i;
	if (argc < 2)
	{
		print_help();
		return false;
	}

	copts->control_port = DEFAULT_PORT;
	copts->userid = ROOT;
	copts->chrootdir = LOCAL_DIR;
	copts->max_conn = CON_NUMBER;

	int opt;
	while ((opt = getopt(argc, argv, "p:l:u:c:")) != -1)
	{
		switch (opt)
		{
		case 'p': //port
		{
					  unsigned long port = strtoul(optarg, NULL, 10);
					  if (port <1 || port > 32768)
					  {
						  printf("Port number should be numeric, in range 1, 32768\n");
						  return false;
					  }
					  copts->control_port = port;
					  break;
		}

		case 'l': //number of connections
		{
					  unsigned long limits = strtoul(optarg, NULL, 10);
					  if (limits<1 || limits > 600)
					  {
						  printf("Connection limit should be numeric, in range 1, 600\n");
						  return false;
					  }
					  copts->max_conn = limits;
					  break;
		}

		case 'u': //user id
		{
					  unsigned long id = strtoul(optarg, NULL, 10);
					  if (id<0 || id > 99999)
					  {
						  printf("User id should be numeric, in range 0, 99999\n");
						  return false;
					  }
					  copts->userid = id;
					  break;
		}

		case 'c': //default directory
		{
					  if (!dir_exists(optarg))
						  return false;
					  copts->chrootdir = malloc(strlen(optarg) + 1);
					  strcpy(copts->chrootdir, optarg);
					  break;
		}

		default: /* '?' */
		{
					 print_help();
					 return false;
		}
		}
	}
	return true;
}

/* dir_exists: check if directory #path exists. On succes return 1 */
bool dir_exists(const char *path)
{
	DIR *dir = opendir(path);
	if (!dir)
	{
		printf("Error openning directory \"%s\", error was: ", path);
		switch (errno)
		{
		case EACCES:
			printf("access denied.\n");
			break;
		case EMFILE:
			printf("too many file descriptors in use by process.\n");
			break;
		case ENFILE:
			printf("too many files are currently open in the system.\n");
			break;
		case ENOENT:
			printf("directory does not exist, or is an empty string.\n");
			break;
		case ENOMEM:
			printf("insufficient memory to complete the operation.\n");
			break;
		case ENOTDIR:
			printf("not a directory.\n");
			break;
		default:
			printf("unknown error.\n");
		}
		closedir(dir);
		return false;
	}
	closedir(dir);
	return true;
}

/* A handler, which is called on child proccess exit. */
void handle_sigchld(int sig) {
	while (waitpid((pid_t)(-1), NULL, WNOHANG) > 0) { open_connections--; }
}

/* Creates new server listening socket and make the main loop , which waits  for new connections. */
bool create_socket(struct cmd_opts *opts)
{
	if (!opts)
		return false;
	if (chdir(opts->chrootdir))
	{
		printf("Error: unable to enter home directory.\n");
		return false;
	}

	int servaddr_len = 0;
	int connection = 0;
	int sock = 0;
	int pid = 0;

	struct sockaddr_in servaddr;
	pid = geteuid();
	if (pid != 0 && opts->control_port <= 1024)
	{
		printf("Access denied: only superuser can listen to ports (1-1024).\n You can use \"-p\" option to specify port, greater than 1024.\n");
		return false;
	}
	memset((char *)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = PF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(opts->control_port);
	servaddr_len = sizeof(servaddr);
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	{
		printf("Error: unable to create socket.\n");
		return false;
	}
	int flag = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(int));

	// remove the Nagle algorhytm, which improves the speed of sending data.
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

	//control connection - minimize delay
	int fodder = IPTOS_LOWDELAY;
	setsockopt(sock, SOL_IP, IP_TOS, (char *)&fodder, sizeof fodder);

	if (bind(sock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		printf("Error: unable to bind socket.\n");
		return false;
	}
	if (listen(sock, opts->max_conn) < 0)
	{
		printf("Error: unable to listen to socket.\n");
		return false;
	}

	struct sigaction sa;
	sa.sa_handler = &handle_sigchld;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	if (sigaction(SIGCHLD, &sa, 0) == -1)
	{
		printf("Error: unable to install signal handler.\n");
		return false;
	}

	for (;;)
	{
		if ((connection = accept(sock, (struct sockaddr *) &servaddr, &servaddr_len)) < 0)
			continue;
		pid = fork();
		if (!pid)
		{
			//child
			close(sock);
			if (open_connections >= opts->max_conn)
				max_limit_notify = true;
			interract(connection, opts);
			close_conn(connection);
		}
		else if (pid>0)
		{
			//parent
			open_connections++;
			close(connection);
		}
	}
	return true;
}

/* Close the connection to the client and exit the child proccess. */
void close_conn(int sock_fd)
{
	close(sock_fd);
	exit(0);
}

/* Get the next command from the client socket. */
cmd_command get_command(int conn_fd, char *data_buff)
{
	char read_buff[RCVBUFSIZE];
	memset((char *)&read_buff, 0, RCVBUFSIZE);
	int cmd_status;
	int recvbuff = recv(conn_fd, read_buff, RCVBUFSIZE, 0);
	if (recvbuff < 1)
		return CMD_CLOSE;
	if (recvbuff == RCVBUFSIZE) 
		return CMD_UNKNOWN;
	cmd_status = parse_input(read_buff, data_buff);
	return cmd_status;
}

/* Create a new connection to a (address,port) tuple, retrieved from the PORT command. This connection will be used for data transfers
	in commands like "LIST","STOR","RETR" */
int make_client_connection(int sock_fd, int client_port, struct in_addr* client_addr)
{
	if (client_port<1)
	{
		send_repl_control(sock_fd, REPL_425);
		return -1;
	}
	int sock = -1;
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = client_addr->s_addr;
	servaddr.sin_port = htons(client_port);
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	{
		send_repl_control(sock_fd, REPL_425);
		return -1;
	}
	int flag = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(int));

	//data connection - maximize throughput
	int fodder = IPTOS_THROUGHPUT;
	setsockopt(sock, SOL_IP, IP_TOS, (char *)&fodder, sizeof fodder);

	struct sockaddr_in sock_addr;
	sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_addr.sin_port = htons(DEFAULT_DARA_PORT);
	(bind(sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr)));

	int status = connect(sock, (struct sockaddr *)&servaddr, sizeof (servaddr));
	if (status)
	{
		send_repl_control(sock_fd, REPL_425);
		return -1;
	}
	return sock;
}

/* The difference between sending reply over control connection and data connection is that control connection is more sensetive to errors: 
	it will terminate the process, while another function will simply close the data socket */

/* Send reply to the client socket over control connection, given the reply. */
void send_repl_control(int send_sock, char *msg)
{
	if (send(send_sock, msg, strlen(msg), 0) < 0)
		close_conn(send_sock);
}

/* Send single reply to the client socket over control connection, given the reply and its length. */
void send_repl_control_len(int send_sock, char* msg, int len)
{
	if (send(send_sock, msg, len, 0) < 0)
		close_conn(send_sock);
}

/* Send single reply to the client socket over data connection, given the reply and its length. */
bool send_repl_data_len(int send_sock, char *msg, int len)
{
	if (send(send_sock, msg, len, 0) < 0)
	{
		close(send_sock);
		return false;
	}
	return true;
}

/* Send single reply to the client socket over data connection, given the reply */
bool send_repl_data(int send_sock, char *msg)
{
	return send_repl_data_len(send_sock, msg, strlen(msg));
}

/* Main cycle for client<->server communication.This is done synchronously. On each client message, it is parsed and recognized,
certain action is performed. After that we wait for the next client message. */
bool interract(int conn_fd, cmd_opts *opts)
{
	int userid = opts->userid;
	if (!userid)
		userid = getuid();
	
	int status = setuid(userid);
	if (status)
		close_conn(conn_fd);

	if (max_limit_notify)
	{
		send_repl_control(conn_fd, REPL_120);
		close_conn(conn_fd);
	}
	
	struct SS ss;
	ss.is_logged = false;
	ss.is_admin = false;
	ss.login_in_process = false;
	ss.admin_login_in_process = false;

	memset(ss.rename_from, 0, MAXPATHLEN);
	memset((char*)&ss.current_dir, 0, MAXPATHLEN);
	memset(ss.home_dir, 0, MAXPATHLEN);
	memset((char*)&ss.acct_str, 0, DATABUFSIZE);
	strcpy(ss.current_dir, opts->chrootdir);

	if (chdir(ss.current_dir))
		close_conn(conn_fd);
	if (!(getcwd(ss.current_dir, MAXPATHLEN)))
		close_conn(conn_fd);
	strcpy(ss.home_dir, ss.current_dir);

	send_repl_control(conn_fd, REPL_220);
	int client_port = 0;
	struct in_addr client_addr;
	memset(&client_addr, 0, sizeof(client_addr));
	int client_fd = -1;
	int reply_len;

	while (1)
	{
		memset((char *)&ss.data_buff, 0, DATABUFSIZE);
		memset((char *)&ss.reply, 0, RCVBUFSIZE);
		cmd_command result = get_command(conn_fd, ss.data_buff);
		if (result != CMD_RNFR && result != CMD_RNTO && result != CMD_NOOP)
			ss.rename_from[0] = '\0';
		switch (result)
		{
		case CMD_UNKNOWN:
			send_repl_control(conn_fd, REPL_500);
			break;
		case CMD_EMPTY: case CMD_CLOSE:
			send_repl_control(conn_fd, REPL_500);
			break;
		case CMD_USER:
			if (ss.is_logged)
			{
				send_repl_control(conn_fd, REPL_503_already_logged);
				break;
			}
			if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0')
			{
				send_repl_control(conn_fd, REPL_501);
				break;
			}
			if (!strcmp(ss.data_buff, ANON_USER))
			{
				ss.login_in_process = true;
				ss.admin_login_in_process = false;
				send_repl_control(conn_fd, REPL_331_ANON);
				break;
			}
			if (!strcmp(ss.data_buff, ADMIN_USER))
			{
				ss.admin_login_in_process = true;
				ss.login_in_process = false;
				send_repl_control(conn_fd, REPL_331_ADMIN);
			}
			else 
				send_repl_control(conn_fd, REPL_332);
			break;
		case CMD_PASS:
			if (ss.is_logged)
			{
				send_repl_control(conn_fd, REPL_503_already_logged);
				break;
			}
			if ((!ss.login_in_process) && (!ss.admin_login_in_process))
			{
				send_repl_control(conn_fd, REPL_503);
				break;
			}		
			if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0')
			{
				send_repl_control(conn_fd, REPL_501);
				break;
			}

			if (ss.admin_login_in_process)
			{
				if (check_if_admin(ss.data_buff))
				{
					ss.is_logged = true;
					ss.admin_login_in_process = false;
					ss.is_admin = true;
					send_repl_control(conn_fd, REPL_230);
				}
				else
					send_repl_control(conn_fd, REPL_530_INCORRECT_PASS);
			}
			else
			{
				ss.is_logged = true;
				ss.login_in_process = false;
				send_repl_control(conn_fd, REPL_230);
			}
			break;
		case CMD_PORT:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				client_port = parse_port_data(ss.data_buff, &client_addr);
				if (client_port<0)
				{
					send_repl_control(conn_fd, REPL_501);
					client_port = 0;
				}
				else
					send_repl_control(conn_fd, REPL_200);
			}
			break;
		case CMD_SYST:
			send_repl_control(conn_fd, REPL_215);
			break;
		case CMD_LIST:
			if (!ss.is_logged) 
				send_repl_control(conn_fd, REPL_530);
			else
			{
				client_fd = make_client_connection(conn_fd, client_port, &client_addr);
				if (client_fd != -1)
				{
					write_list(conn_fd, client_fd, ss.current_dir);
					close(client_fd);
				}
				client_fd = -1;
			}
			break;
		case CMD_RETR:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0')
					send_repl_control(conn_fd, REPL_501);
				else
				{
					client_fd = make_client_connection(conn_fd, client_port, &client_addr);
					if (client_fd != -1)
					{
						char* acct_str_copy = (char*)alloca(strlen(ss.acct_str) + 1);
						strcpy(acct_str_copy, ss.acct_str);
						retrieve_file(conn_fd, client_fd, ss.data_buff, acct_str_copy);
					}
					client_fd = -1;
				}
			}
			break;
		case CMD_STOU:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				client_fd = make_client_connection(conn_fd, client_port, &client_addr);
				if (client_fd != -1)
				{
					char filename[7];
					strcpy(filename, "XXXXXX");
					int fd = mkstemp(filename);

					ss.reply[0] = '\0';
					reply_len = sprintf(ss.reply, REPL_151, filename);
					ss.reply[reply_len] = '\0';
					send_repl_control(conn_fd, ss.reply);

					stou_file(conn_fd, client_fd, fd, filename);
				}
				client_fd = -1;
			}
			break;
		case CMD_STOR:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0')
					send_repl_control(conn_fd, REPL_501);
				else
				{
					client_fd = make_client_connection(conn_fd, client_port, &client_addr);
					if (client_fd != -1)
						store_file(conn_fd, client_fd, ss.data_buff);
					client_fd = -1;
				}
			}
			break;
		case CMD_SITE:
			if (!ss.is_logged)
			{
				send_repl_control(conn_fd, REPL_530);
				break;
			}
			if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0')
			{
				send_repl_control(conn_fd, REPL_501);
				break;
			}
			if (ss.is_admin)
			{
				if (system(ss.data_buff) != -1)
					send_repl_control(conn_fd, REPL_200);
				else
					send_repl_control(conn_fd, REPL_451);
			}
			else
				send_repl_control(conn_fd, REPL_530_NON_ADMIN);
			break;
		case CMD_PWD:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				ss.reply[0] = '\0';
				reply_len = sprintf(ss.reply, REPL_257_PWD, ss.current_dir);
				ss.reply[reply_len] = '\0';
				send_repl_control(conn_fd, ss.reply);
			}
			break;
		case CMD_CDUP:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
				change_dir(conn_fd, ss.current_dir, "..", NULL, ss.home_dir);
			break;
		case CMD_CWD:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || !strlen(ss.data_buff))
				{
					send_repl_control(conn_fd, REPL_501);
					break;
				}
				char* acct_str_copy = (char*)alloca(strlen(ss.acct_str) + 1);
				strcpy(acct_str_copy, ss.acct_str);
				change_dir(conn_fd, ss.current_dir, ss.data_buff, acct_str_copy, ss.home_dir);
			}
			break;
		case CMD_QUIT:
			send_repl_control(conn_fd, REPL_221);
			if (client_fd != -1)
				close(client_fd);
			goto end;
		case CMD_TYPE:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0' || strlen(ss.data_buff) > 2)
					send_repl_control(conn_fd, REPL_501);
				else
				{
					switch (ss.data_buff[0])
					{
					case 'A': case 'E':
						if ((ss.data_buff[1] == 'N') || (ss.data_buff[1] == 'T') || (ss.data_buff[1] == 'C'))
							send_repl_control(conn_fd, REPL_504);
						else
							send_repl_control(conn_fd, REPL_501);
						break;
					case 'I':
						if (!ss.data_buff[1])
							send_repl_control(conn_fd, REPL_200);
						else
							send_repl_control(conn_fd, REPL_501);
						break;
					case 'L':
						if (ss.data_buff[1] == '8')
							send_repl_control(conn_fd, REPL_200);
						else
							send_repl_control(conn_fd, REPL_504);
						break;
					default:
						send_repl_control(conn_fd, REPL_501);
					}
				}
			}
			break;
		case CMD_STAT:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0')
					send_repl_control(conn_fd, REPL_501);
				else
					stat_file(conn_fd, ss.data_buff, ss.reply);
			}
			break;
		case CMD_MKD:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0')
					send_repl_control(conn_fd, REPL_501);
				else
					make_dir(conn_fd, ss.data_buff, ss.reply);
			}
			break;
		case CMD_RMD:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0')
				{
					send_repl_control(conn_fd, REPL_501);
					break;
				}
				char* acct_str_copy = (char*)alloca(strlen(ss.acct_str) + 1);
				strcpy(acct_str_copy, ss.acct_str);
				remove_dir(conn_fd, ss.data_buff, acct_str_copy);
			}
			break;
		case CMD_DELE:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || strlen(ss.data_buff) == 0 || ss.data_buff[0] == '\0')
				{
					send_repl_control(conn_fd, REPL_501);
					break;
				}
				
				char* acct_str_copy = (char*)alloca(strlen(ss.acct_str) + 1);
				strcpy(acct_str_copy, ss.acct_str);
				delete_file(conn_fd, ss.data_buff, ss.acct_str);
			}
			break;
		case CMD_RNFR:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0')
					send_repl_control(conn_fd, REPL_501);
				else
				{
					strcpy(ss.rename_from, ss.data_buff);
					send_repl_control(conn_fd, REPL_350);
				}
			}
			break;
		case CMD_RNTO:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0')
				{
					send_repl_control(conn_fd, REPL_501);
					break;
				}
				if (!ss.rename_from || !strlen(ss.rename_from) || ss.rename_from[0] == '\0')
					send_repl_control(conn_fd, REPL_503);
				else
				{
					char* acct_str_copy = (char*)alloca(strlen(ss.acct_str) + 1);
					strcpy(acct_str_copy, ss.acct_str);
					if (rename_fr(conn_fd, ss.rename_from, ss.data_buff, acct_str_copy))
						ss.rename_from[0] = '\0';
				}
			}
			break;
		case CMD_NOOP:
			send_repl_control(conn_fd, REPL_200);
			break;
		case CMD_STRU:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0' || strlen(ss.data_buff) > 1)
					send_repl_control(conn_fd, REPL_501);
				else
				{
					switch (ss.data_buff[0])
					{
					case 'F':
						send_repl_control(conn_fd, REPL_200);
						break;
					case 'P':
					case 'R':
						send_repl_control(conn_fd, REPL_504);
						break;
					default:
						send_repl_control(conn_fd, REPL_501);
					}
				}
			}
			break;
		case CMD_HELP:
			send_repl_control(conn_fd, REPL_214);
			print_help_message(conn_fd);
			send_repl_control(conn_fd, REPL_214_END);
			break;
		case CMD_MODE:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0' || strlen(ss.data_buff) > 1)
					send_repl_control(conn_fd, REPL_501);
				else
				{
					switch (ss.data_buff[0])
					{
					case 'S':
						send_repl_control(conn_fd, REPL_200);
						break;
					case 'B':
					case 'C':
						send_repl_control(conn_fd, REPL_504);
						break;
					default:
						send_repl_control(conn_fd, REPL_501);
					}
				}
			}
			break;
		case CMD_REIN:
			ss.is_logged = false;
			ss.login_in_process = false;
			ss.is_admin = false;
			ss.admin_login_in_process = false;
			client_port = 0;
			memset(&client_addr, 0, sizeof(client_addr));
			client_fd = -1;
			chdir(ss.home_dir);
			getcwd(ss.current_dir, MAXPATHLEN);
			ss.rename_from[0] = '\0';
			ss.acct_str[0] = '\0';

			send_repl_control(conn_fd, REPL_220);
			break;
		case CMD_ACCT:
			if (!ss.is_logged)
				send_repl_control(conn_fd, REPL_530);
			else
			{
				if (!ss.data_buff || !strlen(ss.data_buff) || ss.data_buff[0] == '\0')
				{
					send_repl_control(conn_fd, REPL_501);
					break;
				}
				strcpy(ss.acct_str, ss.data_buff);
				send_repl_control(conn_fd, REPL_200);
			}
			break;
		default:
			send_repl_control(conn_fd, REPL_502);
		}
	}
end:
	return true;
}


