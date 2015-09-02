#include "ftp.h"

static DIR * ensure_dir_exists(int sock, const char*path);
static bool get_file_info(const char *file_name, char *line);
static bool is_special_dir(const char *dir);
static bool get_file_info_stat(const char *file_name, char *line, struct stat *s_buff);
static void write_file(char *line, const char *mode, int num, const char *user, const char * group, int size, const char *date, const char*fl_name);

/* Get and send the info for the current directory */
bool write_list(int sock, int client_sock, const char *current_dir)
{
	if (client_sock > 0)
	{
		if (sock != client_sock)
			send_repl_control(sock, REPL_150);
	}
	else
	{
		if (sock != client_sock)
			send_repl_control(sock, REPL_425);
		return false;
	}
		
	DIR *dir = ensure_dir_exists(sock, current_dir);
	if (!dir)
	{
		if (sock != client_sock)
		{
			close(client_sock);
			send_repl_control(sock, REPL_451);
		}
		return false;
	}

	char line[FILE_INFO_BUF_SIZE];
	struct dirent *d_next = readdir(dir);
	while (d_next)
	{
		line[0] = '\0';
		if (get_file_info(d_next->d_name, line))
		{
			if (!send_repl_data(client_sock, line))
			{
				if (sock != client_sock)
				{
					send_repl_control(sock, REPL_451);
					close(client_sock);
				}
				return false;
			}
		}
		d_next = readdir(dir);
	}

	if (sock != client_sock)
	{
		close(client_sock);
		send_repl_control(sock, REPL_226);
	}
	closedir(dir);
	return true;
}

/* Check if directory exists. If it exists return an open descriptor to that directory.
	If it is not a directory, or does not exists return NULL. */
static DIR * ensure_dir_exists(int sock, const char*path) {
	DIR *dir = opendir(path);
	if (!dir)
	{
		closedir(dir);
		return NULL;
	}
	return dir;
}

/* Check if the given directory is one of ".", or ".." */
static bool is_special_dir(const char *dir)
{
	if (!dir)
		return false;
	int len = strlen(dir);
	if (len>2)
		return false;
	if (dir[0] != '.')
		return false;
	if (len == 1)
		return true;
	if (dir[1] == '.')
		return true;
	return false;
}

/* Writes the file statistics in a line "line" */
static bool get_file_info(const char *file_name, char *line)
{
	if (!line)
		return false;
	struct stat s_buff;
	if (is_special_dir(file_name))
		return false;
	int status = stat(file_name, &s_buff);
	if (!status)
		return get_file_info_stat(file_name, line, &s_buff);
	return false;
}

/* Write file statics in a line using the buffer from stat(...) primitive */
static bool get_file_info_stat(const char *file_name, char *line, struct stat *s_buff)
{
	char date[DATA_BUF_SIZE];
	char mode[11] = "----------";
	line[0] = '\0';
	struct passwd * pass_info = getpwuid(s_buff->st_uid);
	if (pass_info)
	{
		struct group * group_info = getgrgid(s_buff->st_gid);
		if (group_info)
		{
			int b_mask = s_buff->st_mode & S_IFMT;
			if (b_mask == S_IFDIR)
				mode[0] = 'd';
			else if (b_mask == S_IFREG)
				mode[0] = '-';
			else
				return false;

			mode[1] = (s_buff->st_mode & S_IRUSR) ? 'r' : '-';
			mode[2] = (s_buff->st_mode & S_IWUSR) ? 'w' : '-';
			mode[3] = (s_buff->st_mode & S_IXUSR) ? 'x' : '-';
			mode[4] = (s_buff->st_mode & S_IRGRP) ? 'r' : '-';
			mode[5] = (s_buff->st_mode & S_IWGRP) ? 'w' : '-';
			mode[6] = (s_buff->st_mode & S_IXGRP) ? 'x' : '-';
			mode[7] = (s_buff->st_mode & S_IROTH) ? 'r' : '-';
			mode[8] = (s_buff->st_mode & S_IWOTH) ? 'w' : '-';
			mode[9] = (s_buff->st_mode & S_IXOTH) ? 'x' : '-';
			strftime(date, DATA_BUF_SIZE, "%b %d %H:%M", localtime(&(s_buff->st_mtime)));

			write_file(line, mode, s_buff->st_nlink, pass_info->pw_name, group_info->gr_name, s_buff->st_size, date, file_name);
			return true;
		}
	}
	return false;
}

/* Writes the file statics in a formated string, given a pointer to the string. */
static void write_file(char *line, const char *mode, int num, const char *user, const char * group, int size, const char *date, const char*fl_name)
{
	sprintf(line, "%s %3d %-4s %-4s %8d %12s %s\r\n", mode, num, user, group, size, date, fl_name);
}

/* Writes the contet of a file to the given client socket. This is used for file download in ACTIVE mode. */
bool retrieve_file(int sock, int client_sock, const char * file_name_with_path, char* acct_str)
{
	char read_buff[SENDBUFSIZE];
	if (client_sock > 0)
		send_repl_control(sock, REPL_150);
	else
	{
		close(client_sock);
		send_repl_control(sock, REPL_425);
		return false;
	}

	char* file_name = basename((char*)file_name_with_path);
	struct stat s_buff;
	int status = stat(file_name, &s_buff);
	if (status)
	{
		close(client_sock);
		if (errno == ENOENT)
			send_repl_control(sock, REPL_450_NOT_EXISTED);
		else
			send_repl_control(sock, REPL_450);
		return false;
	}
	int b_mask = s_buff.st_mode & S_IFMT;
	if (b_mask != S_IFREG)
	{
		close(client_sock);
		send_repl_control(sock, REPL_451_NOT_REGULAR);
		return false;
	}
	
	if (!check_acct(file_name, acct_str))
	{
		send_repl_control(sock, REPL_531);
		close(client_sock);
		return false;
	}
		
	int fpr = open(file_name, O_RDONLY);
	if (fpr < 0)
	{
		close(client_sock);
		send_repl_control(sock, REPL_451);
		return false;
	}

	int len = read(fpr, read_buff, SENDBUFSIZE);
	while (len > 0)
	{
		send_repl_data_len(client_sock, read_buff, len);
		len = read(fpr, read_buff, SENDBUFSIZE);
	}
	close(fpr);
	send_repl_control(sock, REPL_226);
	close(client_sock);
	return true;
}

/* Writes a file on the server, given an open client socket descriptor.
	We are waiting for file contents on this descriptor. */
bool stou_file(int sock, int client_sock, int fpr, char* filename)
{
	if (client_sock > 0)
		send_repl_control(sock, REPL_150);
	else
	{
		close(client_sock);
		send_repl_control(sock, REPL_425);
		return false;
	}

	char read_buff[SENDBUFSIZE];
	if (fpr < 0)
	{
		close(client_sock);
		send_repl_control(sock, REPL_451);
		return false;
	}
	
	int len = recv(client_sock, read_buff, SENDBUFSIZE, 0);
	while (len > 0)
	{
		if (write(fpr, read_buff, len) == -1)
		{
			send_repl_control(sock, REPL_451);
			return false;
		}
		len = recv(client_sock, read_buff, SENDBUFSIZE, 0);
	}
	close(client_sock);
	close(fpr);
	char passphrase[PASSPHRASE_SIZE];
	if (generate_passphrase(filename, passphrase))
	{
		char reply[SENDBUFSIZE];
		reply[0] = '\0';
		int reply_len = sprintf(reply, REPL_226_WITH_HASH, passphrase);
		reply[reply_len] = '\0';
		send_repl_control(sock, reply);
		return true;
	}
	else
	{
		send_repl_control(sock, REPL_451);
		return false;
	}
}

/* Writes a file, given a file path(name and location) and open client socket. */
bool store_file(int sock, int client_sock, const char * file_name_with_path)
{
	char* file_name = basename((char*)file_name_with_path);
	
	struct stat s_buff;
	int status = stat(file_name, &s_buff);
	if (!status)
	{
		close(client_sock);
		send_repl_control(sock, REPL_451);
		return false;
	}

	int fpr = open(file_name, O_WRONLY | O_CREAT, 0644);
	return stou_file(sock, client_sock, fpr, file_name);
}

/* Change current working dir. */
bool change_dir(int sock, char *current_dir, char *target_dir_with_path, char* acct_str, char* home_dir)
{
	char* target_dir = basename(target_dir_with_path);
	DIR *dir = ensure_dir_exists(sock, target_dir);
	if (dir)
	{
		closedir(dir);
		
		if (is_special_dir(target_dir))
		{
			//prevent from leaving home directory with ".."
			if ((strlen(target_dir) == 2) && (!strcmp(current_dir, home_dir)))
			{
				send_repl_control(sock, REPL_550_HOME_DIR);
				return false;
			}
		}
		else
		{
			if (!check_acct(target_dir, acct_str))
			{
				send_repl_control(sock, REPL_531);
				return false;
			}
		}

		int status = chdir(target_dir);
		if ((!status) && (getcwd((char*)current_dir, MAXPATHLEN)))
		{
			send_repl_control(sock, REPL_250);
			return true;
		}
	}
	send_repl_control(sock, REPL_550);
	return false;
}

/* Show stats about a file. If the file is a directory show stats about its content. */
bool stat_file(int sock, char *file_name_with_path, char *reply)
{
	char* file_name = basename(file_name_with_path);
	char saved_dir[MAXPATHLEN];
	char cur_dir[MAXPATHLEN];
	struct stat s_buff;
	int status = stat(file_name, &s_buff);
	if (!status)
	{
		reply[0] = '\0';
		int len = sprintf(reply, REPL_211_STATUS, file_name);
		send_repl_control(sock, reply);

		int b_mask = s_buff.st_mode & S_IFMT;
		if (b_mask == S_IFDIR)
		{
			if (getcwd(saved_dir, MAXPATHLEN))
			{
				int status = chdir(file_name);
				getcwd(cur_dir, MAXPATHLEN);
				if (status)
				{
					send_repl_control(sock, REPL_450);
					send_repl_control(sock, REPL_211_END);
					return false;
				}
				else
				{
					if (!write_list(sock, sock, cur_dir))
					{
						send_repl_control(sock, REPL_450);
						send_repl_control(sock, REPL_211_END);
						chdir(saved_dir);
						return false;
					}
					chdir(saved_dir);
				}
			}
			else
			{
				send_repl_control(sock, REPL_450);
				send_repl_control(sock, REPL_211_END);
				return false;
			}
		}
		if (b_mask == S_IFREG)
		{
			char line[FILE_INFO_BUF_SIZE];
			if (get_file_info_stat(file_name, line, &s_buff))
			{
				if (!send_repl_data(sock, line))
				{
					send_repl_control(sock, REPL_450);
					send_repl_control(sock, REPL_211_END);
					return false;
				}
			}
		}
		send_repl_control(sock, REPL_211_END);
	}
	else
	{
		send_repl_control(sock, REPL_450);
		return false;
	}
	return true;
}

/* Create a directory, called "new_dir" */
bool make_dir(int sock, char *new_dir_with_path, char *reply)
{
	char* new_dir = basename(new_dir_with_path);
	struct stat s_buff;
	int status = stat(new_dir, &s_buff);
	if (!status)
	{
		send_repl_control(sock, REPL_550);
		return false;
	}
	status = mkdir(new_dir, 0755);
	if (status)
	{
		send_repl_control(sock, REPL_550);
		return false;
	}

	char passphrase[PASSPHRASE_SIZE];
	if (generate_passphrase(new_dir, passphrase))
	{
		reply[0] = '\0';
		int len = sprintf(reply, REPL_257_WITH_HASH, new_dir, passphrase);
		reply[len] = '\0';
		send_repl_control_len(sock, reply, len);
		return true;
	}
	else
	{
		send_repl_control(sock, REPL_451);
		return false;
	}
}

/* Delete the directory called "removed_dir" */
bool remove_dir(int sock, char *removed_dir_with_path, char* acct_str)
{
	char* removed_dir = basename(removed_dir_with_path);
	if (is_special_dir(removed_dir))
	{
		send_repl_control(sock, REPL_550);
		return false;
	}
	struct stat s_buff;
	int status = stat(removed_dir, &s_buff);
	if (status)
	{
		send_repl_control(sock, REPL_550);
		return false;
	}
	int b_mask = s_buff.st_mode & S_IFMT;
	if (b_mask != S_IFDIR)
	{
		send_repl_control(sock, REPL_550);
		return false;
	}

	if (!check_acct(removed_dir, acct_str))
	{
		send_repl_control(sock, REPL_531);
		return false;
	}

	status = rmdir(removed_dir);
	if (status)
	{
		send_repl_control(sock, REPL_550);
		return false;
	}
	send_repl_control(sock, REPL_250);
	return true;
}

/* Delete a file, given its name */
bool delete_file(int sock, char *delete_file_with_path, char* acct_str)
{
	char* delete_file = basename(delete_file_with_path);
	struct stat s_buff;
	int status = stat(delete_file, &s_buff);
	if (status)
	{
		send_repl_control(sock, REPL_550);
		return false;
	}
	int b_mask = s_buff.st_mode & S_IFMT;
	if (b_mask != S_IFREG)
	{
		send_repl_control(sock, REPL_550);
		return false;
	}
	if (!check_acct(delete_file, acct_str))
	{
		send_repl_control(sock, REPL_531);
		return false;
	}
	status = unlink(delete_file);
	if (status)
	{
		send_repl_control(sock, REPL_450);
		return false;
	}
	send_repl_control(sock, REPL_250);
	return true;
}

/* Rename a file (not directory). If operation is not successfull - return FALSE */
bool rename_fr(int sock, char *from_with_path, char *to_with_path, char* acct_str)
{
	char* from = basename(from_with_path);
	char* to = basename(to_with_path);
	if (is_special_dir(from) || is_special_dir(to))
	{
		send_repl_control(sock, REPL_553);
		return false;
	}
	struct stat s_buff;

	int status = stat(to, &s_buff);
	if (!status)
	{
		send_repl_control(sock, REPL_553);
		return false;
	}

	status = stat(from, &s_buff);
	if (status)
	{
		send_repl_control(sock, REPL_553);
		return false;
	}	
	int b_mask = s_buff.st_mode & S_IFMT;
	if (b_mask == S_IFREG)
	{
		if (!check_acct(from, acct_str))
		{
			send_repl_control(sock, REPL_531);
			return false;
		}
		
		int status = rename(from, to);
		if (status)
		{
			send_repl_control(sock, REPL_553);
			return false;
		}
	}
	else
	{
		send_repl_control(sock, REPL_553);
		return false;
	}

	char passphrase[PASSPHRASE_SIZE];
	char reply[SENDBUFSIZE];
	if (generate_passphrase(to, passphrase))
	{
		reply[0] = '\0';
		int len = sprintf(reply, REPL_250_WITH_HASH, passphrase);
		reply[len] = '\0';
		send_repl_control_len(sock, reply, len);
		return true;
	}
	else
	{
		send_repl_control(sock, REPL_451);
		return false;
	}
}



