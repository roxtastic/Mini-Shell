# Mini-Shell
Unix shell implementation in C — handles I/O redirection, pipes, environment variables, and conditional execution. Built from scratch using fork, exec, dup2, and pipe syscalls.

## What it does

- Runs external programs using `fork` + `execvp`
- Built-in commands: `cd`, `pwd`, `exit`, `quit`
- Environment variable assignment (`VAR=value`)
- I/O redirection (`>`, `>>`, `<`, `2>`)
- Pipes (`cmd1 | cmd2`)
- Sequential execution (`cmd1 ; cmd2`)
- Parallel execution (`cmd1 & cmd2`)
- Conditional execution (`&&`, `||`)

## How it works

The project uses a provided command parser that builds a tree of `command_t` nodes. My implementation walks that tree recursively and executes each node depending on its type.
For external commands, a child process is forked, file descriptors are set up using `dup2`, and the binary is loaded with `execvp`. For pipes, two children are forked with their stdin/stdout wired together through an anonymous pipe. Built-ins like `cd` run in the parent process since they need to affect the shell's own state.
```
parse_command()
├── OP_NONE       → parse_simple() → fork + execvp
├── OP_PIPE       → run_on_pipe()  → pipe() + 2x fork
├── OP_PARALLEL   → run_in_parallel() → 2x fork
├── OP_SEQUENTIAL → cmd1, then cmd2
├── OP_CONDITIONAL_ZERO  → cmd1 && cmd2
└── OP_CONDITIONAL_NZERO → cmd1 || cmd2
```

## Build & run
```bash
make
./mini-shell
```

To run the checker:
```bash
make check
```

## Project structure
```
.
├── src/
│   ├── cmd.c          # Core command execution logic
│   ├── cmd.h
│   ├── utils.c        # get_word(), get_argv() helpers
│   ├── utils.h
│   └── mini-shell.c   # Shell entry point (REPL loop)
└── util/
    └── parser/        # Provided command parser
```

## Relevant syscalls

`fork`, `execvp`, `waitpid`, `pipe`, `dup2`, `open`, `close`, `chdir`, `getcwd`, `setenv`, `getenv`

Building this gave me a much more concrete understanding of how processes, file descriptors, and I/O redirection actually work under the hood — things that come up constantly when writing shell scripts, debugging pipelines, or working with container runtimes and CI systems. Tracing real shell behavior with `strace` to inform the implementation was especially useful for building intuition around Linux internals.
