/***************************************************************************************************
 * MAC0438 Programação Concorrente
 * EP1 Fórmula 1 - 10/04/2014
 *
 * Daniel Augusto Cortez 2960291
 * 
 * Arquivo: ep1.c
 * Compilação: gcc ep1.c -o ep1 -lpthread
 * Última atualização: 07/04/2014
 **************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> 
#include <semaphore.h> 
#include <unistd.h>

#define SHARED 1 

/* Velocidade normal dos carros em metros por segundo. */
#define SPEED 50

/* Velocidade reduzida dos carros em metros por segundo. */
#define HALF_SPEED 25

/* Velocidade dos carros dentro dos boxes em metros por segundo. */
#define BOX_SPEED 25

/* Número de voltas finais em que k% dos carros terão velocidade reduzida. */
#define FINAL_LAPS 10

/* Distância em metros entre segmentos da pista. */
#define SEGMENTS_DISTANCE 25

/* Número de segmentos da pista (extensão 4000 m). */
#define TRACK_SEGMENTS 160

/* Número de segmentos dos boxes (extensão 250 m). */
#define BOXES_SEGMENTS 10

/* Tempos mínimo e máximo que um carro pode permanecer no box em segundos. */
#define MIN_BOX_TIME 4
#define MAX_BOX_TIME 10

/* Intervalo de tempo em milisegundos correspondente a cada iteração da corrida. */
#define RATE 500

/* Intervalo de tempo em minutos para que o relatório da corrida seja exibido. */
#define SHOW_REPORT_INTERVAL 15

/* Definição dos tipos para piloto e segmento de pista. */
typedef struct pilot* Pilot;
typedef struct segment* Segment;

/* Definição de uma estrutura piloto. */
struct pilot
{
	int id;   /* identificador do piloto */
	int team; /* identificador da equipe */
	Segment segment; /* identifica em qual segmento da pista o piloto está */
};

/* Coleção de pilotos da competição. */
Pilot* pilots;

/* Definição de uma estrutura segmento de pista. */
struct segment
{
	int index;     /* índice identificador do segmento */
	int position;  /* posição do começo do segmento em metros */
	int is_double; /* identifica se o segmento tem duas faixas */
	Pilot p1;      /* piloto na primeira faixa */
	Pilot p2;      /* piloto na segunda faixa, se houver */
};

/* A pista será representada por um vetor de TRACK_SEGMENTS segmentos. */
Segment track[TRACK_SEGMENTS];

/* Os boxes serão representados por um vetor de BOXES_SEGMENTS segmentos. */
Segment boxes[BOXES_SEGMENTS];

/* Número de voltas. */
int n;

/* Número de equipes. */
int m; 

/* Porcentagem de pilotos que terá a velocidade modificada nas últimas dez voltas. */
int k = 0;

/* Vetor de semaforos, uma para cada segmento de pista. */
sem_t* track_mutex;

/* Vetor de semaforos, uma para cada segmento dos boxes. */
sem_t* boxes_mutex;

/* Vetor de threads, uma para cada corredor. */
pthread_t* pids;

/* Vetores de semaforos utilizados para implementar a barreira de sincronização. */
sem_t* arrive;
sem_t* go_on;

/* Thread coordenadora para a barreira de sincronização. */
pthread_t coordinator;

/* Define o começo da corrida. */
int start = 0;

/**************************************************************************************************/

void setup_track();
void create_track_mutexes();

void setup_boxes();
void create_boxes_mutexes();

int read_input_file(const char* file);
void setup_pilots();
void setup_start_grid();

void show_pilots();
void show_track();
void show_boxes();

void create_barrier_semaphores();
void create_coordinator_thread();
void* barrier_sync(void* argument);

void create_pilots_threads();
void* pilot_run(void* argument);

void join_threads();

void clean_up();

/**************************************************************************************************/

void setup_track()
{
	int i;

	for (i = 0; i < TRACK_SEGMENTS; i++) {
		track[i] = malloc(sizeof(*track[i]));
		track[i]->index = i;
		track[i]->position = i * SEGMENTS_DISTANCE;
		track[i]->is_double = 0;
		track[i]->p1 = track[i]->p2 = NULL;
	}
}

/**************************************************************************************************/

void create_track_mutexes()
{
	int i;

	track_mutex = malloc(TRACK_SEGMENTS * sizeof(sem_t));
	for (i = 0; i < TRACK_SEGMENTS; i++)
		if (track[i]->is_double)
			sem_init(&track_mutex[i], SHARED, 2);
		else
			sem_init(&track_mutex[i], SHARED, 1);
}

/**************************************************************************************************/

void setup_boxes()
{
	int i;

	for (i = 0; i < BOXES_SEGMENTS; i++) {
		boxes[i] = malloc(sizeof(*boxes[i]));
		boxes[i]->index = i;
		boxes[i]->position = i * SEGMENTS_DISTANCE;
		boxes[i]->is_double = 0;
		boxes[i]->p1 = track[i]->p2 = NULL;
	}
}

/**************************************************************************************************/

void create_boxes_mutexes()
{
	int i;

	boxes_mutex = malloc(BOXES_SEGMENTS * sizeof(sem_t));
	for (i = 0; i < BOXES_SEGMENTS; i++)
		sem_init(&boxes_mutex[i], SHARED, 1);
}

/**************************************************************************************************/

int read_input_file(const char* file)
{
	FILE* fd; char buf[2];
	int i, begin, end;

	if (!(fd = fopen(file, "r")))
		return 0;	

	fscanf(fd, "%d", &n);
	fscanf(fd, "%d", &m);
	fscanf(fd, "%s", buf);
	if (strcmp(buf, "A") == 0)
		fscanf(fd, "%d", &k);
	while (fscanf(fd, "%d", &begin) >= 0) {
		fscanf(fd, "%d", &end);
		for (i = begin; i <= end; i++)
			track[i]->is_double = 1;
	}
	fclose(fd);
	
	return 1;
}

/**************************************************************************************************/

void setup_pilots()
{
	int i;

	pilots = malloc(2 * m * sizeof(Pilot));
	for (i = 0; i < 2 * m; i++) {
		pilots[i] = malloc(sizeof(*pilots[i]));
		pilots[i]->id = i + 1;
		pilots[i]->team = 1 + (i / 2);  
		pilots[i]->segment = NULL;
	}
}

/**************************************************************************************************/

void setup_start_grid()
{
	int i, j, index;

	for (i = j = 0; i < m; i++, j += 2) {
		index = TRACK_SEGMENTS - 1 - i;
		track[index]->p1 = pilots[j];
		track[index]->p2 = pilots[j + 1];
		pilots[j]->segment = track[index];
		pilots[j + 1]->segment = track[index];
	}
}

/**************************************************************************************************/

void show_pilots()
{
	int i;
	Segment s;

	printf("--------------------------------\n");
	printf("PILOTOS\n");
	printf("Id\tEquipe\tPosição\tÍndice\n");
	printf("--------------------------------\n");
	for (i = 0; i < 2 * m; i++) {
		s = pilots[i]->segment;
		printf("%d\t%d\t%d\t%d\n", pilots[i]->id, pilots[i]->team, s->position, s->index);
	}
}

/**************************************************************************************************/

void show_track()
{
	int i; 
	char d, buff1[4], buff2[4];
	Segment s;

	printf("-------------------------------------\n");
	printf("PISTA\n");
	printf("Índice\tPosição\tDuplo\tP_1\tP_2\n");
	printf("-------------------------------------\n");
	for (i = 0; i < TRACK_SEGMENTS; i++) {
		s = track[i];
		d = s->is_double ? 'S' : 'N';

		if (s->p1)
			sprintf(buff1, "%d", s->p1->id);
		else
			sprintf(buff1, "-");

		if (s->p2)
			sprintf(buff2, "%d", s->p2->id);
		else
			sprintf(buff2, "-");

		printf("%d\t%d\t%c\t%s\t%s\n", s->index, s->position, d, buff1, buff2);
	}
}

/**************************************************************************************************/

void show_boxes()
{
	int i, p1, p2; char d;
	Segment s;

	printf("------------------------------------\n");
	printf("BOXES\n");
	printf("Índice\tPosição\tDuplo\tP_1\tP_2\n");
	printf("------------------------------------\n");
	for (i = 0; i < BOXES_SEGMENTS; i++) {
		s = boxes[i];
		d = s->is_double ? 'S' : 'N';
		p1 = s->p1 ? s->p1->id : 0;
		p2 = s->p2 ? s->p2->id : 0;
		printf("%d\t%d\t%c\t%d\t%d\n", s->index, s->position, d, p1, p2);
	}
}

/**************************************************************************************************/

void create_barrier_semaphores()
{
	int i;

	arrive = malloc(2 * m * sizeof(sem_t));
	go_on = malloc(2 * m * sizeof(sem_t));
	for (i = 0; i < 2 * m; i++) {
		sem_init(&arrive[i], SHARED, 0);
		sem_init(&go_on[i], SHARED, 0);
	}
}

/**************************************************************************************************/

void create_coordinator_thread()
{
	pthread_attr_t attr;
	
	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&coordinator, &attr, barrier_sync, NULL);
}

/**************************************************************************************************/

void* barrier_sync(void* argument)
{
	int i, num_pilots = 2 * m;

	while (1) {
		for (i = 0; i < num_pilots; i++)
			sem_wait(&arrive[i]);
		for (i = 0; i < num_pilots; i++)
			sem_post(&go_on[i]);		
	}
}

/**************************************************************************************************/

void create_pilots_threads()
{
	pthread_attr_t attr;
	int i;
	
	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pids = malloc(2 * m * sizeof(pthread_t));
	for (i = 0; i < 2 * m; i++)
		pthread_create(&pids[i], &attr, pilot_run, (void*) &pilots[i]);
}

/**************************************************************************************************/

void* pilot_run(void* argument)
{
	Pilot pilot = *((Pilot*) argument);
	int current_index, next_index;
	Segment current, next;	
	int lap = 0, fuel = 1 + (n + 1)/2, N = 11;
	
	
	while (!start);
	/* printf("[Piloto %d iniciou a corrida]\n", pilot->id); */

	/*
	if (next_index == 0) {
  	lap++;
		fuel--;    		
	}
	*/

	while (N-- > 0) {
		current_index = pilot->segment->index;
		next_index = (current_index + 1) % TRACK_SEGMENTS;
		sem_wait(&track_mutex[next_index]);	
		{	
			current = track[current_index];	    
			next = track[next_index];
			if (next->is_double) {
				if (next->p1 == NULL) {
					next->p1 = pilot;				
					pilot->segment = next; 
					sem_wait(&track_mutex[current_index]); 
					{
						if (current->p1 == pilot) {
							current->p1 = NULL;
						}
						else if (current->p2 == pilot) {
							current->p2 = NULL;
						}
						printf("[Piloto %d fez %d -> %d]\n", pilot->id, current_index, next_index);
					}
					sem_post(&track_mutex[current_index]); 
				}
				else if (next->p2 == NULL) {
					next->p2 = pilot;
					pilot->segment = next;
					sem_wait(&track_mutex[current_index]); 
					{
						if (current->p1 == pilot) {
							current->p1 = NULL;
						}
						else if (current->p2 == pilot) {
							current->p2 = NULL;
						}
						printf("[Piloto %d fez %d -> %d]\n", pilot->id, current_index, next_index);
					}
					sem_post(&track_mutex[current_index]); 
				}
			}
			else {
				if (next->p1 == NULL) {
					next->p1 = pilot;
					pilot->segment = next;
					sem_wait(&track_mutex[current_index]); 
					{
						if (current->p1 == pilot) {
							current->p1 = NULL;
						}
						else if (current->p2 == pilot) {
							current->p2 = NULL;
						}
						printf("[Piloto %d fez %d -> %d]\n", pilot->id, current_index, next_index);
					}
					sem_post(&track_mutex[current_index]); 
				}
			}
		}
		sem_post(&track_mutex[next_index]);	
		/* Barreira de sincronizção */
		sem_post(&arrive[pilot->id - 1]);
		sem_wait(&go_on[pilot->id - 1]);		
	}
	printf("[Piloto %d finalizou a corrida no índice %d]\n", pilot->id, pilot->segment->index);	

	return NULL; 
}

/**************************************************************************************************/

void join_threads()
{
	int i; 

	for (i = 0; i < 2 * m; i++)
		pthread_join(pids[i], NULL);

	printf("[Cancelando thread coordenadora.]\n");
	pthread_cancel(coordinator);
}

/**************************************************************************************************/

void clean_up()
{
	int i;

	for (i = 0; i < TRACK_SEGMENTS; i++)
		if (track[i])
			free(track[i]);

	if (track_mutex) 
		free(track_mutex);

	for (i = 0; i < BOXES_SEGMENTS; i++)
		if (boxes[i]) 
			free(boxes[i]);
		
	if (boxes_mutex) 
		free(boxes_mutex);
	
	if (pilots) {
		for (i = 0; i < 2*m; i++)
			if (pilots[i])
				free(pilots[i]);
		free(pilots);
	}

	if (arrive)
		free(arrive);
	
	if (go_on)
		free(go_on);
	
	if (pids) 
		free(pids);
}void create_track_mutexes();

/**************************************************************************************************/

int main(int argc, char** argv)
{
	if (argc != 2) {
		printf("Uso: ep1 <arquivo_de_entrada>\n");
		return EXIT_FAILURE;
	}
	
	setup_track();
		
	setup_boxes();
	create_boxes_mutexes();

	if (read_input_file(argv[1])) {
		create_track_mutexes();		
		setup_pilots();
		setup_start_grid();
		show_track();
		show_boxes();
		show_pilots();
	}

	create_barrier_semaphores();
	create_coordinator_thread();

	create_pilots_threads();

	printf("[Foi dada a largada]\n");
	start = 1;

	join_threads();
	
	show_track();
	show_pilots();

	clean_up();
	
	return EXIT_SUCCESS;
}
