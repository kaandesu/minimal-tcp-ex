package main

import (
	"context"
	"fmt"
	"log/slog"
	"net"
	"os"
	"os/signal"
	"syscall"
	"time"
)

type (
	Server struct {
		users    map[string]*User
		done     chan os.Signal
		msgch    chan Message
		listener net.Listener
		addr     string
	}
	Message struct {
		from    *User
		payload []byte
	}
	User struct {
		con  net.Conn
		addr string
	}
)

func NewServer(addr string) *Server {
	return &Server{
		addr:  addr,
		users: make(map[string]*User),
		msgch: make(chan Message),
		done:  make(chan os.Signal, 1),
	}
}

func (s *Server) Start() {
	ln, err := net.Listen("tcp", s.addr)
	if err != nil {
		slog.Error("Could not start the server", "ERR", err, "ADDR", s.addr)
	}
	s.listener = ln

	slog.Info("Starting the server on:", "ADDR", s.addr)
	go s.accept()
	go s.handleMessages()

	signal.Notify(s.done, os.Interrupt, syscall.SIGTERM, syscall.SIGABRT)
	<-s.done
	ctx, cancel := context.WithTimeout(context.Background(), time.Second*3)
	defer cancel()
	if err = s.shutdown(ctx); err != nil {
		slog.Error("Could not start the server", "ERR", err, "ADDR", s.addr)
	}
}

func (s *Server) shutdown(ctx context.Context) error {
	temp := make(chan struct{})
	go func() {
		defer s.listener.Close()
		for _, user := range s.users {
			if user.con != nil {
				user.con.Close()
			}
		}

		close(temp)
	}()

	select {
	case <-temp:
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}

func (s *Server) handleCon(con net.Conn) {
	user := &User{
		con:  con,
		addr: con.RemoteAddr().String(),
	}
	s.users[user.addr] = user
	defer func() {
		if user.con != nil {
			user.con.Close()
			slog.Info("User disconnected", "ADDR", user.addr)
		}
	}()
	buf := make([]byte, 1024)
	for {
		n, err := con.Read(buf)
		if err != nil {
			break
		}

		s.msgch <- Message{
			payload: buf[:n],
			from:    user,
		}

	}
}

func (s *Server) accept() {
	for {
		con, err := s.listener.Accept()
		if err != nil {
			break
		}
		go s.handleCon(con)
	}
}

func (s *Server) handleMessages() {
	for msg := range s.msgch {
		message := fmt.Sprintf(">>%s:%s", msg.from.addr, string(msg.payload))
		slog.Info("MSG", "RECV", message)
		if string(msg.payload) == "exit\n" {
			close(s.done)
		}
		for _, user := range s.users {
			if user.addr != msg.from.addr {
				fmt.Fprint(user.con, message)
			}
		}
	}
}

func main() {
	server := NewServer("127.1:3000")
	server.Start()
}
