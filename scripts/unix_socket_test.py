#!/usr/bin/env python3
import socket
import sys

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <socket_path>")
        sys.exit(1)

    socket_path = sys.argv[1]

    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        client.connect(socket_path)
    except FileNotFoundError:
        print(f"Error: Socket path '{socket_path}' not found.")
        sys.exit(1)
    except ConnectionRefusedError:
        print(f"Error: Connection refused at '{socket_path}'.")
        sys.exit(1)
    except Exception as e:
        print(f"Connection error: {e}")
        sys.exit(1)

    print(f"Connected to {socket_path}. Type messages (or 'quit' to exit).")

    try:
        while True:
            msg = input("> ")
            if msg.lower() in ("quit", "exit"):
                break

            # Send message
            client.sendall(msg.encode("utf-8"))

            # Receive reply (up to 4KB)
            reply = client.recv(4096)
            if not reply:
                print("Server closed the connection.")
                break

            print(f"< {reply.decode('utf-8', errors='ignore')}")
    except KeyboardInterrupt:
        print("\nInterrupted by user.")
    finally:
        client.close()
        print("Connection closed.")

if __name__ == "__main__":
    main()
