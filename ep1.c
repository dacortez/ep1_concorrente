/***************************************************************************************************
 * MAC0438 Programação Concorrente
 * EP1 Fórmula 1 - 21/04/2014
 *
 * Daniel Augusto Cortez 2960291
 * 
 * Arquivo: ep1.c
 * Compilação: gcc ep1.c -o ep1 -Wall -ansi -pedantic -lpthread
 * Última atualização: 20/04/2014
 **************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> 
#include <semaphore.h> 
#include <time.h> 
#include <unistd.h>

/* Utilize 1 se desejar informações adicionais de debug durante a execução, ou 0 caso contrário. */
#define DEBUG 0

/* Utilizado para inicialização das threads. */
#define SHARED 1 

/* Número de voltas finais em que k% dos carros terão velocidade reduzida. */
#define FINAL_LAPS 10

/* Distância em metros entre segmentos da pista. */
#define SEGMENTS_DISTANCE 25

/* Número de segmentos da pista (extensão 4000 m). */
#define TRACK_SEGMENTS 160

/* Número de segmentos dos boxes (extensão 250 m). */
#define BOXES_SEGMENTS 10

/* Índice de posição da pista onde se faz a transição para os boxes. */
#define BOXES_ENTRY (TRACK_SEGMENTS - BOXES_SEGMENTS - 1)

/* Índice de posição dos boxes onde se faz o reabastecimento e troca de pneus. */
#define PITSTOP_POINT (BOXES_SEGMENTS / 2)

/* Tempos mínimo e máximo que um carro pode permanecer no box em segundos. */
#define MIN_BOXE_TIME 4
#define MAX_BOXE_TIME 10

/* Intervalo de tempo em milisegundos correspondente a cada iteração da corrida. */
#define RATE 500

/* Intervalo de tempo em minutos para que o relatório da corrida seja exibido. */
#define SHOW_REPORT_INTERVAL 15

/* Linha utilizada para separação na impressão dos relatórios. */
#define LINE "----------------------------------------------------------\n"

/* Definição dos tipos para piloto e segmento de pista. */
typedef struct pilot* Pilot;
typedef struct segment* Segment;

/* Definição de uma estrutura piloto. */
struct pilot
{
	int id;            /* identificador do piloto */
	int team;          /* identificador da equipe */
	Segment segment;   /* identifica em qual segmento da pista o piloto está */
	int lap;           /* número da volta em que o piloto está */
	int fuel;          /* combustível disponível */
	int is_finished;   /* indica se já terminou a prova */
	int order;         /* ordem de chegada no final da prova */
	int points;        /* pontuação do piloto no campeonato */
	int reduce;        /* indica se o piloto deve reduzir velocidade no final na corrida */
	int boxe_lap;      /* volta em que o piloto entrará nos boxes */
	int in_boxes;      /* indica que o piloto está nas faixas dos boxes */
	int in_pitstop;    /* indica que o piloto está no boxe fazendo o pitstop */
	int pitstop_time;  /* tempo em segundos de permanência nos boxes */
	int pitstop_delay; /* número de iterações que o piloto deve permanecer no pitstop */
	int has_stoped;    /* indica se o piloto já fez sua parada nos boxes */
};

/* Definição de uma estrutura segmento de pista. */
struct segment
{
	int index;     /* índice identificador do segmento */
	int position;  /* posição do começo do segmento em metros */
	int is_double; /* identifica se o segmento tem duas faixas */
	Pilot p1;      /* piloto na primeira faixa */
	Pilot p2;      /* piloto na segunda faixa, se houver */
};

/* Definição de uma estrutura para equipe. */
typedef struct team {
	int id;
	int points;
} Team;

/* Coleção de pilotos da competição. */
Pilot* pilots;

/* A pista será representada por um vetor de TRACK_SEGMENTS segmentos. */
Segment track[TRACK_SEGMENTS];

/* Os boxes serão representados por um vetor de BOXES_SEGMENTS segmentos. */
Segment boxes[BOXES_SEGMENTS];

/* Número de voltas. */
int n;

/* Número de equipes. */
int m; 

/* Porcentagem de pilotos que terá a velocidade modificada nas últimas FINAL_LAPS voltas. */
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

/* Conta a ordem de chegada dos pilotos no finalizarem a prova. */
int order = 0;

/* Define a largada da corrida. */
int start = 0;

/**************************************************************************************************/

void setup_track();
void setup_boxes();
int read_input_file(const char* file);
void create_track_mutexes();
void create_boxes_mutexes();
void setup_pilots();
void choose_reduced_pilots();
int random_int(int a, int b);
void setup_start_grid();
void show_pilots();
void show_track();
void show_boxes();
void pilot_tos(Pilot pilot, char* string);
void create_barrier_semaphores();
void create_coordinator_thread();
void* barrier_sync(void* argument);
void show_pilots_report(double time);
void show_pilot_status(Pilot pilot);
int first_has_completed_lap();
void show_first_three_pilots_and_update_order();
Pilot get_next(Pilot first, Pilot second);
int get_dist(Pilot pilot);
void create_pilots_threads();
void* pilot_run(void* argument);
void move_in_track(Pilot pilot);
int should_enter_boxes(Pilot pilot);
void move_in_boxes(Pilot pilot);
void try_to_move(Pilot pilot, Segment current, Segment next);
void move(Pilot pilot, Segment current, Segment next);
void update_pilot(Pilot pilot, Segment current, Segment next);
void exit_track(Pilot pilot);
void join_threads();
void show_race_result();
int order_cmp(const void* a, const void* b);
void show_championship_classification();
int points_cmp(const void* a, const void* b);
void show_teams_classification();
int teams_cmp(const void* a, const void* b);
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

void setup_boxes()
{
	int i;

	for (i = 0; i < BOXES_SEGMENTS; i++) {
		boxes[i] = malloc(sizeof(*boxes[i]));
		boxes[i]->index = i;
		boxes[i]->position = track[TRACK_SEGMENTS - BOXES_SEGMENTS + i]->position;
		boxes[i]->is_double = 0;
		boxes[i]->p1 = track[i]->p2 = NULL;
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
			track[i]->is_double = 1;
	}
	fclose(fd);
	
	return 1;
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

void create_boxes_mutexes()
{
	int i;

	boxes_mutex = malloc(BOXES_SEGMENTS * sizeof(sem_t));
	for (i = 0; i < BOXES_SEGMENTS; i++)
		if (i == PITSTOP_POINT)
			sem_init(&boxes_mutex[i], SHARED, m);
		else
			sem_init(&boxes_mutex[i], SHARED, 1);
}

/**************************************************************************************************/

void setup_pilots()
{
	int i, num_pilots = 2 * m;

	pilots = malloc(num_pilots * sizeof(Pilot));
	for (i = 0; i < num_pilots; i++) {
		pilots[i] = malloc(sizeof(*pilots[i]));
		pilots[i]->id = i + 1;
		pilots[i]->team = 1 + (i / 2);  
		pilots[i]->segment = NULL;
		pilots[i]->lap = 0;
		pilots[i]->fuel = 1 + (n + 1) / 2;
		pilots[i]->is_finished = 0;
		pilots[i]->order = 0;
		pilots[i]->points = random_int(200, 250);
		pilots[i]->reduce = 0;
		pilots[i]->boxe_lap = (n / 2) + i % 2;
		pilots[i]->in_boxes = 0;		
		pilots[i]->in_pitstop = 0;
		pilots[i]->pitstop_time = 0;
		pilots[i]->pitstop_delay = 0;
		pilots[i]->has_stoped = 0;
	}
}

/**************************************************************************************************/

void choose_reduced_pilots()
{
	int i, reduced, count, num_pilots = 2 * m;

	if (k > 0) {
		reduced = (num_pilots * k / 100.0);
		count = 0;
		while (count < reduced) {
			i = random_int(0, num_pilots - 1);
			if (!pilots[i]->reduce) {
				++count;
				pilots[i]->reduce = 1;	
			}		
		}		
	}
}

/**************************************************************************************************/

/* Retorna um inteiro aleatório no intervalo fechado [a, b]. */
int random_int(int a, int b)
{
	double r, x, R = RAND_MAX;
	int i;

	r = rand();
	x = r / (R + 1);
	i = x * (b - a + 1);

	return a + i;
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
	Pilot p;

	printf(LINE);
	printf("PILOTOS\n");
	printf("Piloto\tEquipe\tPontos\tPosição\tÍndice\tReduz\tVolta_Boxe\n");
	printf(LINE);
	for (i = 0; i < 2 * m; i++) {
		p = pilots[i];
		s = p->segment;
		printf("%d\t%d\t%d\t%d\t%d\t%c\t%d\n", 
			p->id, p->team, p->points, s->position, s->index, p->reduce ? 'S' : 'N', p->boxe_lap);
	}
	printf(LINE);
}

/**************************************************************************************************/

void show_track()
{
	int i; 
	char d, buf1[4], buf2[4];
	Segment s;

	printf("\n");
	printf(LINE);
	printf("PISTA\n");
	printf("Índice\tPosição\tDuplo\tP_1\tP_2\n");
	printf(LINE);
	for (i = 0; i < TRACK_SEGMENTS; i++) {
		s = track[i];
		d = s->is_double ? 'S' : 'N';
		pilot_tos(s->p1, buf1);		
		pilot_tos(s->p2, buf2);	
		printf("%d\t%d\t%c\t%s\t%s\n", s->index, s->position, d, buf1, buf2);
	}
	printf(LINE);
}

/**************************************************************************************************/

void show_boxes()
{
	int i;
	char d, buf1[4], buf2[4];
	Segment s;

	printf("\n");
	printf(LINE);
	printf("BOXES\n");
	printf("Índice\tPosição\tDuplo\tP_1\tP_2\n");
	printf(LINE);
	for (i = 0; i < BOXES_SEGMENTS; i++) {
		s = boxes[i];
		d = s->is_double ? 'S' : 'N';
		pilot_tos(s->p1, buf1);
		pilot_tos(s->p2, buf2);
		printf("%d\t%d\t%c\t%s\t%s\n", s->index, s->position, d, buf1, buf2);
	}
	printf(LINE);
}

/**************************************************************************************************/

void pilot_tos(Pilot pilot, char* string)
{
	if (pilot)
		sprintf(string, "%d", pilot->id);
	else
		sprintf(string, "-");
}

/**************************************************************************************************/

void create_barrier_semaphores()
{
	int i, num_pilots = 2 * m;

	arrive = malloc(num_pilots * sizeof(sem_t));
	go_on = malloc(num_pilots * sizeof(sem_t));
	for (i = 0; i < num_pilots; i++) {
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
	int i, all_finished, count = 0, num_pilots = 2 * m;
	int show_report = (int) (SHOW_REPORT_INTERVAL * 60.0) / (RATE / 1000.0); 	

	while (1) {
		all_finished = 1;

		/* Espera todos que estão na corrida terminarem a iteração. */
		for (i = 0; i < num_pilots; i++)
			if (!pilots[i]->is_finished) {
				sem_wait(&arrive[i]);				
				all_finished = 0;								
			}
		
		/* Neste ponto todos os pilotos completaram a iteração 
       e estão esperando para continuar. Podemos imprimir 
       os relatórios necessários pois não haverá mudança   
       nas posições. */ 

		if (++count % show_report == 0)	
				show_pilots_report(count * RATE / (1000.0 * 60.0));

		if (first_has_completed_lap())		
			show_first_three_pilots_and_update_order();		

		/* Libera para continuar a próxima iteração. */
		for (i = 0; i < num_pilots; i++)
			sem_post(&go_on[i]);	
		
		if (all_finished)
			return NULL;			
	}
}

/**************************************************************************************************/

void show_pilots_report(double time)
{
	int i;

	printf("\n");
	printf(LINE);
	printf("RELATÓRIO DOS PILOTOS (TEMPO = %3.1f')\n", time);
	printf("Piloto\tEquipe\tVolta\tPosição\tÍndice\tFuel\n");
	printf(LINE);
	for (i = 0; i < 2 * m; i++)
		show_pilot_status(pilots[i]);	
	printf(LINE);
}	

/**************************************************************************************************/

void show_pilot_status(Pilot pilot)
{
	Segment s;
	
	if (pilot) {
		s = pilot->segment;
		printf("%d\t%d\t%d\t%d\t%d\t%d\n", 
			pilot->id, pilot->team, pilot->lap, s->position, s->index, pilot->fuel);
	}
}	

/**************************************************************************************************/

int first_has_completed_lap()
{
	Segment s;
	int i, lap = 0;
		
	s = track[0];
	if (s->p1 && s->p2)
		lap = s->p1->lap >= s->p2->lap ? s->p1->lap : s->p2->lap;  	
	else if (s->p1)
		lap = s->p1->lap; 	
	else if (s->p2)
		lap = s->p2->lap; 	

	if (lap) {
		for (i = 1; i < TRACK_SEGMENTS; i++) {
			s = track[i];			
			if (s->p1 && s->p1->lap >= lap)
				return 0;		
			if (s->p2 && s->p2->lap >= lap)
				return 0;		
		}
		return 1;	
	}

	return 0;
}		

/**************************************************************************************************/

void show_first_three_pilots_and_update_order()
{
	Pilot first, second, third;
	
	first = get_next(NULL, NULL);
	second = get_next(first, NULL);
	third = get_next(first, second);

	/* A classificação na corrida depende da ordem em
     que foi escolhido os primeiro e segundo colocados
     nas chamadas anteriores. */

	if (first && first->lap > n)
		first->order = ++order;
	if (second && second->lap > n)
		second->order = ++order;
				
	printf("\n");
	printf(LINE);
	printf("PRIMEIROS COLOCADOS\n");
	printf("Piloto\tEquipe\tVolta\tPosição\tÍndice\tFuel\n");
	printf(LINE);
	show_pilot_status(first);
	show_pilot_status(second);	
	show_pilot_status(third);
	printf(LINE);
}	

/**************************************************************************************************/

Pilot get_next(Pilot first, Pilot second)
{
	int i, dist, num_pilots = 2 * m, max_dist = -TRACK_SEGMENTS;
	Pilot p, next = NULL;
 
	for (i = 0; i < num_pilots; i++) {
		p = pilots[i];
		if (!p->is_finished && p != first && p != second) {
			dist = get_dist(p);
			if (dist > max_dist) {			
				next = p;
				max_dist = dist;
			}
			else if (dist == max_dist && rand() % 2)		
				next = p;
		}
	}

	return next;
}

/**************************************************************************************************/

/* Retorna a distância total percorrida pelo piloto na corrida. */
int get_dist(Pilot pilot)
{
	return (pilot->lap - 1) * TRACK_SEGMENTS + pilot->segment->index;
}

/**************************************************************************************************/

void create_pilots_threads()
{
	pthread_attr_t attr;
	int i, num_pilots = 2 * m;
	
	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pids = malloc(num_pilots * sizeof(pthread_t));
	for (i = 0; i < num_pilots; i++)
		pthread_create(&pids[i], &attr, pilot_run, (void*) &pilots[i]);
}

/**************************************************************************************************/

void* pilot_run(void* argument)
{
	Pilot pilot = *((Pilot*) argument);
	int move_turn = 1, boxe_move_turn = 1;

	/* espera ocupada enquanto não for dada a largada na thread main */
	while (!start);
	
	while (pilot->lap <= n) {
		if (!pilot->in_boxes) { /* movimenta o piloto sobre a pista */
			if (pilot->reduce && pilot->lap > n - FINAL_LAPS)
				move_turn = 1 - move_turn;		
			if (pilot->fuel && move_turn)
				move_in_track(pilot);				
		}
		else { /* movimenta o piloto sobre os boxes */
			if (pilot->in_pitstop && --pilot->pitstop_delay <= 0) {	
					move_in_boxes(pilot);	
			}
			else if (!pilot->in_pitstop) {
				boxe_move_turn = boxe_move_turn - 1;
				if (pilot->fuel && boxe_move_turn)
					move_in_boxes(pilot);
			}		
			else {
				if (DEBUG) printf("[Piloto %d no pitstop (time = %d, delay = %d)]\n", pilot->id, pilot->pitstop_time, pilot->pitstop_delay);
			}	
		}			
		/* barreira de sincronizção */
		sem_post(&arrive[pilot->id - 1]);
		sem_wait(&go_on[pilot->id - 1]);		
	}
	
	if (!pilot->is_finished) {			
		exit_track(pilot);				
		/* barreira de sincronizção */
		sem_post(&arrive[pilot->id - 1]); 
		sem_wait(&go_on[pilot->id - 1]); 		
	}

	return NULL; 
}

/**************************************************************************************************/

void move_in_track(Pilot pilot) 
{
	int current_index = pilot->segment->index;
	int next_index = (current_index + 1) % TRACK_SEGMENTS;		
	
	if (should_enter_boxes(pilot)) { /* esta na hora de entrar nos boxes */
		sem_wait(&track_mutex[current_index]);		
		sem_wait(&boxes_mutex[0]);
		{
			try_to_move(pilot, track[current_index], boxes[0]);
		}
		sem_post(&boxes_mutex[0]);
		sem_post(&track_mutex[current_index]);		
	}
	else { /* nao esta na hora de entrar nos boxes, permanece na pista */
		sem_wait(&track_mutex[current_index]);		
		sem_wait(&track_mutex[next_index]);	
		{
			try_to_move(pilot, track[current_index], track[next_index]); 
		}
		sem_post(&track_mutex[next_index]);
		sem_post(&track_mutex[current_index]);
	}
}

/**************************************************************************************************/

int should_enter_boxes(Pilot pilot)
{
	int i, num_pilots = 2 * m;
	int current_index = pilot->segment->index;
	
	if (current_index == BOXES_ENTRY && !pilot->has_stoped && pilot->lap >= pilot->boxe_lap) {	
		for (i = 0; i < num_pilots; i++)
			if (pilot->team == pilots[i]->team && pilots[i]->in_pitstop)
				return 0;
		return 1;
	}	
	else			
		return 0;
}

/**************************************************************************************************/

void move_in_boxes(Pilot pilot)
{
	int current_index = pilot->segment->index;
	int next_index = current_index + 1;	

	if (next_index == PITSTOP_POINT) {	/* andou para o ponto de abastecimento */
		sem_wait(&boxes_mutex[current_index]);		
		sem_wait(&boxes_mutex[PITSTOP_POINT]);	
		{
			pilot->in_pitstop = 1;
			pilot->pitstop_time = random_int(MIN_BOXE_TIME, MAX_BOXE_TIME);
			pilot->pitstop_delay = 1000 * pilot->pitstop_time / RATE;		
			boxes[PITSTOP_POINT]->p1 = pilot;		
			pilot->segment = boxes[PITSTOP_POINT];
			boxes[current_index]->p1 = NULL;		
		}
		sem_post(&boxes_mutex[PITSTOP_POINT]);
		sem_post(&boxes_mutex[current_index]);
	}
	else if (next_index < BOXES_SEGMENTS) { /* continua andando nos boxes */
		sem_wait(&boxes_mutex[current_index]);		
		sem_wait(&boxes_mutex[next_index]);	
		{
			try_to_move(pilot, boxes[current_index], boxes[next_index]); 
		}
		sem_post(&boxes_mutex[next_index]);
		sem_post(&boxes_mutex[current_index]);
	}
	else { /* deve sair dos boxes e voltar para pista */
		sem_wait(&boxes_mutex[current_index]);		
		sem_wait(&track_mutex[0]);	
		{
			try_to_move(pilot, boxes[current_index], track[0]);
		}
		sem_post(&track_mutex[0]);	
		sem_post(&boxes_mutex[current_index]);		
	}
} 

/**************************************************************************************************/

void try_to_move(Pilot pilot, Segment current, Segment next) 
{	
	if (next->is_double) {
		if (next->p1 == NULL) {
			next->p1 = pilot;
			move(pilot, current, next);
		}
		else if (next->p2 == NULL) {
			next->p2 = pilot;
			move(pilot, current, next);			  
		}
	}
	else {
		if (next->p1 == NULL) {
			next->p1 = pilot;
			move(pilot, current, next);									  
		}
	}
}

/**************************************************************************************************/

void move(Pilot pilot, Segment current, Segment next)
{
	pilot->segment = next;

	if (current->p1 == pilot)
		current->p1 = NULL;
	else if (current->p2 == pilot)
		current->p2 = NULL;

	update_pilot(pilot, current, next); 
}

/**************************************************************************************************/

void update_pilot(Pilot pilot, Segment current, Segment next) 
{
	if (next == track[0]) { 
  	pilot->lap++;
		if (pilot->in_boxes) { 
			pilot->in_boxes = 0;
			if (DEBUG) printf("[Piloto %d saiu da faixa dos boxes]\n", pilot->id);
		}
		else if (pilot->lap >= 2)
			pilot->fuel--;    			
	}
	else if (next == boxes[PITSTOP_POINT + 1]) {
		pilot->fuel = 1 + (n + 1) / 2;			
		pilot->has_stoped = 1;
		pilot->in_pitstop = 0;
		printf("[Piloto %d saiu do pitstop e levou %d segundos]\n", pilot->id, pilot->pitstop_time);
	}
	else if (next == boxes[0]) {
		pilot->in_boxes = 1;
		if (DEBUG) printf("[Piloto %d entrou na faixa dos boxes]\n", pilot->id);	
	}
}

/**************************************************************************************************/

void exit_track(Pilot pilot)
{		
	int current_index = pilot->segment->index;
	Segment current;
		
	sem_wait(&track_mutex[current_index]);	
	{
		current = track[current_index];	
		if (current->p1 == pilot)
			current->p1 = NULL;
		else if (current->p2 == pilot)
			current->p2 = NULL;
		pilot->is_finished = 1;
		if (DEBUG) printf("[Piloto %d finalizou a corrida.]\n", pilot->id);
	}
	sem_post(&track_mutex[current_index]);
}

/**************************************************************************************************/

void join_threads()
{
	int i; 

	for (i = 0; i < 2 * m; i++)
		pthread_join(pids[i], NULL);

	pthread_join(coordinator, NULL);
}

/**************************************************************************************************/

void show_race_result()
{
	Pilot p;
	int i, num_pilots = 2 * m;
	int pontuation, points[] = { 25, 18, 15, 12, 10, 8, 6, 4, 2, 1 };	

	qsort(pilots, num_pilots, sizeof(Pilot), order_cmp);	

	printf("\n");
	printf(LINE);
	printf("RESULTADO DA CORRIDA\n");
	printf("Lugar\tPiloto\tEquipe\tPontos\tReduz\n");
	printf(LINE);
	for (i = 0; i < num_pilots; i++) {
		p = pilots[i];
		pontuation = p->order <= 10 ? points[p->order - 1] : 0;
		printf("%d\t%d\t%d\t%d\t%c\n", p->order, p->id, p->team, pontuation, p->reduce ? 'S' : 'N');	
	}	
	printf(LINE);
}

/**************************************************************************************************/

int order_cmp(const void* a, const void* b)
{
	Pilot p1 = *((Pilot*) a); 	
	Pilot p2 = *((Pilot*) b); 	

	return p1->order - p2->order;
}

/**************************************************************************************************/

void show_championship_classification()
{
	Pilot p;
	int i, num_pilots = 2 * m;
	int points[] = { 25, 18, 15, 12, 10, 8, 6, 4, 2, 1 };	

	for (i = 0; i < num_pilots; i++) {
		p = pilots[i];
		if (p->order <= 10)
			p->points += points[p->order - 1];
	}

	qsort(pilots, num_pilots, sizeof(Pilot), points_cmp);	

	printf("\n");
	printf(LINE);
	printf("CLASSIFICAÇÃO NO CAMPEONATO\n");
	printf("Lugar\tPiloto\tEquipe\tTotal_de_Pontos\n");
	printf(LINE);
	for (i = 0; i < num_pilots; i++) {
		p = pilots[i];
		printf("%d\t%d\t%d\t%d\n", i + 1, p->id, p->team, p->points);	
	}	
	printf(LINE);
}

/**************************************************************************************************/

int points_cmp(const void* a, const void* b)
{
	Pilot p1 = *((Pilot*) a); 	
	Pilot p2 = *((Pilot*) b);
 	
	return p2->points - p1->points;
}

/**************************************************************************************************/

void show_teams_classification()
{
	Pilot p;
	Team* teams;
	int i, num_pilots = 2 * m;
	
	teams = malloc(m * sizeof(Team));
	for (i = 0; i < m; i++) {
		teams[i].id = i + 1;
		teams[i].points = 0;
	}

	for (i = 0; i < num_pilots; i++) {
		p = pilots[i];
		teams[p->team - 1].points += p->points;	
	}

	qsort(teams, m, sizeof(Team), teams_cmp);	

	printf("\n");
	printf(LINE);
	printf("CLASSIFICAÇÃO DE CONSTRUTORES\n");
	printf("Lugar\tEquipe\tPontos\n");
	printf(LINE);
	for (i = 0; i < m; i++) 
		printf("%d\t%d\t%d\n", i + 1, teams[i].id, teams[i].points);	
	printf(LINE);
	
	if (teams)
		free(teams);
}

/**************************************************************************************************/

int teams_cmp(const void* a, const void* b)
{
	Team t1 = *((Team*) a); 	
	Team t2 = *((Team*) b); 
	
	return t2.points - t1.points;
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
}

/**************************************************************************************************/

int main(int argc, char** argv)
{
	if (argc != 2) {
		printf("Uso: ep1 <arquivo_de_entrada>\n");
		return EXIT_FAILURE;
	}
	
	srand(time(NULL));

	setup_track();		
	setup_boxes();

	if (read_input_file(argv[1])) {	
		create_track_mutexes();		
		create_boxes_mutexes();
		setup_pilots();
		choose_reduced_pilots();
		setup_start_grid();
		if (DEBUG) show_pilots();		
		if (DEBUG) show_track();
		if (DEBUG) show_boxes();
	}

	create_barrier_semaphores();
	create_coordinator_thread();
	create_pilots_threads();

	if (DEBUG) printf("\n[Foi dada a largada]\n");
	start = 1;

	join_threads();

	if (DEBUG) printf("\n[Corrida finalizada]\n");
	
	show_race_result();
	show_championship_classification();
	show_teams_classification();

	clean_up();
	
	return EXIT_SUCCESS;
}
