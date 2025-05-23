# TCP Chat Project (C)

A multithreaded TCP-based chat application written in C.  
This project includes both a server and a client communicating over TCP sockets. Each client connects with a nickname and can participate in public or private chat. The server handles multiple clients concurrently using POSIX threads (`pthreads`) and supports several user commands.

## Features

- Nickname-based login system
- Broadcast public messages to all connected users
- Send private messages using `@nickname` format
- Client highlights private messages in red
- Commands:
  - `/list` – view all connected users
  - `/quit` – disconnect gracefully
  - `/help` – list available commands
- Thread-safe client management using mutexes
- Server logs all messages with sender information
- Graceful shutdown on server with Ctrl+C

## Compilation

Use a C compiler with POSIX thread support:

```bash
gcc -pthread -o server server.c
gcc -pthread -o client client.c
```

## Usage

1. Run the server in one terminal:

   ```bash
   ./server
   ```

2. Run the client in another terminal (you can run multiple clients):

   ```bash
   ./client
   ```

3. After entering a nickname, you can:
   - Send public messages by typing text and pressing Enter
   - Send private messages by typing `@nickname your message`
   - Use `/list`, `/help`, and `/quit` commands as needed

## Notes

- The server supports up to 100 concurrent clients by default.
- Each message is terminated with a newline and processed line-by-line.
- Private messages are displayed in red (ANSI escape codes) for better visibility.
