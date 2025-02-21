#include "fs/fcntl.h"
#include "ulib.h"

enum command_type {
	EXEC_CMD,
	PIPE_CMD,
	REDIR_CMD,
};

struct command {
	enum command_type type;
};

struct exec_command {
	enum command_type type;
	char *name;
	char **argv;
};

struct pipe_command {
	enum command_type type;
	struct command *left;
	struct command *right;
};

struct redir_command {
	enum command_type type;
	struct command *cmd;
	char *file;
	int omode;
	int fd;
};

char input_buf[256];
char *args_buf[16];
char cwd_cache[128];

void panic(const char *str) __attribute__((noreturn));
void run_cmd(struct command *cmd)  __attribute__((noreturn));

char *safe_gets(char *buf, size_t max_len)
{
	size_t i;
	char c;

	for (i = 0; i + 1 < max_len;) {
		if (read(0, &c, 1) < 1)
			break;
		buf[i++] = c;
		if (c == '\n' || c == '\r')
			break;
	}
	buf[i] = 0;
	return buf;
}

void parse_line(char *input, char **args, size_t max_args)
{
	size_t i;
	char *token;

	char *str = input;
	char *p = NULL;

	while ((p = strchr(str, '|'))) {
	}

	i = 0;
	token = strtok(input, " \t\n\r\v");
	while (token && (i + 1) < max_args) {
		args[i++] = token;
		token = strtok(NULL, " \t\n\r\v");
	}
	args[i] = NULL;
}

struct command *exec_cmd(char **args)
{
	struct exec_command *ecmd = malloc(sizeof(*ecmd));
	if (!ecmd)
		panic("malloc failed");
	ecmd->type = EXEC_CMD;
	ecmd->name = args[0];
	ecmd->argv = args;
	return (struct command *)ecmd;
}

struct command *redir_cmd(char **args, size_t i, int omode, int fd)
{
	struct redir_command *rcmd = malloc(sizeof(*rcmd));
	if (!rcmd)
		panic("malloc failed");
	args[i] = NULL;
	rcmd->type = REDIR_CMD;
	rcmd->cmd = exec_cmd(args);
	rcmd->file = args[i + 1];
	rcmd->omode = omode;
	rcmd->fd = fd;
	return (struct command *)rcmd;
}

struct command *parse_cmd(char **args, size_t max_args)
{
	size_t i;
	struct pipe_command *pcmd;

	for (i = 0; args[i] && i < max_args; i++) {
		if (!strcmp(args[i], "|")) {
			args[i] = NULL;
			pcmd = malloc(sizeof(*pcmd));
			if (!pcmd)
				panic("malloc failed");
			pcmd->type = PIPE_CMD;
			pcmd->left = exec_cmd(args);
			args = args + i + 1;
			max_args = max_args - i - 1;
			pcmd->right = parse_cmd(args, max_args);
			return (struct command *)pcmd;
		} else if (!strcmp(args[i], ">")) {
			return redir_cmd(args, i, O_CREAT | O_TRUNC | O_WRONLY,
					 1);
		} else if (!strcmp(args[i], ">>")) {
			return redir_cmd(args, i, O_CREAT | O_APPEND | O_WRONLY,
					 1);
		} else if (!strcmp(args[i], "2>")) {
			return redir_cmd(args, i, O_CREAT | O_TRUNC | O_WRONLY,
					 2);
		} else if (!strcmp(args[i], "2>>")) {
			return redir_cmd(args, i, O_CREAT | O_APPEND | O_WRONLY,
					 2);
		} else if (!strcmp(args[i], "<")) {
			return redir_cmd(args, i, O_RDONLY, 0);
		}
	}

	return exec_cmd(args);
}

void run_cmd(struct command *cmd)
{
	struct exec_command *ecmd;
	struct pipe_command *pcmd;
	struct redir_command *rcmd;
	pid_t cpid;
	int pipefd[2];
	int fd;

	switch (cmd->type) {
	case EXEC_CMD:
		ecmd = (struct exec_command *)cmd;
		if (!ecmd->name)
			exit(1);
		execvp(ecmd->name, ecmd->argv);
		panic("exec failed");
		break;
	case PIPE_CMD:
		pcmd = (struct pipe_command *)cmd;
		if (pipe(pipefd) < 0)
			panic("pipe failed");
		cpid = fork();
		if (cpid < 0)
			panic("fork failed");
		if (cpid == 0) {
			close(pipefd[0]);
			dup2(pipefd[1], 1);
			run_cmd(pcmd->left);
		}
		cpid = fork();
		if (cpid < 0)
			panic("fork failed");
		if (cpid == 0) {
			close(pipefd[1]);
			dup2(pipefd[0], 0);
			run_cmd(pcmd->right);
		}
		close(pipefd[0]);
		close(pipefd[1]);
		wait(NULL);
		wait(NULL);
		exit(0);
		break;
	case REDIR_CMD:
		rcmd = (struct redir_command *)cmd;
		fd = open(rcmd->file, rcmd->omode);
		if (fd < 0) {
			dprintf(2, "cannot open %s\n", rcmd->file);
			exit(1);
		}
		dup2(fd, rcmd->fd);
		close(fd);
		run_cmd(rcmd->cmd);
		break;
	default:
		exit(1);
	}
}

void end_cmd(struct command *cmd)
{
	if (cmd->type == PIPE_CMD) {
		end_cmd(((struct pipe_command *)cmd)->left);
		end_cmd(((struct pipe_command *)cmd)->right);
	} else if (cmd->type == REDIR_CMD) {
		end_cmd(((struct redir_command *)cmd)->cmd);
	}
	free(cmd);
}

int get_cmd(char *buf, size_t max_len)
{
	printf("shell> %s$ ", cwd_cache);
	safe_gets(buf, max_len);
	if (buf[0] == 0)
		return -1;
	else
		return 0;
}

int main(void)
{

	pid_t cpid;
	struct command *cmd;

	if (!getcwd(cwd_cache, sizeof(cwd_cache)))
		panic("getcwd failed");

	while (!get_cmd(input_buf, sizeof(input_buf))) {
		parse_line(input_buf, args_buf, sizeof(args_buf));
		if (!strcmp(args_buf[0], "cd")) {
			if (!args_buf[1])
				continue;
			if (chdir(args_buf[1]) < 0) {
				dprintf(2, "chdir failed\n");
				continue;
			}
			if (!getcwd(cwd_cache, sizeof(cwd_cache)))
				panic("getcwd failed");
		} else if (!strcmp(args_buf[0], "exit")) {
			printf("exit\n");
			exit(0);
		} else if (!strcmp(args_buf[0], "pwd")) {
			printf("%s\n", cwd_cache);
		} else {
			cmd = parse_cmd(args_buf, sizeof(args_buf));
			cpid = fork();
			if (cpid < 0) {
				dprintf(2, "fork failed\n");
				end_cmd(cmd);
				continue;
			}
			if (cpid == 0)
				run_cmd(cmd);
			wait(NULL);
			end_cmd(cmd);
		}
	}

	printf("\nexit\n");

	return 0;
}

void panic(const char *str)
{
	dprintf(2, "%s\n", str);
	exit(1);
}
