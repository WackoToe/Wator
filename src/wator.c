/* \file wator.c
\author Paolo Didier Alfano
Si dichiara che il contenuto di questo file è in ogni sua parte opera
originale dell'autore.  */

#include <stdio.h>
#include <stdlib.h>
#include <mcheck.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "wator.h"

char cell_to_char (cell_t a)
{
	if(a == SHARK) return 'S';
	if(a == FISH) return 'F';
	if(a == WATER) return 'W';
	else return '?';
}

int char_to_cell (char c)
{
	if (c == 'S') return SHARK;
	if (c == 'F') return FISH;
	if (c == 'W') return WATER;
	else return -1;
}

planet_t * new_planet (unsigned int nrow, unsigned int ncol)
{
	int i;
	int j;
	int check_col;
	planet_t* new_p;

	if((nrow<=0) || (ncol<=0))
	{
		errno=EINVAL;
		return NULL;
	}

	new_p = (planet_t*)malloc(sizeof(planet_t));
	if(new_p == NULL)
	{
		printf("C'è stato un errore di allocazione del puntatore\n");
		errno = ENOMEM;
		return NULL;
	}

	new_p->nrow = nrow;
	new_p->ncol = ncol;

	/*Allocazione delle righe*/
	new_p->w = (cell_t**) malloc(nrow*sizeof(cell_t*));
	new_p->btime = (int**) malloc(nrow*sizeof(int*));
	new_p->dtime = (int**) malloc(nrow*sizeof(int*));

	/*Se anche una sola allocazione non è andata a buon fine, dealloco e interrompo*/
	if ( (new_p->w == NULL) || (new_p->btime == NULL) || (new_p->dtime == NULL) )
	{
		free(new_p->w);
		free(new_p->btime);
		free(new_p->dtime);
		free(new_p);
		errno = ENOMEM;
		return NULL;
	}

	/*Allocazione delle colonne*/
	for(i=0; i<nrow; ++i)
	{
		new_p->w[i] = (cell_t*) malloc(ncol*sizeof(cell_t));	
		new_p->btime[i]=(int*) malloc(ncol*sizeof(int));
		new_p->dtime[i]=(int*) malloc(ncol*sizeof(int));
		
		/*Se anche una sola allocazione non è andata a buon fine, dealloco e interrompo*/
		if( (new_p->w[i] == NULL) || (new_p->btime[i] == NULL) || (new_p->dtime[i] == NULL) )
		{
			for(check_col=0; check_col<i; ++check_col)
			{
				free(new_p->w[check_col]);
				free(new_p->btime[check_col]);
				free(new_p->dtime[check_col]);
			}
			free(new_p->w);
			free(new_p->btime);
			free(new_p->dtime);
			free(new_p);
			errno = ENOMEM;
			return NULL;
		}
		for(j=0; j<ncol; ++j)
		{
			new_p->w[i][j] = char_to_cell ('W');
			new_p->btime[i][j]=0;
			new_p->dtime[i][j]=0;	
		}
	}
	return new_p;
}

void free_planet (planet_t* p)
{
	int i;
	unsigned int _nrow = p->nrow;

	if(p == NULL)
	{
		return;
	}
	for(i=0; i<_nrow; ++i)
	{
		free(p->w[i]);
		free(p->btime[i]);
		free(p->dtime[i]);
	}
	free(p->w);
	free(p->btime);
	free(p->dtime);
	free(p);
}

int print_planet (FILE* f, planet_t* p)
{
	int i,j;

	if((f==NULL) || (p==NULL))
	{
		errno = EINVAL;
		return -1;
	}
	
	fprintf(f, "%d\n", p->nrow);
	fprintf(f, "%d\n", p->ncol);
	for(i=0; i<p->nrow; ++i)
	{
		for(j=0; j<p->ncol; ++j)
		{
			if(j != (p->ncol-1))		/*Se non siamo all'ultimo stampa lo spazio dopo il carattere*/
			{
				fprintf(f, "%c ", cell_to_char( (p->w)[i][j]) );
			}
			else						/*Se siamo all'ultimo NON stampa lo spazio dopo il carattere*/
			{
				fprintf(f, "%c", cell_to_char((p->w)[i][j]) );
			}
		}
		fprintf(f, "\n");
	}
	return 0;
}

planet_t* load_planet (FILE* f)
{
	unsigned int _nrow, _ncol;
	int i,j;
	char righe[5];
	char colonne[5];
	char* controllo;						/*Puntatore a carattere che controlla il formato*/
	char* temp;								/*Stringa dove salvo riga per riga il file di input*/
	planet_t* pian_car;

	if(f == NULL) 
	{
		errno = EINVAL;
		return NULL;
	}

	fgets(righe, 5, f);						/*Acquisisco la prima riga (contiene nrow)*/
	_nrow = strtol(righe, NULL, 10);
	if(_nrow<1)
	{
		errno=EINVAL;
		return NULL;
	}

	fgets(colonne, 5, f);					/*Acquisisco la seconda riga (contiene ncol)*/
	_ncol = strtol(colonne, NULL, 10);
	if(_ncol<1)
	{
		errno=EINVAL;
		return NULL;
	}

	pian_car = new_planet(_nrow, _ncol);	/*Ottenuti nrow e ncol, vado a creare il pianeta*/
	if(pian_car == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}

	temp = malloc (((2*_ncol)+1)*sizeof(char));
	if(temp == NULL)
	{
		free_planet(pian_car);
		errno = ENOMEM;
		return NULL;
	}
	for(i=0; i<_nrow; ++i)
	{
		fgets(temp, (2*_ncol)+1, f);
		if(strlen(temp) != (2*_ncol) )
		{
			free_planet(pian_car);
			errno = ERANGE;
			return NULL;
		}

		/*Pongo sul primo carattere di temp un puntatore che farò scorrere per controllare il formato*/
		controllo=&temp[0];

		for(j=0; j<(_ncol-1); ++j)
		{
			/*Se il valore puntato da controllo è W, S, F allora lo inserisco nel pianeta*/
			if (char_to_cell(*controllo)!=-1)
			{
				pian_car->w[i][j] = char_to_cell(*controllo);
			}
			else	/*Altrimenti dealloco e interrompo*/
			{
				free_planet(pian_car);
				errno = ERANGE;
				return NULL;
			}
			++controllo;

			/*Se il valore puntato da controllo non è uno spazio, dealloco e interrompo*/
			if((*controllo)!= ' ')
			{
				free_planet(pian_car);
				errno = ERANGE;
				return NULL;
			}
			++controllo;
		}

		if (char_to_cell(*controllo)!=-1)			/*Controllo l'ultima posizione*/
		{
			pian_car->w[i][j] = char_to_cell(*controllo);
		}
		else
		{
			free_planet(pian_car);
			errno = ERANGE;
			return NULL;
		}
	}
	free(temp);
	return pian_car;
}

wator_t* new_wator (char* fileplan)
{
	FILE* p_file;
	FILE* conf;
	char* control;						/*Puntatore a carattere che controlla il formato*/
	int scor;
	int i=0;
	int k;
	char riga_intera[16];
	char temp[8];
	/*Utilizzo tre variabili per segnalare se, rispettivamente sb,sd,fb sono gia' stati letti*/
	int p_sb=0;
	int p_sd=0;
	int p_fb=0;
	wator_t* nuovo_wator;
	srand(time(NULL));
	nuovo_wator= (wator_t*)malloc(sizeof(wator_t));

	if(nuovo_wator == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}

	conf = fopen("wator.conf", "r");

	/*Eseguo tre volte un ciclo per caricare le prime tre righe di un file. Se non 
	contengono esattamente sd, sb, fb (in un ordine qualunque) interromperò*/
	for(k=0; k<3; ++k)
	{
		for(scor=0; scor<16; ++scor) riga_intera[scor] = '\0';
		for(scor=0; scor<8; ++scor) temp[scor] = '\0';
		if(fgets(riga_intera, 16, conf) == NULL)
		{
			free(nuovo_wator);
			errno = EINVAL;
			return NULL;
		}
		i=0;

		control=&riga_intera[0];
		/*Copio i primi due caratteri di "riga_intera" in "temp"*/
		temp[i] = *control;
		++control;
		++i;
		temp[i] = *control;
		control+=2;
		/*Se la riga non contiene né sb, né sd, né fb allora non è valida dunque dealloco e termino*/
		if( (strcmp(temp, "sb")!=0) && (strcmp(temp, "sd")!=0) && (strcmp(temp, "fb")!=0) )
		{
			free(nuovo_wator);
			errno = EINVAL;
			return NULL;
		}
		else
		{
			i=0;

			if(strcmp(temp,"sb")==0)							/*Nella k-esima riga avevo sb*/
			{
				for(scor=0; scor<8; ++scor) temp[scor] = '\0';
				p_sb = 1;										/*Segnalo di aver letto sb*/
				while((*control) != '\n')
				{	
					temp[i]=*control;
					++i;
					++control;
				}	
				nuovo_wator->sb=atoi(temp);
			}
			if(strcmp(temp,"sd")==0)							/*Nella k-esima riga avevo sd*/
			{
				for(scor=0; scor<8; ++scor) temp[scor] = '\0';
				p_sd = 1;										/*Segnalo di aver letto sd*/
				while((*control) != '\n')
				{	
					temp[i]=*control;
					++i;
					++control;
				}	
				nuovo_wator->sd=atoi(temp);
			}	
			if(strcmp(temp,"fb")==0)							/*Nella k-esima riga avevo fb*/
			{
				for(scor=0; scor<8; ++scor) temp[scor] = '\0';
				p_fb = 1;										/*Segnalo di aver letto fb*/
				while((*control) != '\n')
				{	
					temp[i]=*control;
					++i;
					++control;
				}	
				nuovo_wator->fb=atoi(temp);
			}
		}
	}

	/*Se anche uno solo dei tre segnalatori non è stato  
	settato allora il file wator era formattato male*/
	if( (p_sb!=1) || (p_sd!=1) || (p_fb!=1) )
	{
		free(nuovo_wator);
		errno = ERANGE;
		return NULL;
	}

	p_file = fopen(fileplan, "r");

	nuovo_wator->plan = load_planet(p_file);
	if( (nuovo_wator->plan) == NULL) 
	{
		free(nuovo_wator);
		return NULL;
	}
	nuovo_wator->nf = fish_count(nuovo_wator->plan);
	nuovo_wator->ns = shark_count(nuovo_wator->plan);
	nuovo_wator->nwork = 0;
	nuovo_wator->chronon = 0;

	fclose(conf);
	fclose(p_file);
	return nuovo_wator;
}

void free_wator(wator_t* pw)
{
	free_planet(pw->plan);
	free(pw);
}

int shark_rule1 (wator_t* pw, int x, int y, int *k, int* l)
{
	int _nrow = pw->plan->nrow;
	int _ncol = pw->plan->ncol;
	int ind_rand = rand()%4;		/*comincerò a controllare l'array att[] a partire da questo indice*/
	int contatore = 0;				/*serve per non controllare più di 3 posizioni di att[]*/
	cell_t att[4];

	if(pw == NULL)
	{
		errno = EINVAL;
		return -1;		
	}
	if( (pw->plan->w[x][y]) != SHARK) 				/*Controllo che nella cella [x][y] ci sia uno squalo*/
	{
		errno = EINVAL;
		return -1;
	}

	/*	array che ospita il contenuto delle celle ATTorno a [x][y] costruito come segue:
		att[0] contiene l'elemento a sinistra di [x][y], ovvero [x][y-1]
		att[1] contiene l'elemento sopra a [x][y], ovvero [x-1][y]
		att[2] contiene l'elemento a destra di [x][y], ovvero [x][y+1]
		att[3] contiene l'elemento sotto a [x][y], ovvero [x+1][y]
		Graficamente:

				att[1]
		att[0]	[x][y]	att[2]
				att[3]
	*/	
	att[0] = pw->plan->w[x][((y-1) + _ncol) % _ncol];
	att[1] = pw->plan->w[((x-1) + _nrow) % _nrow][y];
	att[2] = pw->plan->w[x][(y+1) %_ncol];
	att[3] = pw->plan->w[(x+1)%_nrow ][y];

	/*Lo squalo resta fermo perché intorno ha solo altri squali*/
	if( (att[0] == SHARK) && (att[1] == SHARK) && (att[2] == SHARK) && (att[3] == SHARK) )
	{
		*k = x;
		*l = y;
		return STOP;
	}

	/*Attorno allo squalo vi è almeno una cella di acqua, ma non un pesce*/
	if( ((att[0]==SHARK) || (att[0]==WATER)) && ((att[1]==SHARK) || (att[1]==WATER)) && ((att[2]==SHARK) || (att[2]==WATER)) && ((att[3]==SHARK) || (att[3]==WATER)) )
	{
		/*Determino casualmente una posizione dell'array da cui partire a cercare fino a che non trovo WATER*/
		while( (contatore < 3) && ((att[ind_rand])!=WATER) )
		{
			++contatore;
			ind_rand = (ind_rand+1)%4;
		}

		/*Adesso che so in quale indice di att[] ho l'acqua, lo squalo vi si sposta*/
		switch(ind_rand)
		{
			case 0:
			{
				*k = x;								/*Aggiorno il numero di riga*/
				*l = ((y-1)+_ncol) % _ncol;			/*Aggiorno il numero di colonna*/
				pw->plan->w[*k][*l] = SHARK;		/*Sposto lo squalo*/
				pw->plan->w[x][y] = WATER;			/*Setto a WATER la cella che era occupata dallo squalo*/

				pw->plan->btime[*k][*l] = pw->plan->btime[x][y];	/*Sposto il btime dello squalo sulla nuova cella*/
				pw->plan->dtime[*k][*l] = pw->plan->dtime[x][y];	/*Sposto il dtime dello squalo sulla nuova cella*/
				pw->plan->btime[x][y] = 0;							/*"Pulisco" il btime della cella liberata dallo squalo*/
				pw->plan->dtime[x][y] = 0;							/*"Pulisco" il dtime della cella liberata dallo squalo*/

				return MOVE;
			}
			case 1:
			{
				*k= ((x-1)+_nrow) % _nrow;			/*Aggiorno il numero di riga*/
				*l = y;								/*Aggiorno il numero di colonna*/
				pw->plan->w[*k][*l] = SHARK;		/*Sposto lo squalo*/
				pw->plan->w[x][y] = WATER;			/*Setto a WATER la cella che era occupata dallo squalo*/

				pw->plan->btime[*k][*l] = pw->plan->btime[x][y];	/*Sposto il btime dello squalo sulla nuova cella*/
				pw->plan->dtime[*k][*l] = pw->plan->dtime[x][y];	/*Sposto il dtime dello squalo sulla nuova cella*/
				pw->plan->btime[x][y] = 0;							/*"Pulisco" il btime della cella liberata dallo squalo*/
				pw->plan->dtime[x][y] = 0;							/*"Pulisco" il dtime della cella liberata dallo squalo*/
				return MOVE;
			}
			case 2:
			{
				*k = x;								/*Aggiorno il numero di riga*/
				*l = (y+1) %_ncol;					/*Aggiorno il numero di colonna*/
				pw->plan->w[*k][*l] = SHARK;		/*Sposto lo squalo*/
				pw->plan->w[x][y] = WATER;			/*Setto a WATER la cella che era occupata dallo squalo*/

				pw->plan->btime[*k][*l] = pw->plan->btime[x][y];	/*Sposto il btime dello squalo sulla nuova cella*/
				pw->plan->dtime[*k][*l] = pw->plan->dtime[x][y];	/*Sposto il dtime dello squalo sulla nuova cella*/	
				pw->plan->btime[x][y] = 0;							/*"Pulisco" il btime della cella liberata dallo squalo*/
				pw->plan->dtime[x][y] = 0;							/*"Pulisco" il dtime della cella liberata dallo squalo*/
				return MOVE;
			}
			case 3:
			{
				*k = (x+1)%_nrow;					/*Aggiorno il numero di riga*/
				*l=y;								/*Aggiorno il numero di colonna*/
				pw->plan->w[*k][*l] = SHARK;		/*Sposto lo squalo*/
				pw->plan->w[x][y] = WATER;			/*Setto a WATER la cella che era occupata dallo squalo*/

				pw->plan->btime[*k][*l] = pw->plan->btime[x][y];	/*Sposto il btime dello squalo sulla nuova cella*/
				pw->plan->dtime[*k][*l] = pw->plan->dtime[x][y];	/*Sposto il dtime dello squalo sulla nuova cella*/	
				pw->plan->btime[x][y] = 0;							/*"Pulisco" il btime della cella liberata dallo squalo*/
				pw->plan->dtime[x][y] = 0;							/*"Pulisco" il dtime della cella liberata dallo squalo*/
				return MOVE;
			}
		}
	}

	/*Se la funzione non ha ancora terminato, allora intorno vi è necessariamente un pesce. Determino
	casualmente una posizione dell'array da cui partire a cercare fino a che non trovo FISH*/
	while( (contatore < 3) && (att[ind_rand]) != FISH)
	{			
		++contatore;
		ind_rand = (ind_rand+1)%4;
	}

	/*Adesso che so in quale indice di att[] ho il pesce, lo squalo lo mangia*/
	switch(ind_rand)
	{
		case 0:
		{
			*k = x;									/*Aggiorno il numero di riga*/
			*l = ((y-1)+_ncol) % _ncol;				/*Aggiorno il numero di colonna*/
			pw->plan->w[*k][*l] = SHARK;			/*Sposto lo squalo*/
			pw->plan->w[x][y] = WATER;				/*Setto a WATER la cella che era occupata dallo squalo*/

			pw->plan->btime[*k][*l] = pw->plan->btime[x][y];		/*Sposto il btime dello squalo sulla nuova cella*/
			pw->plan->dtime[*k][*l] = (pw->plan->dtime[x][y])-1;	/*Sposto il dtime dello squalo sulla nuova cella*/
			pw->plan->btime[x][y] = 0;								/*"Pulisco" il btime della cella liberata dallo squalo*/
			pw->plan->dtime[x][y] = 0;								/*"Pulisco" il dtime della cella liberata dallo squalo*/
			return EAT;
		}
		case 1:
		{
			*k= ((x-1)+_nrow) % _nrow;				/*Aggiorno il numero di riga*/
			*l = y;									/*Aggiorno il numero di colonna*/
			pw->plan->w[*k][*l] = SHARK;
			pw->plan->w[x][y] = WATER;

			pw->plan->btime[*k][*l] = pw->plan->btime[x][y];		/*Sposto il btime dello squalo sulla nuova cella*/
			pw->plan->dtime[*k][*l] = (pw->plan->dtime[x][y])-1;	/*Sposto il dtime dello squalo sulla nuova cella*/
			pw->plan->btime[x][y] = 0;								/*"Pulisco" il btime della cella liberata dallo squalo*/
			pw->plan->dtime[x][y] = 0;								/*"Pulisco" il dtime della cella liberata dallo squalo*/
			return EAT;
		}
		case 2:
		{
			*k = x;									/*Aggiorno il numero di riga*/
			*l = (y+1) %_ncol;						/*Aggiorno il numero di colonna*/
			pw->plan->w[*k][*l] = SHARK;
			pw->plan->w[x][y] = WATER;

			pw->plan->btime[*k][*l] = pw->plan->btime[x][y];		/*Sposto il btime dello squalo sulla nuova cella*/
			pw->plan->dtime[*k][*l] = (pw->plan->dtime[x][y])-1;	/*Sposto il dtime dello squalo sulla nuova cella*/	
			pw->plan->btime[x][y] = 0;								/*"Pulisco" il btime della cella liberata dallo squalo*/
			pw->plan->dtime[x][y] = 0;								/*"Pulisco" il dtime della cella liberata dallo squalo*/
			return EAT;
		}
		case 3:
		{
			*k = (x+1)%_nrow;						/*Aggiorno il numero di riga*/
			*l=y;									/*Aggiorno il numero di colonna*/
			pw->plan->w[*k][*l] = SHARK;
			pw->plan->w[x][y] = WATER;

			pw->plan->btime[*k][*l] = pw->plan->btime[x][y];		/*Sposto il btime dello squalo sulla nuova cella*/
			pw->plan->dtime[*k][*l] = (pw->plan->dtime[x][y])-1;	/*Sposto il dtime dello squalo sulla nuova cella*/
			pw->plan->btime[x][y] = 0;								/*"Pulisco" il btime della cella liberata dallo squalo*/
			pw->plan->dtime[x][y] = 0;								/*"Pulisco" il dtime della cella liberata dallo squalo*/
			return EAT;
		}
	}
	return -1;			
}

int shark_rule2 (wator_t* pw, int x, int y, int *k, int* l)
{
	int _nrow = pw->plan->nrow;
	int _ncol = pw->plan->ncol;
	int _btime = pw->plan->btime[x][y];
	cell_t att[4];							/*Per maggiori dettagli su att[] vedi shark_rule1*/
	int _dtime = pw->plan->dtime[x][y];
	int ind_rand = rand()%4;				/*comincerò a controllare l'array att[] a partire da questo indice*/
	int contatore = 0;						/*serve per non controllare più di 3 posizioni di att[]*/

	if(pw == NULL)
	{
		errno = EINVAL;
		return -1;		
	}
	if(pw->plan->w[x][y] != SHARK) 			/*Controllo che nella cella [x][y] ci sia uno squalo*/
	{
		errno = EINVAL;
		return -1;
	}
	if(_btime == (pw->sb))					/*Può riprodursi*/
	{
		att[0] = pw->plan->w[x][((y-1) + _ncol) % _ncol];
		att[1] = pw->plan->w[((x-1) + _nrow) % _nrow][y];
		att[2] = pw->plan->w[x][(y+1) %_ncol];
		att[3] = pw->plan->w[(x+1)%_nrow ][y];

		/*Lo squalo ha intorno almeno una cella libera, dunque si riproduce*/
		if( (att[0] == WATER) || (att[1] == WATER) || (att[2] == WATER) || (att[3] == WATER) )
		{
			while( (contatore < 3) && (att[ind_rand]) != WATER)
			{			
				++contatore;
				ind_rand = (ind_rand+1)%4;
			}
		
			/*Adesso che so in quale indice di att[] ho l'acqua, lo squalo vi inserisce un figlio*/
			switch(ind_rand)
			{
				case 0:
				{
					*k = x;
					*l = ((y-1)+_ncol) % _ncol;
					pw->plan->w[*k][*l] = SHARK;

					pw->plan->btime[x][y] = 0;
					pw->plan->btime[*k][*l] = 0;
					pw->plan->dtime[*k][*l] = 0;
					break;
				}
				case 1:
				{
					*k= ((x-1)+_nrow) % _nrow;
					*l = y;
					pw->plan->w[*k][*l] = SHARK;
					pw->plan->btime[x][y]= 0;
					pw->plan->btime[*k][*l] = 0;
					pw->plan->dtime[*k][*l] = 0;
					break;
				}
				case 2:
				{
					*k = x;
					*l = (y+1) %_ncol;
					pw->plan->w[*k][*l] = SHARK;
					pw->plan->btime[x][y] = 0;
					pw->plan->btime[*k][*l] = 0;
					pw->plan->dtime[*k][*l] = 0;
					break;
				}
				case 3:
				{
					*k = (x+1)%_nrow;
					*l=y;
					pw->plan->w[*k][*l] = SHARK;
					pw->plan->btime[x][y] = 0;
					pw->plan->btime[*k][*l] = 0;
					pw->plan->dtime[*k][*l] = 0;
					break;
				}
			}
		}
		else					/*Lo squalo non ha celle libere intorno quindi la sua riproduzione non avviene*/
		{
			*k = x;
			*l = y;
			pw->plan->btime[x][y] = 0;
		}
	}
	if(_btime < (pw->sb)) 		/* Lo squalo non può riprodursi*/
	{
		++(pw->plan->btime[x][y]);
		*k = x;
		*l = y;
	}			
	/*Adesso che lo squalo si è riprodotto, controlliamo se muore*/
	if(_dtime < (pw->sd))				/*Non muore*/
	{
		++(pw->plan->dtime[x][y]);
		return ALIVE;
	}
	if(_dtime >= (pw->sd))				/*Muore*/
	{
		pw->plan->w[x][y] = WATER;
		pw->plan->btime[x][y] = 0;
		pw->plan->dtime[x][y] = 0;
		return DEAD;
	}
	return -1;
}

int fish_rule3 (wator_t* pw, int x, int y, int *k, int* l)
{
	int _nrow = pw->plan->nrow;
	int _ncol = pw->plan->ncol;
	cell_t att[4];						/*Per maggiori dettagli su att[] vedi shark_rule1*/
	int ind_rand = rand()%4;			/*comincerò a controllare l'array att[] a partire da questo indice*/
	int contatore = 0;					/*serve per non controllare più di 3 posizioni di att[]*/

	if(pw == NULL)
	{
		errno = EINVAL;
		return -1;		
	}
	if(pw->plan->w[x][y] != FISH) 		/*Controllo che nella cella [x][y] ci sia un pesce*/
	{
		errno = EINVAL;
		return -1;
	}

	att[0] = pw->plan->w[x][((y-1) + _ncol) % _ncol];
	att[1] = pw->plan->w[((x-1) + _nrow) % _nrow][y];
	att[2] = pw->plan->w[x][(y+1) %_ncol];
	att[3] = pw->plan->w[(x+1)%_nrow ][y];

	/*Il pesce ha intorno almeno una cella libera, dunque si sposta*/
	if( (att[0] == WATER) || (att[1] == WATER) || (att[2] == WATER) || (att[3] == WATER) )
	{
		while( (contatore < 3) && (att[ind_rand]) != WATER)
		{			
			++contatore;
			ind_rand = (ind_rand+1)%4;
		}
		/*Adesso che so in quale indice c'è l'acqua, il pesce vi si sposta*/
		switch(ind_rand)
		{
			case 0:
			{
				*k = x;
				*l = ((y-1)+_ncol) % _ncol;
				pw->plan->w[*k][*l] = FISH;
				pw->plan->w[x][y] = WATER;

				pw->plan->btime[*k][*l] = pw->plan->btime[x][y];		
				pw->plan->btime[x][y] = 0;
				pw->plan->dtime[*k][*l] = 0;
				pw->plan->dtime[x][y] = 0;

				return MOVE;
			}
			case 1:
			{
				*k= ((x-1)+_nrow) % _nrow;
				*l = y;
				pw->plan->w[*k][*l] = FISH;
				pw->plan->w[x][y] = WATER;

				pw->plan->btime[*k][*l] = pw->plan->btime[x][y];		
				pw->plan->btime[x][y] = 0;
				pw->plan->dtime[*k][*l] = 0;
				pw->plan->dtime[x][y] = 0;

				return MOVE;
			}
			case 2:
			{
				*k = x;
				*l = (y+1) %_ncol;
				pw->plan->w[*k][*l] = FISH;
				pw->plan->w[x][y] = WATER;

				pw->plan->btime[*k][*l] = pw->plan->btime[x][y];		
				pw->plan->btime[x][y] = 0;
				pw->plan->dtime[*k][*l] = 0;
				pw->plan->dtime[x][y] = 0;

				return MOVE;
			}
			case 3:
			{
				*k = (x+1)%_nrow;
				*l=y;
				pw->plan->w[*k][*l] = FISH;
				pw->plan->w[x][y] = WATER;

				pw->plan->btime[*k][*l] = pw->plan->btime[x][y];		
				pw->plan->btime[x][y] = 0;
				pw->plan->dtime[*k][*l] = 0;
				pw->plan->dtime[x][y] = 0;

				return MOVE;
			}
		}
	}
	/*Se la funzione prosegue fino a questo punto, il pesce è circondato, dunque resta fermo*/
	*k = x;
	*l = y;
	return STOP;
}

int fish_rule4 (wator_t* pw, int x, int y, int *k, int* l)
{
	int _nrow = pw->plan->nrow;
	int _ncol = pw->plan->ncol;
	int _btime = pw->plan->btime[x][y];
	cell_t att[4];							/*Per maggiori dettagli su att[] vedi shark_rule1*/
	int ind_rand = rand()%4;				/*comincerò a controllare l'array att[] a partire da questo indice*/
	int contatore = 0;						/*serve per non controllare più di 3 posizioni di att[]*/

	if(pw == NULL)
	{
		errno = EINVAL;
		return -1;		
	}
	if(pw->plan->w[x][y] != FISH)
	{
		errno = EINVAL;
		return -1;
	}
	if(_btime == (pw->fb))					/*Può riprodursi*/
	{
		att[0] = pw->plan->w[x][((y-1) + _ncol) % _ncol];
		att[1] = pw->plan->w[((x-1) + _nrow) % _nrow][y];
		att[2] = pw->plan->w[x][(y+1) %_ncol];
		att[3] = pw->plan->w[(x+1)%_nrow ][y];

		/*Il pesce ha intorno almeno una cella libera, dunque si riproduce*/
		if( (att[0] == WATER) || (att[1] == WATER) || (att[2] == WATER) || (att[3] == WATER) )
		{
			while( (contatore < 3) && (att[ind_rand]) != WATER)
			{			
				++contatore;
				ind_rand = (ind_rand+1)%4;
			}
			
			/*Adesso che ind_rand contiene l'indice di att[] in cui ho l'acqua, il pesce vi inserisce un figlio*/
			switch(ind_rand)
			{
				case 0:
				{
					*k = x;
					*l = ((y-1)+_ncol) % _ncol;
					pw->plan->w[*k][*l] = FISH;
					
					pw->plan->btime[x][y] = 0;
					pw->plan->btime[*k][*l] = 0;
					pw->plan->dtime[*k][*l] = 0;
					return 0;
				}
				case 1:
				{
					*k= ((x-1)+_nrow) % _nrow;
					*l = y;
					pw->plan->w[*k][*l] = FISH;

					pw->plan->btime[x][y] = 0;
					pw->plan->btime[*k][*l] = 0;
					pw->plan->dtime[*k][*l] = 0;
					return 0;
				}
				case 2:
				{
					*k = x;
					*l = (y+1) %_ncol;
					pw->plan->w[*k][*l] = FISH;
					pw->plan->btime[x][y] = 0;
					pw->plan->btime[*k][*l] = 0;
					pw->plan->dtime[*k][*l] = 0;
					return 0;
				}
				case 3:
				{
					*k = (x+1)%_nrow;
					*l=y;
					pw->plan->w[*k][*l] = FISH;
					pw->plan->btime[x][y] = 0;
					pw->plan->btime[*k][*l] = 0;
					pw->plan->dtime[*k][*l] = 0;
					return 0;
				}
			}
		}
		else						/*Non ci sono celle libere intorno al pesce, non si riproduce*/
		{
			*k = x;
			*l = y;
			pw->plan->btime[x][y] = 0;
		}
	}
	if(_btime < (pw->fb)) 					/*Non può riprodursi*/
	{
		++(pw->plan->btime[x][y]);
		*k = x;
		*l = y;
		return 0;
	}
	return -1;
}

int fish_count (planet_t* p)
{
	int contatore_pesci = 0;
	int _nrow = p->nrow;
	int _ncol = p->ncol;
	int i;
	int j;

	if (p == NULL)
	{
		errno = EINVAL;
		return -1;
	}

	for(i=0; i<_nrow; ++i)
	{
		for(j=0; j<_ncol; ++j)
		{
			if(p->w[i][j] == FISH) ++contatore_pesci;
		}
	}
	return contatore_pesci;
}

int shark_count (planet_t* p)
{
	int contatore_squali = 0;
	int _nrow = p->nrow;
	int _ncol = p->ncol;
	int i, j;

	if (p == NULL)
	{
		errno = EINVAL;
		return -1;
	}

	for(i=0; i<_nrow; ++i)
	{
		for(j=0; j<_ncol; ++j)
		{
			if(p->w[i][j] == SHARK) ++contatore_squali;
		}
	}

	return contatore_squali;
}

int update_wator (wator_t * pw)
{
	/*Matrice di soli zeri e uni che controlla se una determinata cella è già stata servita*/
	int ** servito;

	int _nrow = pw->plan->nrow;
	int _ncol = pw->plan->ncol;
	int i, j, k, l;
	int res;
	int check_col;

	if(pw == NULL)
	{
		errno = EINVAL;
		return -1;		
	}
	/*Alloco la matrice servito*/
	servito = (int**) malloc(_nrow*sizeof(int*));
	if(servito == NULL)
	{
		errno = ENOMEM;
		return -1;
	}
	for(i=0; i<_nrow; ++i)
	{
		servito[i] = (int*) malloc(_ncol*sizeof(int));
		for(check_col = 0; check_col<i; ++check_col)
		{
			if(servito[i] == NULL)
			{
				for(check_col = 0; check_col<_nrow; ++check_col) free(servito[check_col]);
				free(servito);
				errno = ENOMEM;
				return -1;
			}
		}
		/*Azzero l'intera matrice*/
		for(j=0; j<_ncol; ++j) servito[i][j] = 0;
	}

	/*Comincio ad aggiornare la matrice w*/
	for(i=0; i<_nrow; ++i)
	{
		for(j=0; j<_ncol; ++j)
		{
			if(servito[i][j] == 0)
			{
				switch(pw->plan->w[i][j])
				{
					case SHARK:
					{
						res = shark_rule2(pw, i, j, &k, &l);
						if(res==-1)
						{
							errno = EINVAL;
							free_wator(pw);
						}
						servito[k][l] = 1;
						shark_rule1(pw, i, j, &k, &l);
						if(res==-1)
						{
							errno = EINVAL;
							free_wator(pw);
						}
						servito[k][l] = 1;
						servito[i][j] = 1;
						break;
					}
					case FISH:
					{
						fish_rule4(pw, i, j, &k, &l);
						if(res==-1)
						{
							errno = EINVAL;
							free_wator(pw);
						}
						servito[k][l] = 1;
						fish_rule3(pw, i, j, &k, &l);
						if(res==-1)
						{
							errno = EINVAL;
							free_wator(pw);
						}
						servito[k][l] = 1;
						servito[i][j] = 1;
						break;
					}
					case WATER:
					{
						servito[i][j] = 1;
						break;
					}
				}
			}
		}
	}
	for(check_col = 0; check_col<i; ++check_col)
	{
		for(check_col = 0; check_col<_nrow; ++check_col) free(servito[check_col]);
		free(servito);
	}
	return 0;
}