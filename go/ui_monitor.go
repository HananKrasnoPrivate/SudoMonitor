package main

import (
	"fmt"
	"net"
	"os"
	"strings"

	"github.com/charmbracelet/bubbles/table"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
)

var baseStyle = lipgloss.NewStyle().
	BorderStyle(lipgloss.NormalBorder()).
	BorderForeground(lipgloss.Color("240"))

type auditMsg string

type model struct {
	table    table.Model
	socket   *net.UnixConn
}

// Listen for data from the Unix Socket
func waitForActivity(sub chan auditMsg, conn *net.UnixConn) {
	buf := make([]byte, 4096)
	for {
		n, _, err := conn.ReadFromUnix(buf)
		if err != nil {
			continue
		}
		sub <- auditMsg(string(buf[:n]))
	}
}

func (m model) Init() tea.Cmd {
	return nil
}

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "q", "ctrl+c":
			return m, tea.Quit
		}
	case auditMsg:
		// Logic to parse the string: "TYPE|USER|DATA"
		parts := strings.Split(string(msg), "|")
		if len(parts) < 3 {
			parts = []string{"UNKNOWN", "SYSTEM", string(msg)}
		}

		row := table.Row{parts[0], parts[1], parts[2]}
		rows := m.table.Rows()
		rows = append(rows, row)
		if len(rows) > 15 { // Keep only last 15 entries
			rows = rows[1:]
		}
		m.table.SetRows(rows)
	}
	return m, nil
}

func (m model) View() string {
	return "\n 🛡️  SUDO AUDIT REAL-TIME MONITOR (Press 'q' to quit)\n\n" +
		baseStyle.Render(m.table.View()) + "\n"
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: ./ui_monitor /path/to/socket")
		os.Exit(1)
	}
	socketPath := os.Args[1]

	// 1. Set up the Unix Socket
	_ = os.Remove(socketPath) // Clean up if exists
	addr, _ := net.ResolveUnixAddr("unixgram", socketPath)
	conn, err := net.ListenUnixgram("unixgram", addr)
	if err != nil {
		fmt.Printf("Error listening: %v\n", err)
		os.Exit(1)
	}
	// Important: Allow the sudo plugin (root) to write to the socket
	os.Chmod(socketPath, 0666)

	// 2. Initialize Table
	columns := []table.Column{
		{Title: "Event", Width: 10},
		{Title: "User", Width: 10},
		{Title: "Information", Width: 50},
	}
	t := table.New(
		table.WithColumns(columns),
		table.WithFocused(true),
		table.WithHeight(15),
	)

	s := table.DefaultStyles()
	s.Header = s.Header.
		BorderStyle(lipgloss.NormalBorder()).
		BorderForeground(lipgloss.Color("240")).
		BorderBottom(true).
		Bold(false)
	s.Selected = s.Selected.
		Foreground(lipgloss.Color("229")).
		Background(lipgloss.Color("57")).
		Bold(false)
	t.SetStyles(s)

	// 3. Start UI and Background Listener
	sub := make(chan auditMsg)
	go waitForActivity(sub, conn)

	p := tea.NewProgram(model{table: t, socket: conn})

	// Bridge the Go channel to BubbleTea updates
	go func() {
		for {
			m := <-sub
			p.Send(m)
		}
	}()

	if _, err := p.Run(); err != nil {
		fmt.Printf("UI Error: %v", err)
		os.Exit(1)
	}
}