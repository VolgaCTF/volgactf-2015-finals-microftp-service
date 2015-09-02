#include "ftp.h"

static bool cmp2(char c1, char c2, char d1, char d2);
static bool cmp3(char c1, char c2, char c3, char d1, char d2, char d3);
static bool _cmp1(char c1, char* d);
static bool _cmp2(char c1, char c2, char *d);
static bool _cmp3(char c1, char c2, char c3, char *d);

static bool cmp2(char c1, char c2, char d1, char d2)
{
	if (c1 == d1 && c2 == d2)
		return true;
	return false;
}

static bool cmp3(char c1, char c2, char c3, char d1, char d2, char d3)
{
	if (cmp2(c1, c2, d1, d2) && c3 == d3)
		return true;
	return false;
}

static bool _cmp2(char c1, char c2, char *d)
{
	return (cmp2(c1, c2, d[0], d[1]));
}

static bool _cmp1(char c1, char *d)
{
	return (c1 == d[0]);
}

static bool _cmp3(char c1, char c2, char c3, char *d)
{
	return (cmp3(c1, c2, c3, d[0], d[1], d[2]));
}

cmd_command parse_input(char *input_buff, char *data_buff)
{
	if (!input_buff)
		return CMD_EMPTY;

	char* command_buf = strtok(input_buff, CMDDELIM);
	char* data = strtok(NULL, CMDDELIM);
	if (data)
		strcpy(data_buff, data);

	int len = strlen(command_buf);
	if (len<3)
		return CMD_UNKNOWN;
	switch (command_buf[0])
	{
	case 'A':
		switch (command_buf[1])
		{
		case 'B':
			if (_cmp2('O', 'R', command_buf + 2))
				return CMD_ABOR;
		case 'C':
			if (_cmp2('C', 'T', command_buf + 2))
				return CMD_ACCT;
		case 'L':
			if (_cmp2('L', 'O', command_buf + 2))
				return CMD_ALLO;
		case 'P':
			if (_cmp2('P', 'E', command_buf + 2))
				return CMD_APPE;
			return CMD_UNKNOWN;
		}
	case 'C':
		if (_cmp2('W', 'D', command_buf + 1))
			return CMD_CWD;
		else if (_cmp3('D', 'U', 'P', command_buf + 1))
			return CMD_CDUP;
		return CMD_UNKNOWN;

	case 'D':
		if (_cmp3('E', 'L', 'E', command_buf + 1))
			return CMD_DELE;
		return CMD_UNKNOWN;
	case 'H':
		if (_cmp3('E', 'L', 'P', command_buf + 1))
			return CMD_HELP;
		return CMD_UNKNOWN;
	case 'L':
		if (_cmp3('I', 'S', 'T', command_buf + 1))
			return CMD_LIST;
		return CMD_UNKNOWN;
	case 'M':
		switch (command_buf[1])
		{
		case 'K':
			if (_cmp1('D', command_buf + 2))
				return CMD_MKD;
		case 'O':
			if (_cmp2('D', 'E', command_buf + 2))
				return CMD_MODE;
		case 'D':
			if (_cmp2('T', 'M', command_buf + 2))
				return CMD_MDTM;
			return CMD_UNKNOWN;
		}
	case 'N':
		switch (command_buf[1])
		{
		case 'L':
			if (_cmp2('S', 'T', command_buf + 2))
				return CMD_NLST;
		case 'O':
			if (_cmp2('O', 'P', command_buf + 2))
				return CMD_NOOP;
			return CMD_UNKNOWN;
		}
	case 'P':
		switch (command_buf[1])
		{
		case 'A':
			if (_cmp1('S', command_buf + 2))
			{
				if (_cmp1('S', command_buf + 3))
					return CMD_PASS;
				if (_cmp1('V', command_buf + 3))
					return CMD_PASV;
				return CMD_UNKNOWN;
			}
		case 'O':
			if (_cmp2('R', 'T', command_buf + 2))
				return CMD_PORT;
		case 'W':
			if (_cmp1('D', command_buf + 2))
				return CMD_PWD;
			return CMD_UNKNOWN;
		}
	case 'Q':
		if (_cmp3('U', 'I', 'T', command_buf + 1))
			return CMD_QUIT;
		return CMD_UNKNOWN;
	case 'R':
		switch (command_buf[1])
		{
		case 'E':
			switch (command_buf[2])
			{
			case 'I':
				if (_cmp1('N', command_buf + 3))
					return CMD_REIN;
			case 'S':
				if (_cmp1('T', command_buf + 3))
					return CMD_REST;
			case 'T':
				if (_cmp1('R', command_buf + 3))
					return CMD_RETR;
				return CMD_UNKNOWN;
			}
			return CMD_UNKNOWN;
		case 'M':
			if (_cmp1('D', command_buf + 2))
				return CMD_RMD;
		case 'N':
			switch (command_buf[2])
			{
			case 'F':
				if (_cmp1('R', command_buf + 3))
					return CMD_RNFR;
			case 'T':
				if (_cmp1('O', command_buf + 3))
					return CMD_RNTO;
				return CMD_UNKNOWN;
			}
			return CMD_UNKNOWN;
		}
	case 'S':
		switch (command_buf[1])
		{
		case 'I':
			if (_cmp2('T', 'E', command_buf + 2))
				return CMD_SITE;
			if (_cmp2('Z', 'E', command_buf + 2))
				return CMD_SIZE;
		case 'M':
			if (_cmp2('N', 'T', command_buf + 2))
				return CMD_SMNT;
		case 'T':
			switch (command_buf[2])
			{
			case 'A':
				if (_cmp1('T', command_buf + 3))
					return CMD_STAT;
			case 'O':
				if (_cmp1('R', command_buf + 3))
					return CMD_STOR;
				if (_cmp1('U', command_buf + 3))
					return CMD_STOU;
			case 'R':
				if (_cmp1('U', command_buf + 3))
					return CMD_STRU;
			}
			return CMD_UNKNOWN;
		case 'Y':
			if (_cmp2('S', 'T', command_buf + 2))
				return CMD_SYST;
			return CMD_UNKNOWN;
		}
	case 'T':
		if (_cmp3('Y', 'P', 'E', command_buf + 1))
			return CMD_TYPE;
		return CMD_UNKNOWN;
	case 'U':
		if (_cmp3('S', 'E', 'R', command_buf + 1))
			return CMD_USER;
		return CMD_UNKNOWN;
		return CMD_UNKNOWN;
	}
	return CMD_UNKNOWN;
}

/* Parses the results from the PORT command, writes the address in "client_addr" and returnes the port */
int parse_port_data(char *data_buff, struct in_addr *client_addr)
{
	int port = 0, iter_number;
	char *result;
	uint32_t ip = 0;
	uint32_t _order = 1;
	for (iter_number = 0; iter_number < 6; iter_number++)
	{
		if (!iter_number)
			result = strtok(data_buff, PORTDELIM);
		else
			result = strtok(NULL, PORTDELIM);
		if (!result)
			return -1;
		char* endptr;
		unsigned long _toint = strtoul(result, &endptr, 10);
		if ((!_toint) && (result == endptr))
			return - 1;
		
		if (_toint<0 || _toint>255)
			return -1;
		if (iter_number < 4)
		{
			ip += _toint * _order;
			_order *= 0x100;
		}
		else
		{
			port *= 0x100;
			port += _toint;
		}
	}
	client_addr->s_addr = ip;
	return port;
}