/***************************************************************************************************
 * MAC0438 Programação Concorrente
 * EP1 Fórmula 1 - 10/04/2014
 *
 * Daniel Augusto Cortez 2960291
 * 
 * Arquivo: ep1.c
 * Compilação: gcc ep1.c -o ep1 -lpthread
 * Última atualização: 21/04/2014
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
typedef struct pilot Pilot;
typedef struct segment Segment;

/* Definição de uma estrutura piloto. */
struct pilot
{
	int id;   /* identificador do piloto */
	int team; /* identificador da equipe */
	Segment* segment; /* identifica em qual segmento da pista o piloto está */
};

/* Coleção de pilotos da competição. */
Pilot* pilots;

/* Definição de uma estrutura segmento de pista. */
struct segment
{
	int start;      /* distância do começo do segmento em metros */
	int is_double;  /* identifica se o segmento tem duas faixas */
	Pilot* p1;      /* piloto na primeira faixa */
	Pilot* p2;      /* piloto na segunda faixa, se houver */
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
sem_t* track_sems;

/* Vetor de semaforos, uma para cada segmento dos boxes. */
sem_t* boxes_sems;

/* Vetor de threads, uma para cada corredor. */
pthread_t* pids;

/* Define o começo da corrida. */
int start = 0;

/**************************************************************************************************/

void setup_track();
void setup_boxes();
int read_input_file(const char* file);
void setup_pilots();
void create_track_semaphores();
void create_boxes_semaphores();
void setup_start_grid();
void show_pilots();
void show_track();
void show_boxes();
void create_threads();
void* pilot_run(void* argument);
void join_threads();
void clean_up();

/**************************************************************************************************/

void setup_track()
{
	int i;

	for (i = 0; i < TRACK_SEGMENTS; i++) {
		track[i].start = i * SEGMENTS_DISTANCE;
		track[i].is_double = 0;
		track[i].p1 = track[i].p2 = NULL;
	}
}

/**************************************************************************************************/

void setup_boxes()
{
	int i;

	for (i = 0; i < BOXES_SEGMENTS; i++) {
		boxes[i].start = i * SEGMENTS_DISTANCE;
		boxes[i].is_double = 0;
		boxes[i].p1 = track[i].p2 = NULL;
	}
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
			track[i].is_double = 1;
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
		pilots[i].id = i + 1;
		pilots[i].team = 1 + (i / 2);  
		pilots[i].segment = NULL;
	}
}

/**************************************************************************************************/

void create_track_semaphores()
{
	int i;

	track_sems = malloc(TRACK_SEGMENTS * sizeof(sem_t));
	for (i = 0; i < TRACK_SEGMENTS; i++)
		if (track[i].is_double)
			sem_init(&track_sems[i], SHARED, 2);
		else
			sem_init(&track_sems[i], SHARED, 1);
}

/**************************************************************************************************/

void create_boxes_semaphores()
{
	int i;

	boxes_sems = malloc(BOXES_SEGMENTS * sizeof(sem_t));
	for (i = 0; i < BOXES_SEGMENTS; i++)
		sem_init(&boxes_sems[i], SHARED, 1);
}

/**************************************************************************************************/

void setup_start_grid()
{
	int i, j, pos;

	for (i = j = 0; i < m; i++, j += 2) {
		pos = TRACK_SEGMENTS - 1 - i;
		track[pos].p1 = &pilots[j];
		track[pos].p2 = &pilots[j + 1];
		pilots[j].segment = &track[pos];
		pilots[j + 1].segment = &track[pos];
		sem_wait(&track_sems[pos]);
		sem_wait(&track_sems[pos]);
	}
}

/**************************************************************************************************/

void show_pilots()
{
	int i;

	printf("------------------------\n");
	printf("PILOTOS\n");
	printf("Id\tEquipe\tPosição\n");
	printf("------------------------\n");
	for (i = 0; i < 2 * m; i++)
		printf("%d\t%d\t%d\n", pilots[i].id, pilots[i].team, pilots[i].segment->start);
}

/**************************************************************************************************/

void show_track()
{
	int i, p1, p2; char d;
	Segment s;

	printf("------------------------------------\n");
	printf("PISTA\n");
	printf("Posição\tComeço\tDuplo\tP_1\tP_2\n");
	printf("------------------------------------\n");
	for (i = 0; i < TRACK_SEGMENTS; i++) {
		s = track[i];
		d = s.is_double ? 'S' : 'N';
		p1 = s.p1 ? s.p1->id : 0;
		p2 = s.p2 ? s.p2->id : 0;
		printf("%d\t%d\t%c\t%d\t%d\n", i, s.start, d, p1, p2);
	}
}

/**************************************************************************************************/

void show_boxes()
{
	int i, p1, p2; char d;
	Segment s;

	printf("------------------------------------\n");
	printf("BOXES\n");
	printf("Posição\tComeço\tDuplo\tP_1\tP_2\n");
	printf("------------------------------------\n");
	for (i = 0; i < BOXES_SEGMENTS; i++) {
		s = boxes[i];
		d = s.is_double ? 'S' : 'N';
		p1 = s.p1 ? s.p1->id : 0;
		p2 = s.p2 ? s.p2->id : 0;
		printf("%d\t%d\t%c\t%d\t%d\n", i, s.start, d, p1, p2);
	}
}

/**************************************************************************************************/

void create_threads()
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
	Pilot* pilot = (Pilot*) argument;
	while (!start);
	printf("[Piloto %d iniciou a corrida]\n", pilot->id);
	return NULL; 
}

/**************************************************************************************************/

void join_threads()
{
	int i; 

	for (i = 0; i < 2 * m; i++)
		pthread_join(pids[i], NULL);
}

/**************************************************************************************************/

void clean_up()
{
	if (pilots) free(pilots);
	if (track_sems) free(track_sems);
	if (boxes_sems) free(boxes_sems);
	if (pids) free(pids);
}

/**************************************************************************************************/

int main(int argc, char** argv)
{
	if (argc != 2) {
		printf("Uso: ep1 <arquivo_de_entrada>\n");
		return EXIT_FAILURE;
	}
	setup_track();
	setup_boxes();
	if (read_input_file(argv[1])) {
		setup_pilots();
		create_track_semaphores();
		create_boxes_semaphores();
		setup_start_grid();
		show_pilots();
		show_track();
		show_boxes();
	}
	create_threads();

	printf("[Foi dada a largada]\n");
	start = 1;

	join_threads();
	clean_up();
	
	return EXIT_SUCCESS;
}
