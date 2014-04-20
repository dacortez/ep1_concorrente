/**************************************************************************************************
 *
 * MAC0438 Programação Concorrente
 *
 * EP1 Fórmula 1 - 21/04/2014
 *
 * Daniel Augusto Cortez - 2960291
 * 
 *************************************************************************************************/

1. Introdução
-------------

	Este programa implementa uma simulação de corrida de fórmula 1 conforme as especificações 
contidas no enunciado do EP. A implementação é feita utilizando POSIX threads em C com semáforos
para proteger a manipulação de variáveis compartilhadas (seção crítica) e criar uma barreira de
sincronização.


2. Compilação
-------------

	Para compilar o EP utilize o Makefile:
	
		$> make

	Também é possível compilar o programa utilizando diratamente o gcc: 	

		$> gcc ep1.c -o ep1 -Wall -ansi -pedantic -lpthread


3. Utilização
-------------

	Para utilizar basta informar o arquivo de entrada:

		$> ./ep1 entrada.txt


4. Implementação
----------------

	A estrutura de dados básico do programa está contida nos tipos 'Segment' e 'Pilot' que 
descrevem um segmento de pista e um piloto. O código-fonte apresenta comentários descrevendo 
melhor esses tipos.

	Basicamente a simlução é implementada com uma thread para cada piloto. Uma pista foi criada 
como um vetor de 160 posições, cada posição representando um segmento de pista que pode conter
dois pilotos, dependendo se ela é dupla ou não. As iterações ocorrem a cada 500 ms, quando um 
piloto pode andar um segmento de pista, se tal segmento estiver disponível. Os segmentos são
protegidos por semáforos (com valor inicial 2 para segmento duplo e valor inicial 1 para 
segmento simples).

	Foi criada uma barreira de sincronização utilizando uma thread coordenadora com semáforos que 
gerencia o final e o começo de uma iteração para todos os pilotos. Os relatórios de saída são 
impressos entre o final das iterações de todos os pilotos e antes que se comece as próximas. 
Nesse ponto não há nenhuma movimentação na pista e podemos ter certeza das informações lidas.

	Quando um carro reduz sua velocidade pela metade, ele só deve andar a cada 2 iterações. 
Para isso, criou-se uma variável local na thread do piloto para indicar se ele deve andar ou 
não, sendo que seu valor se alterna a cada iteração do piloto.

	A entrada nos boxes é garantida para os pilotos da mesma equipe nas voltas n/2 e n/2 + 1. 
Os boxes funcionam como uma pista simples paralela aos 250 metros final da pista principal. 
A posição central dos boxes (PITSTOP_POINT) comporta até 10 carros, um para cada equipe. 
Para isso, o semaforo que protege tal posição foi iniciado com o valor 10. O carro de uma 
equipe só pode entrar nas faixas dos boxes quando seu companheiro não estiver no pitstop.


4. Relatórios
------------- 

	Relatórios são gerados a cada 15 minutos de corrida contendo a posição de todos os 
corredores. Além disso, toda vez que o primeiro colocado completa a i-ésima volta são
exibidos os 3 primeiros colocados. Toda vez que um piloto sai do pitstop, essa informação 
também é exibida com o tempo que ele gastou na parada.

	Ao final da simulação são apresentados os resultados da corrida (lugar em que os pilotos 
chegaram com os pontos que ganharam), a classificação dos pilotos no campeonato (considerando 
uma pontuação inicial aleatória para os pilotos) e a classificação final dos construtores.

	Informações adicionais podem ser exibidas durante a execução da simulação se o programa
for compilado com a variável DEBUG no código-fonte do programa. 

	Segue abaixo uma descrição mais detalhada de algumas das informações apresentados nos 
relatórios que podem gerar alguma dúvida: 

	- Índice: índice do segmento no vetor da pista (0 a 159) ou dos boxes (0 a 9).

	- Posição: posição em metros do segmento da pista ou dos boxes. 

	- P_1 / P_2: pilotos que ocupam as posições 1 e 2 de um determinado segmento da pista 
               ou dos boxes.

	- Reduz: S/N indica se o piloto reduz velocidade nas últimas 10 voltas (escolhidos 
           aleatoriamente).
		
	- Volta_Boxe: volta em que o piloto deve parar nos boxes.

	- Fuel: combustível disponível para o piloto em número de voltas.

	- Lugar: colocação do piloto na corrida em relação aos demais pilotos.



