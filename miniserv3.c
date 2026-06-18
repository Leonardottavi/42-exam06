#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int		maxfd;
int		ids[65536];
char	*bufs[65536];
int		next_id;

void	send_all(int except, char *msg, int len)
{
	for (int fd = 0; fd <= maxfd; fd++)
		if (ids[fd] >= 0 && fd != except)
			send(fd, msg, len, 0);
}

void	fatal(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	int srv = socket(AF_INET, SOCK_STREAM, 0);
	if (srv < 0)
		fatal();

	struct sockaddr_in sa;
	bzero(&sa, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(0x7f000001);
	sa.sin_port = htons(atoi(argv[1]));

	if (bind(srv, (struct sockaddr *)&sa, sizeof(sa)) < 0)
		fatal();
	if (listen(srv, 128) < 0)
		fatal();

	fd_set master, sel;
	FD_ZERO(&master);
	FD_SET(srv, &master);
	maxfd = srv;
	memset(ids, -1, sizeof(ids));

	char buf[65536];
	char msg[131072];

	while (1)
	{
		sel = master;
		if (select(maxfd + 1, &sel, 0, 0, 0) < 0)
			continue;
		for (int fd = 0; fd <= maxfd; fd++)
		{
			if (!FD_ISSET(fd, &sel))
				continue;
			if (fd == srv)
			{
				int cli = accept(srv, 0, 0);
				if (cli < 0)
					continue;
				if (cli > maxfd)
					maxfd = cli;
				FD_SET(cli, &master);
				ids[cli] = next_id++;
				bufs[cli] = NULL;
				sprintf(msg, "server: client %d just arrived\n", ids[cli]);
				send_all(cli, msg, strlen(msg));
			}
			else
			{
				int n = recv(fd, buf, sizeof(buf) - 1, 0);
				if (n <= 0)
				{
					sprintf(msg, "server: client %d just left\n", ids[fd]);
					send_all(fd, msg, strlen(msg));
					free(bufs[fd]);
					bufs[fd] = NULL;
					ids[fd] = -1;
					FD_CLR(fd, &master);
					close(fd);
				}
				else
				{
					buf[n] = 0;
					int old = bufs[fd] ? strlen(bufs[fd]) : 0;
					char *tmp = realloc(bufs[fd], old + n + 1);
					if (!tmp)
						fatal();
					bufs[fd] = tmp;
					strcpy(bufs[fd] + old, buf);
					char *p = bufs[fd], *nl;
					while ((nl = strstr(p, "\n")))
					{
						*nl = 0;
						sprintf(msg, "client %d: %s\n", ids[fd], p);
						send_all(fd, msg, strlen(msg));
						p = nl + 1;
					}
					int rem = strlen(p);
					char *newbuf = malloc(rem + 1);
					if (!newbuf)
						fatal();
					strcpy(newbuf, p);
					free(bufs[fd]);
					bufs[fd] = newbuf;
				}
			}
		}
	}
}