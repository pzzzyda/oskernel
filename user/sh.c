#include "fs/fcntl.h"
#include "ulib.h"

#define MAX_INPUT 128
#define MAX_ARGS 10

#define TOK_ILLEGAL 0
#define TOK_EOF 1
#define TOK_ARG 2
#define TOK_PIPE 3
#define TOK_REDIR_IN 4
#define TOK_REDIR_OUT 5
#define TOK_REDIR_OUT_A 6
#define TOK_REDIR_ERR 7
#define TOK_REDIR_ERR_A 8

struct token {
	int type;
	int len;
	char *lexeme;
	struct token *next;
};

#define CMD_NONE 0
#define CMD_EXEC 1
#define CMD_PIPE 2
#define CMD_REDIR 3

struct command {
	int type;
};

struct exec_command {
	int type;
	char *argv[MAX_ARGS];
};

struct pipe_command {
	int type;
	struct command *left;
	struct command *right;
};

struct redir_command {
	int type;
	struct command *cmd;
	char *file;
	int omode;
	int fd;
};

void panic(const char *str) __attribute__((noreturn));
void *malloc1(size_t size);
pid_t fork1(void);
struct token *parse_line(char *input);
struct command *parse_cmd(struct token *toks);
void run_cmd(struct command *cmd) __attribute__((noreturn));
void end_cmd(struct command *cmd);
void free_tokens(struct token *toks);

struct token *token_new(int type, char *lexeme, int len)
{
	struct token *tok = malloc1(sizeof(*tok));
	tok->type = type;
	tok->len = len;
	tok->lexeme = lexeme;
	tok->next = NULL;
	return tok;
}

struct exec_command *exec_command_new(void)
{
	struct exec_command *ecmd = malloc1(sizeof(*ecmd));
	memset(ecmd->argv, 0, sizeof(ecmd->argv));
	ecmd->type = CMD_EXEC;
	return ecmd;
}

struct pipe_command *pipe_command_new(struct command *left,
				      struct command *right)
{
	struct pipe_command *pcmd = malloc1(sizeof(*pcmd));
	pcmd->left = left;
	pcmd->right = right;
	pcmd->type = CMD_PIPE;
	return pcmd;
}

struct redir_command *redir_command_new(struct command *cmd, char *file,
					int omode, int fd)
{
	struct redir_command *rcmd = malloc1(sizeof(*rcmd));
	rcmd->cmd = cmd;
	rcmd->file = file;
	rcmd->omode = omode;
	rcmd->fd = fd;
	rcmd->type = CMD_REDIR;
	return rcmd;
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
	case CMD_EXEC:
		ecmd = (struct exec_command *)cmd;
		if (!ecmd->argv[0])
			exit(1);
		execvp(ecmd->argv[0], ecmd->argv);
		dprintf(2, "exec %s failed\n", ecmd->argv[0]);
		exit(1);
		break;
	case CMD_PIPE:
		pcmd = (struct pipe_command *)cmd;
		if (pipe(pipefd) < 0)
			panic("pipe failed");
		cpid = fork1();
		if (cpid == 0) {
			close(pipefd[0]);
			dup2(pipefd[1], 1);
			run_cmd(pcmd->left);
		}
		cpid = fork1();
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
	case CMD_REDIR:
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
	if (cmd->type == CMD_PIPE) {
		end_cmd(((struct pipe_command *)cmd)->left);
		end_cmd(((struct pipe_command *)cmd)->right);
	} else if (cmd->type == CMD_REDIR) {
		end_cmd(((struct redir_command *)cmd)->cmd);
	}
	free(cmd);
}

void free_tokens(struct token *toks)
{
	struct token *tok = NULL;
	while (toks) {
		tok = toks;
		toks = toks->next;
		free(tok);
	}
}

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

int get_cmd(char *buf, size_t max_len)
{
	safe_gets(buf, max_len);
	return !buf[0] ? -1 : 0;
}

int main()
{
	static char cwd[128];
	static char buf[MAX_INPUT];
	struct token *toks = NULL;
	struct command *cmd = NULL;

	if (!getcwd(cwd, sizeof(cwd)))
		panic("getcwd failed");

	while (true) {
		printf("shell> %s$ ", cwd);
		if (get_cmd(buf, MAX_INPUT) < 0)
			break;
		if (buf[0] == '\n' || buf[0] == '\r')
			continue;
		buf[strcspn(buf, "\n\r")] = 0;
		if (!strncmp(buf, "cd ", 3)) {
			if (chdir(buf + 3) < 0) {
				dprintf(2, "cannot cd %s\n", buf + 3);
				continue;
			}
			if (!getcwd(cwd, sizeof(cwd)))
				panic("getcwd failed");
		} else if (!strncmp(buf, "exit", 4)) {
			printf("exit\n");
			exit(0);
		} else {
			toks = parse_line(buf);
			cmd = parse_cmd(toks);
			if (fork1() == 0)
				run_cmd(cmd);
			end_cmd(cmd);
			cmd = NULL;
			free_tokens(toks);
			toks = NULL;
			wait(NULL);
		}
	}

	printf("\nexit\n");
	return 0;
}

#define WHITESPACE " \t\n\r\v"

struct token *get_token(char **input)
{
	char *i = *input;
	struct token *tok = NULL;

	while (*i && strchr(WHITESPACE, *i))
		i++;

	switch (*i) {
	case 0:
		tok = token_new(TOK_EOF, "", 0);
		break;
	case '|':
		tok = token_new(TOK_PIPE, i, 1);
		i++;
		break;
	case '>':
		if (*(i + 1) == '>') {
			tok = token_new(TOK_REDIR_OUT_A, i, 2);
			i++;
		} else {
			tok = token_new(TOK_REDIR_OUT, i, 1);
		}
		i++;
		break;
	case '<':
		tok = token_new(TOK_REDIR_IN, i, 1);
		i++;
		break;
	case '2':
		if (*(i + 1) == '>') {
			if (*(i + 2) == '>') {
				tok = token_new(TOK_REDIR_ERR_A, i, 3);
				i += 3;
			} else {
				tok = token_new(TOK_REDIR_ERR, i, 2);
				i += 2;
			}
			break;
		}
	default:
		tok = token_new(TOK_ARG, i, 1);
		while (*i && !strchr(WHITESPACE, *i) && !strchr("2|<>", *i))
			i++;
		tok->len = i - tok->lexeme;
		break;
	}
	*input = i;

	return tok;
}

struct token *parse_line(char *input)
{
	struct token *toks = NULL;
	struct token **ptoks = &toks;
	while (true) {
		*ptoks = get_token(&input);
		if ((*ptoks)->type == TOK_EOF)
			break;
		ptoks = &((*ptoks)->next);
	}
	return toks;
}

struct command *parse_exec(struct token *toks)
{
	struct exec_command *ecmd = exec_command_new();
	int argc = 0;
	struct token *tok = toks;
	while (tok->type != TOK_EOF) {
		tok->lexeme[tok->len] = 0;
		ecmd->argv[argc++] = tok->lexeme;
		tok = tok->next;
	}
	return (struct command *)ecmd;
}

struct command *parse_pipe(struct token *left, struct token *right)
{
	struct pipe_command *pcmd = NULL;
	if (!left || left->type != TOK_ARG)
		panic("illegal command for pipe");
	if (!right || right->type == TOK_EOF)
		panic("missing arguments for pipe");
	pcmd = pipe_command_new(parse_exec(left), parse_cmd(right));
	return (struct command *)pcmd;
}

struct command *parse_redir(struct token *toks, struct token *op,
			    struct token *file)
{
	struct redir_command *rcmd = NULL;
	int omode = 0;
	int fd = 0;
	if (!file || file->type != TOK_ARG)
		panic("missing arguments for redirect");
	switch (op->type) {
	case TOK_REDIR_IN:
		omode = O_RDONLY;
		fd = 0;
		break;
	case TOK_REDIR_OUT:
		omode = O_WRONLY | O_CREAT | O_TRUNC;
		fd = 1;
		break;
	case TOK_REDIR_OUT_A:
		omode = O_WRONLY | O_CREAT | O_APPEND;
		fd = 1;
		break;
	case TOK_REDIR_ERR:
		omode = O_WRONLY | O_CREAT | O_TRUNC;
		fd = 2;
		break;
	case TOK_REDIR_ERR_A:
		omode = O_WRONLY | O_CREAT | O_APPEND;
		fd = 2;
		break;
	default:
		panic("unsupported redirect operation");
	}
	op->type = TOK_EOF;
	file->lexeme[file->len] = 0;
	rcmd = redir_command_new(parse_exec(toks), file->lexeme, omode, fd);
	return (struct command *)rcmd;
}

struct command *parse_cmd(struct token *toks)
{
	struct token *tok = NULL;

	if (toks->type == TOK_EOF)
		return NULL;

	for (tok = toks; tok; tok = tok->next) {
		switch (tok->type) {
		case TOK_PIPE:
			tok->type = TOK_EOF;
			return parse_pipe(toks, tok->next);
		case TOK_REDIR_IN:
		case TOK_REDIR_OUT:
		case TOK_REDIR_OUT_A:
		case TOK_REDIR_ERR:
		case TOK_REDIR_ERR_A:
			return parse_redir(toks, tok, tok->next);
		case TOK_ARG:
			break;
		case TOK_EOF:
			break;
		default:
			panic("illegal token");
			break;
		}
	}

	return parse_exec(toks);
}

void *malloc1(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr)
		panic("malloc failed");
	return ptr;
}

pid_t fork1(void)
{
	pid_t pid = fork();
	if (pid < 0)
		panic("fork failed");
	return pid;
}

void panic(const char *str)
{
	dprintf(2, "%s\n", str);
	exit(1);
}
