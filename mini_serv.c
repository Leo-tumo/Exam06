#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>		// for sockaddr_in
#include <sys/socket.h>
#include <sys/select.h>

typedef struct client
{
	int				id;
	int				fd;
	struct client	*next;
}				t_client;


t_client *g_clients = NULL;
int sockfd, g_id = 0;
fd_set current, s_write, s_read;
char msg[60], str[4096 * 42], tmp[4096 * 42], buf[4096 * 42 + 60];

void fatal()
{
	write(2, "Fatal error\n", 12);
	close(sockfd);
	exit(1);
}

/*
**	@brief	
**	return client's id number
*/
int get_id(int fd)
{
	for (t_client *temp = g_clients; temp; temp = temp->next)
	{
		if (temp->fd == fd)
			return temp->id;
	}
	return -1;
}

 /*
 **	@brief	Get the max fd object
 **	
 **	@return	int max
 */
int get_max_fd()
{
	int max = sockfd;
	for (t_client *temp = g_clients; temp; temp = temp->next)
	{
		if (temp->fd > max)
			max = temp->fd;
	}
	return max;
}

/* sends the message to all clients except the one that 
is responsible for it (sends it, or connected / disconected) */ 

/*
**	@brief	
**	
**	@param	fd	fd of client that is responsible for the message
**	i.e. send it or connected/disconnected
**	@param	message		message to be sent
*/

void	send_all(int fd, char *message)
{
	for (t_client *temp = g_clients; temp; temp = temp->next)
	{
		if (temp->fd != fd && FD_ISSET(temp->fd, &s_write))
		{
			if (send(temp->fd, message, strlen(message), 0) < 0)
				fatal();
		}
	}
}

/*
**	@brief	
**	creates new node, initializes it
** 	and adds to our list
**	returns client's 'id' number	
*/
int add_client_to_list(int fd)
{
	t_client *new, *temp = g_clients;

	if (!(new = calloc(1, sizeof(t_client))))
		fatal();
	new->id = g_id++;
	new->fd = fd;
	new->next = NULL;

	if (!g_clients)
		g_clients = new;
	else
	{
		while(temp->next)
			temp = temp->next;
		temp->next = new;
	}
	return new->id;
}

/*
**	@brief	
**	accepts connection from the client
**	sends message to other clients that new client has arrived
**	with FD_SET adds new fd to our main fd_set – 'current'	
*/
void	add_client()
{
	int client_fd;
	struct sockaddr_in clientaddr;
	socklen_t	len = sizeof(clientaddr);

	if ((client_fd = accept(sockfd, (struct sockaddr*)& clientaddr, &len)) < 0)
		fatal();
	bzero(&msg, sizeof(msg));
	sprintf(msg, "server: client %d just arrived\n", add_client_to_list(client_fd));
	send_all(client_fd, msg);
	FD_SET(client_fd, &current);
}

/*
**	@brief	
**	
**	removes disconnected client from the
**	list and frees allocated memory,
**	return -> disconnected client's id
*/
int	rm_client_from_list(int fd)
{
	t_client *del, *temp = g_clients;
	int id = get_id(fd);

	if (temp->fd == fd)
	{
		g_clients = temp->next;
		free(temp);
	}
	else
	{
		while(temp && temp->next && temp->next->fd != fd)
			temp = temp->next;
		del = temp->next;
		temp->next = temp->next->next;
		free(del);
	}
	return id;
}

/*
**	@brief	
**	
**	removes the fd from our main fd_set – 'current'
**	closes the fd and tells other clients that he's left
*/
void	rm_client(int fd)
{
	bzero(&msg, sizeof(msg));
	sprintf(msg, "server: client %d just left\n", rm_client_from_list(fd));
	send_all(fd, msg);
	FD_CLR(fd, &current);
	close(fd);
}


/*
**	@brief	
**	
**	sends clients sent message to all other clients
**	1. we read the message in 'str' buffer (in main with "recv")
**	2. we copy the buffer in 'tmp' till each '\n'
**	3. we unite client's id with 'tmp' in 'buf'
**	and send it to others.
*/
void ex_msg(int fd)
{
	int i = 0, j = 0;
	while(str[i])
	{
		tmp[j] = str[i];
		++j;
		if (str[i] == '\n')
		{
			sprintf(buf, "client %d: %s", get_id(fd), tmp);
			send_all(fd, buf);
			j = 0;
			bzero(&tmp, sizeof(tmp));
			bzero(&buf, sizeof(buf));
		}
		++i;
	}
	bzero(&str, sizeof(str));
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	struct sockaddr_in servaddr;		// create and initialize sockaddr struct for ipv4
	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1 (2^24 * 127 + 1)
	servaddr.sin_port = htons(atoi(argv[1]));

	if ((sockfd = socket(2, 1, 0)) < 0) // create the socket
		fatal();
	if (bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) // bind it with the struct
		fatal();
	if (listen(sockfd, 0) < 0)		// start to listen to socket
		fatal();
	
	FD_ZERO(&current);				// first clear the fd_set from potential garbage values
	FD_SET(sockfd, &current);		// add sockfd to it
	bzero(&str, sizeof(str));
	bzero(&buf, sizeof(buf));
	bzero(&tmp, sizeof(tmp));

	while (23)
	{
		s_read = s_write = current;
		if (select(get_max_fd() + 1, &s_read, &s_write, NULL, NULL) < 0)	// if nothing changed, continue
			continue;
		for (int fd = 0; fd <= get_max_fd(); ++fd)
		{
			if (FD_ISSET(fd, &s_read))		// we are interested only in READ fds
			{
				if (fd == sockfd)			// if a new client wants to connect to sockfd
				{
					add_client();
					break;
				}
				else						// client shows activity
				{
					int str_req = 1000;
					while(str_req == 1000 || str[strlen(str) - 1] != '\n')
					{
						str_req = recv(fd, str + strlen(str), 1000, 0);
						if (str_req <= 0)
							break;
					}
					if (str_req <= 0)		// client disconnected
					{
						rm_client(fd);
						break;
					}
					else
						ex_msg(fd);			// client sent message
				}
			}
		}
	}
	return 0;
}


















































































































































