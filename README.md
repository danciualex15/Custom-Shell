# Custom Shell

A custom Unix-like shell implementation written in C, supporting command parsing, execution, and standard I/O redirection.

## Features

- **Interactive Shell**: Provides a command prompt for user interaction
- **Command Execution**: Executes parsed commands
- **Pipe Support**: Supports command piping for connecting processes

## Build Instructions

Run `make` command with not parameters.

## Usage

Run the shell using `./custom-shell`.

## Architecture

### Main Components

- **main.c**:
  - Reads user input line by line
  - Calls the parser to build a command tree
  - Executes the parsed commands
  - Handles shell exit

- **cmd.c/cmd.h**:
  - `parse_command()` executes recursively command trees
  - `shell_cd()` is a built-in directory change command
  - Handles I/O redirection (stdin/stdout/stderr)
  - Manages process creation and synchronization

- **utils.c/utils.h**:
  - Helper functions for command processing
  - Error handling

- **Parser (util/parser/)**:
  - Lexer (parser.l): Tokenizes input
  - Parser (parser.y): Builds abstract syntax tree
  - Represents commands as `command_t` structures
