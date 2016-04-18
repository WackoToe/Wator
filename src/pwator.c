/** \file pwator.c
*	\author Paolo Didier Alfano
*	Si dichiara che il contenuto di questo file e' in ogni sua parte opera
*	originale dell' autore.  
*/

#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/syscall.h>
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
#define CHRON_DEF 5

typedef struct _dispatcher
{
	int nwork;
	int nrighe;
	int ncolonne;
} dispatcher;

typedef struct _worker
{
	int number;
} worker;

typedef struct _collector
{
	FILE* dump;
	pthread_t* tid; 
	int nwork;
	int chronon;
} collector;


/*Elemento della lista*/
typedef struct _elem
{
	int key;
	struct _elem *next;
} elem;




wator_t* wator;						/* Variabile in cui salvo il pianeta letto da file */
pid_t pidv;							/* Variabile contenente il pid del visualizer */
int state = 1;						/* Variabile che indica di quale thread è il turno */

volatile sig_atomic_t sit = 0;		/* Variabile che viene settata a 1 se c'è stata un SIGINT o un SIGTERM */
volatile sig_atomic_t su = 0;		/* Variabile che viene settata a 1 se c'è stato un SIGUSR1 */
int last = 0;						/* Variabile che il collector segna ad 1 quando vede che sit è stata settata */

static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;		/* Lock che si acquisisce per gestire gli stati */
static pthread_cond_t d_cond = PTHREAD_COND_INITIALIZER;			/* Variabile di condizione sullo stato */
static pthread_cond_t w_cond = PTHREAD_COND_INITIALIZER;			/* Variabile di condizione sullo stato */
static pthread_cond_t c_cond = PTHREAD_COND_INITIALIZER;			/* Variabile di condizione sullo stato */
static pthread_mutex_t coda_lock = PTHREAD_MUTEX_INITIALIZER;		/* Lock per lavorare sulla coda dei task */
static pthread_mutex_t lock_lc = PTHREAD_MUTEX_INITIALIZER;			/* Lock per incrementare il numero di lavori completati */
static pthread_mutex_t main_lock_aqu = PTHREAD_MUTEX_INITIALIZER;	/* Lock che si acquisisce per poter prelevare le lock da mat_lock */
static pthread_mutex_t* mat_lock;	/* Matrice contenente le lock (una per ogni riga del pianeta) */
static pthread_mutex_t lock_last = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock_wf = PTHREAD_MUTEX_INITIALIZER;

elem* head;							/* Puntatore alla testa della lista dei task */
elem* tail;							/* Puntatore alla coda della lista dei task */
int lavori_completati = 0;
int wf = 0;

int** servito;						/* Matrice che segna gli elementi già valutati */




/* Funzione per inserire in coda ad una lista l'elemento "e" */
void inseriscicoda(elem **tail, elem **head,  int e)
{
	if(head==NULL) return;
	if(*tail==NULL)
	{
		*head=malloc(sizeof(elem));
		(*head)->key=e;
		(*head)->next=NULL;
		*tail = *head;
	}
	else
	{
		elem *t=*tail;
		/* qui t punta all'ultima struttura della lista: ne
		creo una nuova e sposto il puntatore in avanti */
		t->next=malloc(sizeof(elem));
		t=t->next;
		/* metto i valori nell'ultima struttura */
		t->key=e;
		t->next=NULL;
		*tail = t;
	}
}

/* 	Questa funzione estrae il primo elemento da una coda. Restituisce la chiave associata	
	all'elemento che estrae. Nel caso non vi siano elementi nella coda, restituisce -1 */
int estrai(elem** tail,elem** head)
{
	int val_key;
	elem *t = *head;
	if(*head == NULL)
	{
		return -1;
	}
	if((*head)->next == NULL)
	{
		val_key = (*head)->key;
		*head = NULL;
		*tail = NULL;
		free(t);
		return val_key;
	}
	else
	{
		val_key = (*head)->key;
		*head = (*head)->next;
		free(t);
		return val_key;
	}
}

int update_riga (wator_t * pw, int row_num)
{
	int _ncol = pw->plan->ncol;
	int j, k, l;

	if(pw == NULL)
	{
		errno = EINVAL;
		return -1;		
	}

	/*Comincio ad aggiornare la matrice w*/
	for(j=0; j<_ncol; ++j)
	{
		if(servito[row_num][j] == 0)
		{
			switch(pw->plan->w[row_num][j])
			{
				case SHARK:
				{
					shark_rule2(pw, row_num, j, &k, &l);
					servito[k][l] = 1;
					shark_rule1(pw, row_num, j, &k, &l);
					servito[k][l] = 1;
					servito[row_num][j] = 1;
					break;
				}
				case FISH:
				{
					fish_rule4(pw, row_num, j, &k, &l);
					servito[k][l] = 1;
					fish_rule3(pw, row_num, j, &k, &l);
					servito[k][l] = 1;
					servito[row_num][j] = 1;
					break;
				}
				case WATER:
				{
					servito[row_num][j] = 1;
					break;
				}
			}
		}
	}
	return 0;
}

/*  Il gestore quando riceve un'interruzione setta la variabile globale corrispondente al segnale */
static void gestore (int signum) 
{
	if( (signum == SIGINT) || (signum == SIGTERM) )
	{
		sit = 1;
		fprintf(stderr, "Ho messo sit\n");
	}
	if(signum == SIGUSR1)
	{
		su = 1;
	}
}


void* disp(void* _d)
{
	dispatcher* d;
	sigset_t set;
	int i, j;

	sigfillset(&set);			/* Maschero tutti i segnali in modo che solo il processo wator li gestisca */
	if(pthread_sigmask(SIG_SETMASK,&set,NULL) < 0)
	{
		exit(1);
	}

	d = (dispatcher*) _d;

	while(1)
	{
		fprintf(stderr, "Ho iniziato dispatcher\n");
		pthread_mutex_lock(&state_lock);
		while(state != 1)
		{
			pthread_cond_wait(&d_cond, &state_lock);
		}
		pthread_mutex_unlock(&state_lock);
		fprintf(stderr, "Qui ci arrivo ispatcher\n");

		if(last==1)
		{
			fprintf(stderr, "TERMINAZIONE DISPATCHER\n");
			pthread_mutex_lock(&state_lock);
			state = 2;
			pthread_cond_broadcast(&w_cond);
			pthread_mutex_unlock(&state_lock);
			return (void*) 0;
		}

		head = NULL;
		tail = NULL;
		for(i=0; i< (d->nrighe); ++i)
		{
			for(j=0; j<(d->ncolonne); ++j) servito[i][j] = 0;			/* Azzero la matrice servito */
		}


		for(i=0; i< (d->nrighe); i+=3)
		{
			inseriscicoda(&tail, &head, i);
		}
		for(i=1; i< (d->nrighe); i+=3)
		{
			inseriscicoda(&tail, &head, i);
		}
		for(i=2; i< (d->nrighe); i+=3)
		{
			inseriscicoda(&tail, &head, i);
		}	
		lavori_completati = 0;
		state = 2;
		fprintf(stderr, "Ho finito dispatcher\n");
		pthread_mutex_lock(&state_lock);
		pthread_cond_broadcast(&w_cond);
		pthread_mutex_unlock(&state_lock);
	}
	return (void*) 0;
}

void* work(void* w)
{
	FILE* pfile;
	char num_worker[32];
	char* nome;
	worker* tw;
	sigset_t set;

	int num_riga;


	sigfillset(&set);					/* Maschero tutti i segnali in modo che solo il processo wator li gestisca */
	if(pthread_sigmask(SIG_SETMASK,&set,NULL) < 0)
	{
		exit(1);
	}

	nome = (char*) malloc(32*sizeof(char));
	tw = (worker*)w;
	nome = strcpy(nome, "wator_worker_");
	sprintf(num_worker, "%d", tw->number);
	nome = strcat(nome, num_worker);
	pfile = fopen(nome, "w");
	
	while(1)
	{
		fprintf(stderr, "Ho iniziato worker %d\n", last);
		if(last==1)
		{
			fprintf(stderr, "TERMINAZIONE WORKER %d\n",tw->number);
			fprintf(stderr, "state: %d\n", state);
			free(nome);
			fclose(pfile);
			pthread_cond_broadcast(&w_cond);
			pthread_mutex_lock(&lock_wf);
			++wf;
			pthread_mutex_unlock(&lock_wf);
			return (void*) 0;
		}
		
		pthread_mutex_lock(&state_lock);
		//fprintf(stderr, "Qui ci arrivo worker %d\n", tw->number);
		while(state != 2)
		{
			pthread_cond_wait(&w_cond, &state_lock);
		}
		pthread_mutex_unlock(&state_lock);

		if(last==0)
		{
			pthread_mutex_lock(&coda_lock);
			num_riga = estrai(&tail, &head);
			pthread_mutex_unlock(&coda_lock);
			if(num_riga != -1)
			{
				pthread_mutex_lock(&lock_lc);
				++lavori_completati;
				/*fprintf(stderr, "Lavori completati: %d\n", lavori_completati);*/
				pthread_mutex_unlock(&lock_lc);

				switch(wator->plan->nrow)
				{
					case 1:
					{
						pthread_mutex_lock(&main_lock_aqu);
						pthread_mutex_lock(&mat_lock[num_riga]);
						pthread_mutex_unlock(&main_lock_aqu);

						update_riga(wator, num_riga);

						pthread_mutex_unlock(&mat_lock[num_riga]);
						break;
					}
					case 2:
					{
						pthread_mutex_lock(&main_lock_aqu);
						pthread_mutex_lock(&mat_lock[num_riga]);
						pthread_mutex_lock(&mat_lock[ ((num_riga-1)+ (wator->plan->nrow))%(wator->plan->nrow)]);
						pthread_mutex_unlock(&main_lock_aqu);

						update_riga(wator, num_riga);

						pthread_mutex_unlock(&mat_lock[num_riga]);
						pthread_mutex_unlock(&mat_lock[ ((num_riga-1)+ (wator->plan->nrow))%(wator->plan->nrow)]);
						break;
					}
					default:
					{
						pthread_mutex_lock(&main_lock_aqu);
						/* Acquisisco la lock della riga da aggiornare */
						pthread_mutex_lock(&mat_lock[num_riga]);
						/* Acquisisco la lock della riga sopra a quella che devo aggiornare */
						pthread_mutex_lock(&mat_lock[ ((num_riga-1)+ (wator->plan->nrow))%(wator->plan->nrow)]);
						/* Acquisisco la lock della riga sotto a quella che devo aggiornare */
						pthread_mutex_lock(&mat_lock[ ((num_riga+1)+ (wator->plan->nrow))%(wator->plan->nrow)]);
						pthread_mutex_unlock(&main_lock_aqu);

						update_riga(wator, num_riga);

						pthread_mutex_unlock(&mat_lock[num_riga]);
						pthread_mutex_unlock(&mat_lock[ ((num_riga-1)+ (wator->plan->nrow))%(wator->plan->nrow)]);
						pthread_mutex_unlock(&mat_lock[ ((num_riga+1)+ (wator->plan->nrow))%(wator->plan->nrow)]);
					}
				}

				pthread_mutex_lock(&state_lock);
				if( (lavori_completati == wator->plan->nrow) && (last==0) )
				{
					state = 3;
					pthread_cond_broadcast(&c_cond);
				}
				pthread_mutex_unlock(&state_lock);
			}
		}
	}
	return (void*) 0;
}

void* coll(void* _c)
{
	int i;
	int j;
	collector* c;
	int fd_skt;
	char buf[N];
	struct sockaddr_un sa;
	int w;
	int timer;
	char *riga;
	char scelta[2];
	int r;
	FILE* wa_check;
	sigset_t set;


	sigfillset(&set);			/* Maschero tutti i segnali in modo che solo il processo wator li gestisca */
	if(pthread_sigmask(SIG_SETMASK,&set,NULL) < 0)
	{
		exit(1);
	}

	riga = (char*) malloc(((wator->plan->ncol)+1)*sizeof(char));
	strncpy(sa.sun_path, SOCKNAME,UNIX_PATH_MAX);
	sa.sun_family=AF_UNIX;

	c = (collector*) _c;
	/*
	for(i=0;i<(c->nwork); ++i)
	{
		pthread_join(c->tid[i], NULL);
	}
	*/

	timer = 0;
	while(1)
	{
		/* sleep(1); */
		pthread_mutex_lock(&state_lock);
		while(state != 3)
		{
			pthread_cond_wait(&c_cond, &state_lock);
		}
		pthread_mutex_unlock(&state_lock);

		if(su)		/* Se "su" è settata allora devo fare una scrittura sul file wator.check */
		{
			if( (wa_check = fopen("wator.check", "w")) == NULL) return (void*) 1;
			print_planet(wa_check, wator->plan);
			fclose(wa_check);
			su = 0;
		}

		fd_skt=socket(AF_UNIX,SOCK_STREAM,0);
		while (connect(fd_skt,(struct sockaddr*)&sa,sizeof(sa)) == -1 )
		{
			if ( errno == ENOENT )
			{
			fprintf(stderr, "ENOENT\n" );
			sleep(1);
			}										/*sock non esiste*/
			else sleep(1);
		}
		/*
		* PROTOCOLLO:
		* Il thread collector invia un valore diverso sulla socket a seconda che
		* la variabile "sit" (legata a SIGINT e SIGTERM) sia stata settata. Nel
		* caso "sit" sia uguale a 1 invio 1 al visualizer e aspetto la sua
		* risposta. Se invece "sit" è uguale a 0 allora è tutto ok, dunque 
		* viene mandato 0 sulla socket e subito dopo viene mandata anche la
		* matrice. Ogni volta che collector invia qualcosa, aspetta un ack da
		* parte del visualizer per proseguire.
		*/
		if(sit)
		{
			scelta[0] = '1';
			scelta[1] = '\0';
			w = write(fd_skt,scelta, strlen(scelta)+1);
			while( (r = read(fd_skt, buf, strlen(scelta)+1)) != 2);
			free(riga);

			last = 1;
			pthread_mutex_lock(&state_lock);
			state = 1;
			pthread_mutex_unlock(&state_lock);
			pthread_cond_broadcast(&d_cond);
			//pthread_cond_broadcast(&w_cond);
			return (void*) 0;
		}
		else
		{
			if( (timer%(c->chronon)) == 0)	/* Se è il chronon giusto, invio sulla socket */
			{
				scelta[0] = '0';
				scelta[1] = '\0';
				w = write(fd_skt,scelta, strlen(scelta)+1);
				while( (r = read(fd_skt, buf, strlen(scelta)+1)) != 2);
			
				for(i=0; i<(wator->plan->nrow); ++i)
				{
					for(j=0; j<(wator->plan->ncol); ++j)
					{
						riga[j] = cell_to_char(wator->plan->w[i][j]);
					}
					riga[j] = '\0';
					if((w = write(fd_skt,riga, strlen(riga)+1) != -1));
					while( (r = read(fd_skt, buf, sizeof(scelta))) == -1);
				}
			}
			
		}
		close(fd_skt);
		++timer;

		pthread_mutex_lock(&state_lock);
		state = 1;
		pthread_cond_broadcast(&d_cond);
		pthread_mutex_unlock(&state_lock);
	}
	return (void*) 0;
}





int main(int argc, char const *argv[]) 
{
	int i;								/* Variabile di scorrimento */
	FILE* pianeta;						/* Puntatore al pianeta da leggere */
	int tp;								/* Variabile che segnala se il file contenente il pianeta è stato trovato */
	int pos_input;						/* Variabile che segnala la posizione del file da leggere negli argomenti */
	FILE* check;						/* Puntatore che controlla se un parametro è un file */
	int cnf;							/* Contatore del numero di file apribili passati da riga di comando */


	int n;								/* Numero di worker */
	int c;								/* Numero di chronon*/
	FILE* d;							/* Puntatore al file su cui dumpare i risultati*/

	pthread_t dtid;						/* Dispatcher tid */
	dispatcher dis;						/* Il dispatcher */
	pthread_t* ar_tid;					/* Array contenente tutti i tid dei worker */
	int check_create;					/* Variabile di controllo sulle creazioni dei thread */
	worker* arr_worker;					/* Array contenente tutti i dati dei worker */
	collector collettore;				/* Variabile contenente i dati del collettore */
	pthread_t ctid;						/* Collector tid */

	char* righe;
	char* colonne;
	int pfd;							/* Variabile contenente la posizione del file di dump */

	struct sigaction s;

	memset(&s, 0, sizeof(s));
	s.sa_handler=gestore;
	if((sigaction(SIGINT,&s,NULL)) == -1)
	{
		exit(1);
	}
	if((sigaction(SIGTERM,&s,NULL)) == -1)
	{
		exit(1);
	}
	if((sigaction(SIGUSR1,&s,NULL)) == -1)
	{
		exit(1);
	}

	
	n = NWORK_DEF;
	c = CHRON_DEF;
	d = NULL;
	pfd = 0;
	/*Inizio il controllo dei parametri*/
	if(argc <2 ) exit(1);
	tp = 0;
	if( argc%2 != 0 ) exit(1);								/* Se il numero degli argomenti non è pari, esco */
	cnf = 0;
	for(i=1; i<argc; ++i)
	{
		if( (check =fopen(argv[i], "r")) != NULL)
		{
			++cnf;											/* Controllo quanti file apribili ci sono */
			fclose(check);		
		}
	}
	if( (cnf<1) ||(cnf > 2) )exit(1);						/* Se ci sono più di due file apribili, esco */
	
	for(i=1; i<argc; ++i)									/* Vado a cercare il pianeta da leggere */
	{
		if(tp==0)											/* Se non l'ho ancora trovato, continuo a cercare */
		{
			if( (pianeta = fopen(argv[i], "r")) != NULL)
			{
				if( (strcmp(argv[i-1], "-f") != 0) )
				{
					tp = 1;									/* Ho trovato il file da leggere */
					pos_input = i;
				}
				 
			}
		}
	}
	if(tp==0) exit(1);									/* Se non ho trovato un pianeta da leggere, esco */

	for(i=1; i<argc; ++i)								/* Controllo tutti gli argomenti e vedo cosa contengono*/
	{
		if(strcmp(argv[i], "-n") == 0 )
		{
			if(i+1>=argc) exit(1);
			else if ( (n = atoi(argv[i+1])) == 0) exit(1);
		}
		if(strcmp(argv[i], "-v") == 0 )
		{
			if(i+1>=argc) exit(1);
			else if ( (c = atoi(argv[i+1])) == 0) exit(1);
		}
		if(strcmp(argv[i], "-f") == 0 )							/*Se l'argomento i-esimo è -f controllo il successivo*/
		{
			if(i+1>=argc) exit(1);								/*Se l'argomento successivo a -f non esiste, esco*/
			else
			{
				if ( (d = fopen(argv[i+1], "w")) == NULL )		/*Se l'argomento successivo a -f non è apribile, esco*/
				{
						exit(1);
				}
				else
				{
						pfd = i+1;						/*Altrimenti incremento la posizione del file di dump*/	
						fclose(d);	
				}				
			} 	

		}
	}



	wator = new_wator((char*)argv[pos_input]);

	/*Alloco la matrice servito*/
	servito = (int**) malloc((wator->plan->nrow)*sizeof(int*));
	for(i=0; i<(wator->plan->nrow); ++i)
	{
		servito[i] = (int*) malloc((wator->plan->ncol)*sizeof(int));
	}

	/* Alloco la matrice di lock */
	mat_lock = (pthread_mutex_t*)malloc((wator->plan->nrow)*sizeof(pthread_mutex_t));
	for(i=0; i<(wator->plan->nrow); ++i)
	{
		pthread_mutex_init(&mat_lock[i], NULL);
	}

	if ((pidv = fork()) !=0) 
	{																/*padre, server*/
		ar_tid = (pthread_t*) malloc(n*sizeof(pthread_t));
		arr_worker = (worker*) malloc(n*sizeof(worker));

		/* Setto i valori iniziali per dispatcher e collector */
		dis.nwork = n;
		dis.nrighe = wator->plan->nrow;
		dis.ncolonne = wator->plan->ncol;
		collettore.dump = d;
		collettore.tid = (pthread_t*) malloc(n*sizeof(pthread_t));
		collettore.nwork = n;
		collettore.chronon = c;

		/* Creo il thread dispatcher*/
		if( (check_create = pthread_create(&dtid, NULL, &disp, (void*) &dis)) != 0)
		{
			fclose(pianeta);
			free(ar_tid);
			free(arr_worker);
			free(collettore.tid);
			free_wator(wator);
			exit(1);			
		}



		/* Creo i thread worker */
		for(i=0; i<n; ++i)
		{
			arr_worker[i].number = i;
			if( (check_create = pthread_create(&ar_tid[i], NULL, &work, (void*) &arr_worker[i])) != 0 )
			{
				fclose(pianeta);
				free(ar_tid);
				free(arr_worker);
				free(collettore.tid);
				free_wator(wator);
				exit(1);
			}

			collettore.tid[i] = ar_tid[i];
		}

		/* Creo il thread collector */
		if( (check_create = pthread_create(&ctid,NULL,&coll,(void*) &collettore)) != 0)
		{
			fclose(pianeta);
			free(ar_tid);
			free(arr_worker);
			free(collettore.tid);
			free_wator(wator);
			exit(1);
		}

		pthread_join(ctid,NULL);						/* Aspetto il collector */
		pthread_join(dtid, NULL);						/* Aspetto il dispatcher */
		/*
		for(i=0; i<n; ++i)
		{
			pthread_join(ar_tid[i], NULL);
		}
		*/
		while(wf<n);
		fprintf(stderr, "Ho aspettato i workers\n");
		fclose(pianeta);
		free(ar_tid);
		free(arr_worker);
		free(collettore.tid);
		free(mat_lock);
		free(head);
		free(tail);
		for(i=0; i<(wator->plan->nrow); ++i)
		{
			free(servito[i]);				
		}
		free(servito);
		free_wator(wator);
		return 0;
	}
	else
	{
		righe = (char*) malloc (16*sizeof(char));
		colonne = (char*) malloc (16*sizeof(char));
		sprintf(righe, "%d", wator->plan->nrow);
		sprintf(colonne, "%d", wator->plan->ncol);
		/* 
		* Se esiste un file su cui dumpare passo il valore 1 come argomento al visualizer
		* e passo il nome di tale file sempre come argomento al visualizer. Nel caso non
		* ci sia un file su cui fare dump, passo 0 come argomento al visualizer.
		*/
		if(d != NULL)							
		{
			execl("./visualizer","visualizer", righe, colonne, "1", argv[pfd], NULL);
		}
		else
		{
			execl("./visualizer","visualizer", righe, colonne, "0", NULL);
		}
		free(righe);
		free(colonne);
		return 0;
	}
}