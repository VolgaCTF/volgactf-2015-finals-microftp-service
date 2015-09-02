#include "ftp.h"

/* Read the command line options and if they are correct create a listenning server socket. */
int main(int argc, char *argv[])
{
	struct cmd_opts copts;
	bool result = pars_cmd_args(&copts, argc, argv);
	if (!result)
		return -1;
	printf("Initializing server... Wait.\n");
	if (!generate_key())
	{
		printf("Unable to create key-pair... Aborting.\n");
		return -1;
	}
	printf("Server is ready.\n");
	return (int)create_socket(&copts);
}
