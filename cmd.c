// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

static bool check_std_red(simple_command_t *s)
{
	return s->in != NULL || s->out != NULL || s->err != NULL;
}

int find_char(const char *str, char c)
{
	if (str == NULL)
		return -1;

	for (int i = 0; str[i] != '\0'; i++)
		if (str[i] == c)
			return i;

	return -1;
}

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	/* TODO: Execute cd. */
	if (dir == NULL)
		return false;
	char *path = get_word(dir);

	if (path == NULL)
		return false;
	int res = chdir(path);

	free(path);

	return res == 0;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */

	return SHELL_EXIT; /* TODO: Replace with actual exit code. */
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* TODO: Sanity checks. */
	if (s == NULL || s->verb == NULL)
		return shell_exit();

	char *cmd_name = get_word(s->verb);
	char *saveptr;

	/* TODO: If builtin command, execute the command. */
	if ((!strcmp(cmd_name, "exit") || !strcmp(cmd_name, "quit")))
		return shell_exit();

	if (!strcmp(cmd_name, "cd") && !check_std_red(s)) {
		if (shell_cd(s->params))
			return 0;
		return 1;
	}

	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */

	if (find_char(cmd_name, '=') >= 0) {
		char *name = strtok_r(cmd_name, "=", &saveptr);
		char *val = strtok_r(NULL, "=", &saveptr);

		if (name == NULL || val == NULL)
			return 1;
		if (setenv(name, val, 1) < 0)
			return 1;
		return 0;
	}

	// daca nu e comanda cd facem verificarea daca comanda exista si poate fi executata
	if (strcmp(cmd_name, "cd")) {
		char exec_path[256];

		snprintf(exec_path, sizeof(exec_path), "/bin/%s", cmd_name);
		// daca executabilul nu se afla in /bin atunci verficam daca este executabil in folderul actual
		if (access(exec_path, X_OK) != 0) {
			snprintf(exec_path, sizeof(exec_path), "./%s", cmd_name);
			// daca nu e nici in folderul actual atunci returnam eroare
			if (access(exec_path, X_OK) != 0)
				return 1;
		}
	}


	/* TODO: If external command:
	 *   1. Fork new process
	 *	 2c. Perform redirections in child
	 *	 3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */

	int pid = fork();

	if (pid != 0) {
		int status;

		waitpid(pid, &status, 0);
		return WEXITSTATUS(status);
	}

	bool are_std_red = false;

	int fd_input = -1;
	int fd_output = -1;
	int fd_error = -1;

	int fd_backup_input = dup(0);
	int fd_backup_output = dup(1);
	int fd_backup_error = dup(2);

	if (s->in != NULL) {
		char *input_string = get_word(s->in);

		fd_input = open(input_string, O_RDONLY | O_CREAT, 0644);
		free(input_string);
		if (fd_input < 0)
			return 1;
		dup2(fd_input, 0);
		are_std_red = true;
	}

	if (s->out != NULL) {
		char *output_string = get_word(s->out);

		if (s->io_flags == IO_OUT_APPEND || s->err != NULL)
			fd_output = open(output_string, O_WRONLY | O_CREAT | O_APPEND, 0644);
		else
			fd_output = open(output_string, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		free(output_string);
		if (fd_output < 0)
			return 1;
		dup2(fd_output, 1);
		are_std_red = true;
	}

	if (s->err != NULL) {
		char *error_string = get_word(s->err);

		if (s->io_flags != IO_ERR_APPEND)
			fd_error = open(error_string, O_WRONLY | O_CREAT, 0644);
		else
			fd_error = open(error_string, O_WRONLY | O_CREAT | O_APPEND, 0644);
		free(error_string);
		if (fd_error < 0)
			return 1;
		dup2(fd_error, 2);
		are_std_red = true;
	}

	if (are_std_red && !strcmp(s->verb->string, "cd")) {
		bool res_cd = shell_cd(s->params);

		if (s->in) {
			dup2(fd_backup_input, 0);
			close(fd_input);
			close(fd_backup_input);
		}

		if (s->out) {
			dup2(fd_backup_output, 1);
			close(fd_output);
			close(fd_backup_output);
		}

		if (s->err) {
			dup2(fd_backup_error, 2);
			close(fd_error);
			close(fd_backup_error);
		}

		return res_cd ? 0 : 1;
	}

	int length;
	char **args = get_argv(s, &length);

	if (args == NULL)
		return 1;
	int exec_cmd_res = execvp(cmd_name, args);

	if (exec_cmd_res < 0)
		return 1;
	free(args);

	if (s->in != NULL) {
		close(fd_input);
		dup2(fd_backup_input, 0);
		close(fd_backup_input);
	}

	if (s->out != NULL) {
		close(fd_output);
		dup2(fd_backup_output, 1);
		close(fd_backup_output);
	}

	if (s->err != NULL) {
		close(fd_error);
		dup2(fd_backup_error, 2);
		close(fd_backup_error);
	}

	return exec_cmd_res; /* TODO: Replace with actual exit status. */
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */
	int p1 = fork();

	if (!p1)
		exit(parse_command(cmd1, ++level, father));

	int p2 = fork();

	if (!p2)
		exit(parse_command(cmd2, ++level, father));

	wait(&p1);
	wait(&p2);

	return true; /* TODO: Replace with actual exit status. */
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level, command_t *father)
{
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */
	if (cmd1 == NULL || cmd2 == NULL)
		return false;
	char *cmd2_name = NULL;

	if (cmd2->scmd)
		cmd2_name = get_word(cmd2->scmd->verb);
	else
		return false;

	if (!strcmp(cmd2_name, "true")) {
		free(cmd2_name);
		return true;
	}
	if (!strcmp(cmd2_name, "false")) {
		free(cmd2_name);
		return false;
	}

	int pipefd[2];

	if (pipe(pipefd) < 0)
		return false;
	int pid = fork();

	if (pid != 0) {
		close(pipefd[0]);
		int fd_backup = dup(1);
		int status;

		dup2(pipefd[1], 1);
		int res = parse_command(cmd1, ++level, father);

		if (res != 0)
			return false;
		close(pipefd[1]);
		dup2(fd_backup, 1);
		close(fd_backup);
		waitpid(pid, &status, 0);
		return WEXITSTATUS(status) == 0;
	}

	close(pipefd[1]);
	int fd_backup = dup(0);

	dup2(pipefd[0], 0);
	int res = parse_command(cmd2, ++level, father);

	close(pipefd[0]);
	dup2(fd_backup, 0);
	close(fd_backup);
	exit(res);
}


/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* TODO: sanity checks */
	if (c == NULL)
		return shell_exit();

	int res;

	if (c->op == OP_NONE) {
		/* TODO: Execute a simple command. */
		res = parse_simple(c->scmd, level, father);
		if (res && res != shell_exit()) {
			char *cmd = get_word(c->scmd->verb);

			printf("Execution failed for '%s'\n", cmd);
			fflush(stdout);
			free(cmd);
			return 1;
		}
		return res; /* TODO: Replace with actual exit code of command. */
	}

	switch (c->op) {
	case OP_SEQUENTIAL:
		/* TODO: Execute the commands one after the other. */
		res = parse_command(c->cmd1, ++level, c);
		res = parse_command(c->cmd2, ++level, c);
		return res;

	case OP_PARALLEL:
		/* TODO: Execute the commands simultaneously. */
		if (run_in_parallel(c->cmd1, c->cmd2, ++level, c)) {
			res = 0;
			break;
		}
			res = 1;
		return res;

	case OP_CONDITIONAL_NZERO:
		/* TODO: Execute the second command only if the first one
		 * returns non zero.
		 */
		res = parse_command(c->cmd1, ++level, c);
		if (res != 0)
			res = parse_command(c->cmd2, ++level, c);
		return res;

	case OP_CONDITIONAL_ZERO:
		/* TODO: Execute the second command only if the first one
		 * returns zero.
		 */
		res = parse_command(c->cmd1, ++level, c);
		if (res == 0)
			res = parse_command(c->cmd2, ++level, c);
		return res;
	case OP_PIPE:
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		if (run_on_pipe(c->cmd1, c->cmd2, ++level, c)) {
			res = 0;
			break;
		}
			res = 1;
		return res;

	default:
		return shell_exit();
	}

	return res; /* TODO: Replace with actual exit code of command. */
}
