/** \file pvisualizer.c
*	\author Paolo Didier Alfano
*	Si dichiara che il contenuto di questo file e' in ogni sua parte opera
*	originale dell' autore.  
*/

#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "wator.c"

#define UNIX_PATH_MAX 108
#define SOCKNAME "./visualsock"
#define N 100
#define NWORK_DEF 2
#define CHRON_DEF 4

/*
volatile sig_atomic_t sit = 0;
*/
planet_t* planet;

/*
static void gestore (int signum) 
{
	if(signum == SIGCHLD)
	{
		sit = 1;
	}
}
*/

int main(int argc, char const *argv[])
{
	int righe;
	int colonne;
	int fd_skt, fd_c;
	char buf[N];
	struct sockaddr_un sa;
	char ack[2];

	int y = 1;
	int r;
	int j;

	char* riga;
	FILE* dumpfile;
	sigset_t set;

	/*
	struct sigaction s;


	memset(&s, 0, sizeof(s));	
	s.sa_handler=gestore;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGUSR1);
	if(pthread_sigmask(SIG_SETMASK,&set,NULL) < 0)
	{
		exit(1);
	}
	if((sigaction(SIGCHLD,&s,NULL)) == -1)
	{
		exit(1);
	}
	*/
	sigfillset(&set);
	if(pthread_sigmask(SIG_SETMASK,&set,NULL) < 0)
	{
		exit(1);
	}

	righe = atoi(argv[1]);
	colonne = atoi(argv[2]);

	planet = new_planet(righe, colonne);

	riga = (char*) malloc((colonne*2)*sizeof(char));

	sa.sun_family=AF_UNIX;
	unlink(SOCKNAME);
	strncpy(sa.sun_path, SOCKNAME,UNIX_PATH_MAX);

	fd_skt=socket(AF_UNIX,SOCK_STREAM,0);
	bind(fd_skt,(struct sockaddr *)&sa,sizeof(sa));
	listen(fd_skt,SOMAXCONN);

	ack[0] = '9';
	ack[1] = '\0';
	while(1)
	{
		while( (fd_c = accept(fd_skt,NULL,0)) == -1);
		r = read(fd_c, buf, sizeof(ack));
		switch(r)
		{
			case -1:
			{
				break;
			}
			case 0:
			{
				break;
			}
			default:
			{
				riga[0] = buf[0];
				riga[1] = '\0';
				while( (write(fd_c, ack,sizeof(ack))) == -1);
				break;
			}
		}



		if(riga[0] == '0')		/* Poiché mi è stato inviato zero sulla socket, c'è una matrice da leggere */
		{
			if(atoi(argv[3]) == 1)
			{
				dumpfile = fopen(argv[4], "w");
			}
			for(y=0; y<righe; ++y)
			{
				r = read(fd_c, buf, colonne+1);
				switch(r)
				{
					case -1:
					{
						break;
					}
					case 0:
					{
						break;
					}
					default:
					{
						for(j=0; j<colonne; ++j )
						{
							planet->w[y][j] = char_to_cell(buf[j]);
						}
						if(atoi(argv[3]) == 0)
						{
							print_planet(stdout, planet);					
						}

						while ( write(fd_c, ack,sizeof(ack)) == -1);
						break;
					}
				}
			}
			if(atoi(argv[3]) == 1)
			{
				print_planet(dumpfile, planet);
				fclose(dumpfile);
			}

			if (close(fd_c) == -1) return 1;
		}
		else		/* Poiché non mi è stato inviato zero sulla socket, non c'è nulla leggere, concludo. */
		{
			free(riga);
			if(close(fd_c) == -1) return 1;
			if(close(fd_skt) == -1) return 1;
			return(0);
		}
	}
	return 0;
}