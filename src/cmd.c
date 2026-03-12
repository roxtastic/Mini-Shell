// SPDX-License-Identifier: BSD-3-Clause
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	/* TODO: Execute cd. */
	if (dir == NULL)
		return 0;
	if (dir->next_word != NULL)
		return 0;
	//comanda e formata din 2 cuvinte: cd si directorul
	char *path = get_word(dir); //obtin calea directorului

	if (chdir(path) != 0) {
		fprintf(stderr, "nu am gasit\n");
		free(path);
		return 1;
	}
	char cwd[1024]; //current working directory

	if (getcwd(cwd, sizeof(cwd)) != NULL) //daca exista, inlocuim in variabila de mediu PWD
		setenv("PWD", cwd, 1);
	free(path);
	return 0;
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
	char *verb = get_word(s->verb);
	/* TODO: If builtin command, execute the command. */
	if (strcmp(verb, "cd") == 0 || strcmp(verb, "pwd") == 0 ||
		strcmp(verb, "exit") == 0 || strcmp(verb, "quit") == 0) {
		int res = 0;
		int old_stdout = dup(STDOUT_FILENO);
		int old_stderr = dup(STDERR_FILENO);

		if (s->out != NULL) {
			char *out_f = get_word(s->out);
			int flags = O_WRONLY | O_CREAT | (s->io_flags & IO_OUT_APPEND ? O_APPEND : O_TRUNC);
			int fd_out = open(out_f, flags, 0644);
			//redirectionez stdout catre fisier
			dup2(fd_out, STDOUT_FILENO);
			close(fd_out);
			free(out_f);
		}
		if (s->err != NULL) {
			//redirectionez stderr catre fisier
			char *err_f = get_word(s->err);
			int flags = O_WRONLY | O_CREAT | (s->io_flags & IO_ERR_APPEND ? O_APPEND : O_TRUNC);
			int fd_err = open(err_f, flags, 0644);

			dup2(fd_err, STDERR_FILENO);
			close(fd_err);
			free(err_f);
		}
		if (strcmp(verb, "cd") == 0) {
			//aplic cd
			res = shell_cd(s->params);
		} else if (strcmp(verb, "pwd") == 0) {
			char cwd[1024];

			if (getcwd(cwd, sizeof(cwd)) != NULL) {
				printf("%s\n", cwd);
				fflush(stdout);
			}
		} else if (strcmp(verb, "exit") == 0 || strcmp(verb, "quit") == 0) {
			res = shell_exit();
		}
		//restaurare descriptori std
		dup2(old_stdout, STDOUT_FILENO);
		dup2(old_stderr, STDERR_FILENO);
		close(old_stdout);
		close(old_stderr);
		free(verb);
		return res;
	}
	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */
	/* TODO: If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */
	if (s->verb->next_part != NULL && strcmp(s->verb->next_part->string, "=") == 0) {
		const char *name = s->verb->string;
		char *value = get_word(s->verb->next_part->next_part);
		int res = setenv(name, value, 1);

		free(value);
		free(verb);
		return res;
	}
	pid_t pid = fork();

	if (pid < 0)
		return -1;

	if (pid == 0) {
		if (s->in != NULL) {
			char *in_f = get_word(s->in);
			int fd_in = open(in_f, O_RDONLY);

			if (fd_in < 0) {
				free(in_f);
				exit(1);
			}
			dup2(fd_in, STDIN_FILENO);
			close(fd_in);
			free(in_f);
		}

		if (s->out != NULL) {
			char *out_f = get_word(s->out);
			int flags = O_WRONLY | O_CREAT | (s->io_flags & IO_OUT_APPEND ? O_APPEND : O_TRUNC);
			int fd_out = open(out_f, flags, 0644);

			dup2(fd_out, STDOUT_FILENO);
			if (s->err != NULL) {
				char *err_f = get_word(s->err);

				if (strcmp(out_f, err_f) == 0)
					dup2(fd_out, STDERR_FILENO);
				free(err_f);
			}
			close(fd_out);
			free(out_f);
		}
		if (s->err != NULL) {
			char *err_f = get_word(s->err);
			bool already_redirected = false;

			if (s->out != NULL) {
				char *out_f = get_word(s->out);

				if (strcmp(out_f, err_f) == 0)
					already_redirected = true;
				free(out_f);
			}
			if (!already_redirected) {
				int flags = O_WRONLY | O_CREAT | (s->io_flags & IO_ERR_APPEND ? O_APPEND : O_TRUNC);
				int fd_err = open(err_f, flags, 0644);

				dup2(fd_err, STDERR_FILENO);
				close(fd_err);
			}
			free(err_f);
		}
		int argc;
		char **argv = get_argv(s, &argc);

		execvp(verb, argv);
		fprintf(stderr, "Execution failed for '%s'\n", verb);
		exit(EXIT_FAILURE);
	} else {
		int status;

		waitpid(pid, &status, 0);
		free(verb);
		return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	}
}
/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */
	pid_t pid1 = fork();

	if (pid1 == 0)
		exit(parse_command(cmd1, level + 1, father));
	pid_t pid2 = fork();

	if (pid2 == 0)
		exit(parse_command(cmd2, level + 1, father));
	int status1, status2;

	waitpid(pid1, &status1, 0);
	waitpid(pid2, &status2, 0);
	return WEXITSTATUS(status2); /* TODO: Replace with actual exit status. */
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	int pipe_fds[2];

	if (pipe(pipe_fds) == -1)
		return false;
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */
	pid_t pid1 = fork();

	if (pid1 == 0) {
		close(pipe_fds[READ]);
		dup2(pipe_fds[WRITE], STDOUT_FILENO);
		close(pipe_fds[WRITE]);
		exit(parse_command(cmd1, level + 1, father));
	}
	pid_t pid2 = fork();

	if (pid2 == 0) {
		close(pipe_fds[WRITE]);
		dup2(pipe_fds[READ], STDIN_FILENO);
		close(pipe_fds[READ]);
		exit(parse_command(cmd2, level + 1, father));
	}
	close(pipe_fds[READ]);
	close(pipe_fds[WRITE]);

	int status1, status2;

	waitpid(pid1, &status1, 0);
	waitpid(pid2, &status2, 0);
	return WEXITSTATUS(status2); /* TODO: Replace with actual exit status. */
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* TODO: sanity checks */

	if (c->op == OP_NONE) {
		/* TODO: Execute a simple command. */

		return parse_simple(c->scmd, level, c); /* TODO: Replace with actual exit code of command. */
	}
	int res1;

	switch (c->op) {
	case OP_SEQUENTIAL:
		/* TODO: Execute the commands one after the other. */
		parse_command(c->cmd1, level + 1, c);
		return parse_command(c->cmd2, level + 1, c);

	case OP_PARALLEL:
		/* TODO: Execute the commands simultaneously. */
		return run_in_parallel(c->cmd1, c->cmd2, level + 1, c);

	case OP_CONDITIONAL_NZERO:
		/* TODO: Execute the second command only if the first one
		 * returns non zero.
		 */
		res1 = parse_command(c->cmd1, level + 1, c);
		if (res1 != 0)
			return parse_command(c->cmd2, level + 1, c);
		return res1;

	case OP_CONDITIONAL_ZERO:
		/* TODO: Execute the second command only if the first one
		 * returns zero.
		 */
		res1 = parse_command(c->cmd1, level + 1, c);
		if (res1 == 0)
			return parse_command(c->cmd2, level + 1, c);
		return res1;

	case OP_PIPE:
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		return run_on_pipe(c->cmd1, c->cmd2, level + 1, c);

	default:
		return SHELL_EXIT;
	}
	return 0; /* TODO: Replace with actual exit code of command. */
}
