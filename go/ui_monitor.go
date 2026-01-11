package main

import (
	"bufio"
	"fmt"
	"net"
	"os"
)

// StartSocketServer listens for audit events from the C++ components
func StartSocketServer(socketPath string) error {
	// 1. Cleanup old socket file if it exists
	if _, err := os.Stat(socketPath); err == nil {
		os.Remove(socketPath)
	}

	// 2. Start listening (using "unix" for STREAM or "unixgram" for DATAGRAM)
	listener, err := net.Listen("unix", socketPath)
	if err != nil {
		return fmt.Errorf("failed to listen on socket: %w", err)
	}

	// 3. Set permissions so the C++ PAM module (root) and UI can both access it
	os.Chmod(socketPath, 0777)

	go func() {
		defer listener.Close()
		for {
			// Accept new connection
			conn, err := listener.Accept()
			if err != nil {
				fmt.Printf("Accept error: %v\n", err)
				continue
			}

			// Handle the connection in a separate goroutine
			go handleConnection(conn)
		}
	}()

	return nil
}

func handleConnection(conn net.Conn) {
	defer conn.Close()
	fmt.Printf("New client connected: %s\n", conn.RemoteAddr())

	// Use a scanner to read messages line by line
	scanner := bufio.NewScanner(conn)
	for scanner.Scan() {
		message := scanner.Text()

		// 💡 This is where you trigger your Go UI updates
		processIncomingMessage(message)
	}

	if err := scanner.Err(); err != nil {
		fmt.Printf("Socket read error: %v\n", err)
	}
}

func processIncomingMessage(msg string) {
	// For your Sunday Sprint: just print it to verify communication
	fmt.Printf("[AUDIT EVENT]: %s\n", msg)
}


func main() {
    socketPath := "/tmp/ui_monitor.sock"
    fmt.Println("🚀 Starting SudoMonitor Go Bridge...")

    // This channel is the bridge
    messages := make(chan string, 10)

    // Pass the channel to the server
    err := StartSocketServer(socketPath)
    if err != nil {
       fmt.Printf("❌ Critical Error: %v\n", err)
       os.Exit(1)
    }

    fmt.Printf("📍 Listening on UDS: %s\n", socketPath)

    // The Main Event Loop (remove 'go' if this is the only thing running,
    // otherwise the program will exit immediately!)
    for msg := range messages {
        fmt.Printf("🔔 New Audit Event: %s\n", msg)
    }
}