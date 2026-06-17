#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int		i;
	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;
	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int		id[FD_SETSIZE], nxt, max, srv;
char	*buf[FD_SETSIZE];
fd_set	fds;

void	die() { write(2, "Fatal error\n", 12); exit(1); }

void	bc(int s, char *m)
{
	for (int i = srv + 1; i <= max; i++)
		if (FD_ISSET(i, &fds) && i != s)
			send(i, m, strlen(m), 0);
}

int main(int argc, char **argv)
{
	if (argc != 2) { write(2, "Wrong number of arguments\n", 26); exit(1); }

	int					fd, sockfd, connfd, n, len;
	char				tmp[4097], m[64], *msg, *out;
	struct sockaddr_in	servaddr, cli;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) die();
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port        = htons(atoi(argv[1]));
	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) die();
	if (listen(sockfd, 10) != 0) die();

	srv = max = sockfd;
	FD_ZERO(&fds); FD_SET(sockfd, &fds);
	while (1)
	{
		fd_set r = fds;
		if (select(max + 1, &r, 0, 0, 0) < 0) continue;
		for (fd = 0; fd <= max; fd++)
		{
			if (!FD_ISSET(fd, &r)) continue;
			if (fd == sockfd)
			{
				len = sizeof(cli);
				if ((connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t *)&len)) < 0) continue;
				FD_SET(connfd, &fds); if (connfd > max) max = connfd;
				id[connfd] = nxt++; buf[connfd] = 0;
				sprintf(m, "server: client %d just arrived\n", id[connfd]); bc(connfd, m);
			}
			else
			{
				n = recv(fd, tmp, 4096, 0); tmp[n > 0 ? n : 0] = 0;
				sprintf(m, n > 0 ? "client %d: " : "server: client %d just left\n", id[fd]);
				if (n <= 0) { bc(fd, m); FD_CLR(fd, &fds); free(buf[fd]); buf[fd] = 0; close(fd); continue; }
				if (!(buf[fd] = str_join(buf[fd], tmp))) die();
				while (extract_message(&buf[fd], &msg) == 1)
				{
					if (!(out = str_join(str_join(0, m), msg))) die();
					bc(fd, out); free(msg); free(out);
				}
			}
		}
	}
}
/*
Modifiche rispetto al main originale:

1. FIRMA
   int main() -> int main(int argc, char **argv)
   + controllo: if (argc != 2) { write(2, "Wrong number of arguments\n", 26); exit(1); }

2. ERRORI
   tutti i printf(...) + exit(0) -> die()
   le righe di successo ("Socket successfully created..") rimosse

3. PORTA
   htons(8081) -> htons(atoi(argv[1]))

4. VARIABILI DICHIARATE IN CIMA AL MAIN
   int fd, connfd, n, len;
   char tmp[4097], m[64], *msg, *out;
   riusate ad ogni iterazione senza riallocare nulla sullo stack

5. LOOP SELECT (sostituisce il singolo accept finale)
   srv = max = sockfd;
   FD_ZERO(&fds); FD_SET(sockfd, &fds);
   while (1) {
	   fd_set r = fds; if (select(max + 1, &r, 0, 0, 0) < 0) continue;
	   for (fd = 0; fd <= max; fd++) { ... }
   }

6. on_msg ELIMINATA
   la logica e' assorbita direttamente nel for loop del main.
   un solo sprintf gestisce sia disconnect che prefix con un ternario:
   sprintf(m, n > 0 ? "client %d: " : "server: client %d just left\n", id[fd]);

 */