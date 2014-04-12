/***************************************************************************************************
 * MAC0438 Programação Concorrente
 * EP1 Fórmula 1 - 21/04/2014
 *
 * Daniel Augusto Cortez 2960291
 * 
 * Arquivo: ep1.c
 * Compilação: gcc ep1.c -o ep1 -lpthread
 * Última atualização: 11/04/2014
 **************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> 
#include <semaphore.h> 
#include <time.h> 
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
	int id;            /* identificador do piloto */
	int team;          /* identificador da equipe */
	Segment segment;   /* identifica em qual segmento da pista o piloto está */
	int lap;           /* número da volta em que o piloto está */
	int fuel;          /* combustível disponível */
	int is_finished;   /* indica se já terminou a prova */
	int order;         /* ordem de chegada no final da prova */
	int points;        /* pontuação do piloto no campeonato */
	int reduce;        /* indica de o piloto deve reduzir velocidade no final na corrida */
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

/* Definição de uma estrutura para equipe. */
typedef struct team {
	int id;
	int points;
} Team;

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

/* Define o começo da corrida. */
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
void try_to_move(Pilot pilot);
void move(Pilot pilot, int current_index, Segment next);
void leave_position(Pilot pilot, int current_index);
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
	int i, j;

	for (i = 0; i < BOXES_SEGMENTS; i++) {
		boxes[i] = malloc(sizeof(*boxes[i]));
		j = TRACK_SEGMENTS - BOXES_SEGMENTS + i;
		boxes[i]->index = track[j]->index;
		boxes[i]->position = track[j]->position;
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

	printf("------------------------------------------\n");
	printf("PILOTOS\n");
	printf("Id\tEquipe\tPontos\tPosição\tÍndice\tR\n");
	printf("------------------------------------------\n");
	for (i = 0; i < 2 * m; i++) {
		p = pilots[i];
		s = p->segment;
		printf("%d\t%d\t%d\t%d\t%d\t%d\n", p->id, p->team, p->points, s->position, s->index, p->reduce);
	}
	printf("------------------------------------------\n");
}

/**************************************************************************************************/

void show_track()
{
	int i; 
	char d, buff1[4], buff2[4];
	Segment s;

	printf("\n");
	printf("------------------------------------------\n");
	printf("PISTA\n");
	printf("Índice\tPosição\tDuplo\tP_1\tP_2\n");
	printf("------------------------------------------\n");
	for (i = 0; i < TRACK_SEGMENTS; i++) {
		s = track[i];
		d = s->is_double ? 'S' : 'N';
		pilot_tos(s->p1, buff1);		
		pilot_tos(s->p2, buff2);	
		printf("%d\t%d\t%c\t%s\t%s\n", s->index, s->position, d, buff1, buff2);
	}
	printf("------------------------------------------\n");
}

/**************************************************************************************************/

void show_boxes()
{
	int i;
	char d, buff1[4], buff2[4];
	Segment s;

	printf("\n");
	printf("------------------------------------------\n");
	printf("BOXES\n");
	printf("Índice\tPosição\tDuplo\tP_1\tP_2\n");
	printf("------------------------------------------\n");
	for (i = 0; i < BOXES_SEGMENTS; i++) {
		s = boxes[i];
		d = s->is_double ? 'S' : 'N';
		pilot_tos(s->p1, buff1);
		pilot_tos(s->p2, buff2);
		printf("%d\t%d\t%c\t%s\t%s\n", s->index, s->position, d, buff1, buff2);
	}
	printf("------------------------------------------\n");
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
	int i, count = 0, num_pilots = 2 * m;
	int show_report = (int) (SHOW_REPORT_INTERVAL * 60.0) / (RATE / 1000.0); 	
	int all_finished, some_finished;

	while (1) {
		all_finished = 1; some_finished = 0;
		for (i = 0; i < num_pilots; i++)
			if (!pilots[i]->is_finished) {
				sem_wait(&arrive[i]);				
				all_finished = 0;								
			}
			else
				some_finished = 1;
		
		/* Neste ponto todos os pilotos completaram a iteração 
       e estão esperando para continuar. Podemos imprimir 
       os relatórios necessários pois não haverá mudança   
       nas posições. */ 

		if (++count % show_report == 0)	
				show_pilots_report(count * RATE / (1000.0 * 60.0));

		if (first_has_completed_lap())		
			show_first_three_pilots_and_update_order();		

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
	printf("------------------------------------------\n");
	printf("RELATÓRIO DOS PILOTOS (TEMPO = %3.1f')\n", time);
	printf("Piloto\tEquipe\tVolta\tPosição\tÍndice\n");
	printf("------------------------------------------\n");
	for (i = 0; i < 2 * m; i++)
		show_pilot_status(pilots[i]);	
	printf("------------------------------------------\n");
}	

/**************************************************************************************************/

void show_pilot_status(Pilot pilot)
{
	Segment s;
	
	if (pilot) {
		s = pilot->segment;
		printf("%d\t%d\t%d\t%d\t%d\n", pilot->id, pilot->team, pilot->lap, s->position, s->index);
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
	printf("------------------------------------------\n");
	printf("PRIMEIROS COLOCADOS\n");
	printf("Piloto\tEquipe\tVolta\tPosição\tÍndice\n");
	printf("------------------------------------------\n");
	show_pilot_status(first);
	show_pilot_status(second);	
	show_pilot_status(third);
	printf("------------------------------------------\n");
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
	int current_index, next_index;
	int move_turn = 1;

	while (!start);
	
	while (pilot->lap <= n) {
		if (pilot->reduce && pilot->lap > n - FINAL_LAPS)	{
			move_turn = 1 - move_turn;		
		}
		if (move_turn) {
			current_index = pilot->segment->index;
			next_index = (current_index + 1) % TRACK_SEGMENTS;		
			sem_wait(&track_mutex[current_index]);		
			sem_wait(&track_mutex[next_index]);	
			{
				try_to_move(pilot); 
			}
			sem_post(&track_mutex[next_index]);
			sem_post(&track_mutex[current_index]);
		}			
		/* Barreira de sincronizção */
		sem_post(&arrive[pilot->id - 1]);
		sem_wait(&go_on[pilot->id - 1]);		
	}
	
	if (!pilot->is_finished) {			
		exit_track(pilot);				
		/* Barreira de sincronizção */
		sem_post(&arrive[pilot->id - 1]); 
		sem_wait(&go_on[pilot->id - 1]); 		
	}

	return NULL; 
}

/**************************************************************************************************/

void try_to_move(Pilot pilot) 
{
	int current_index = pilot->segment->index;
	int next_index = (current_index + 1) % TRACK_SEGMENTS;			
	Segment next = track[next_index];
	
	if (next->is_double) {
		if (next->p1 == NULL) {
			next->p1 = pilot;
			move(pilot, current_index, next);
		}
		else if (next->p2 == NULL) {
			next->p2 = pilot;
			move(pilot, current_index, next);			  
		}
	}
	else {
		if (next->p1 == NULL) {
			next->p1 = pilot;
			move(pilot, current_index, next);									  
		}
	}
}

/**************************************************************************************************/

void move(Pilot pilot, int current_index, Segment next)
{
	pilot->segment = next;
	if (next->index == 0) 
  	pilot->lap++;
	leave_position(pilot, current_index);    	
}

/**************************************************************************************************/

void leave_position(Pilot pilot, int current_index)
{
	Segment current = track[current_index];	    			
	if (current->p1 == pilot)
		current->p1 = NULL;
	else if (current->p2 == pilot)
		current->p2 = NULL;
}

/**************************************************************************************************/

void exit_track(Pilot pilot)
{
		Segment current;		
		int current_index = pilot->segment->index;
				
		sem_wait(&track_mutex[current_index]);	
		{
			current = track[current_index];	
			if (current->p1 == pilot) {
				current->p1 = NULL;
			}	
			else if (current->p2 == pilot) {
				current->p2 = NULL;
			}
		}
		pilot->is_finished = 1;
		/* printf("[Piloto %d finalizou a corrida.]\n", pilot->id); */
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
	printf("------------------------------------------\n");
	printf("RESULTADO DA CORRIDA\n");
	printf("Lugar\tPiloto\tEquipe\tPontos\tR\n");
	printf("------------------------------------------\n");
	for (i = 0; i < num_pilots; i++) {
		p = pilots[i];
		pontuation = p->order <= 10 ? points[p->order - 1] : 0;
		printf("%d\t%d\t%d\t%d\t%d\n", p->order, p->id, p->team, pontuation, p->reduce);	
	}	
	printf("------------------------------------------\n");
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
	printf("------------------------------------------\n");
	printf("CLASSIFICAÇÃO NO CAMPEONATO\n");
	printf("Lugar\tPiloto\tEquipe\tTotal_de_Pontos\n");
	printf("------------------------------------------\n");
	for (i = 0; i < num_pilots; i++) {
		p = pilots[i];
		printf("%d\t%d\t%d\t%d\n", i + 1, p->id, p->team, p->points);	
	}	
	printf("------------------------------------------\n");
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
	int i, num_pilots = 2 * m;
	Team* teams;
	
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
	printf("------------------------------------------\n");
	printf("CLASSIFICAÇÃO DE CONSTRUTORES\n");
	printf("Lugar\tEquipe\tPontos\n");
	printf("------------------------------------------\n");
	for (i = 0; i < m; i++) 
		printf("%d\t%d\t%d\n", i + 1, teams[i].id, teams[i].points);	
	printf("------------------------------------------\n");
	
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
		show_pilots();		
		show_track();
		show_boxes();
	}

	create_barrier_semaphores();
	create_coordinator_thread();
	create_pilots_threads();

	printf("\n[Foi dada a largada]\n");
	start = 1;

	join_threads();

	printf("\n[Corrida finalizada]\n");
	
	show_race_result();
	show_championship_classification();
	show_teams_classification();

	clean_up();
	
	return EXIT_SUCCESS;
}
