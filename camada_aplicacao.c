#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <inttypes.h>
#include <time.h>
#include "biblio.h"
#include "camada_de_ligacao.h"

#define erro -1
#define erro_duplicada -2
int trama_cont;      //nr tramas enviados

int transmissor(int fd_port, char *path) {

	unsigned char buff[TAM_BUF], pacote[TAM_BUF];
	int abrir_path, aux=0;
	int  estado=0, ciclo=0, seq_num=0, i=0;
	int count_bytes2=0, ler=0;
	int count_bytes=0;
	int trama_cont=0;
	parametros tvl[tamanho_trama];
	detalhes ficheiro;
	struct stat fileStat;
	
	while(!ciclo) {
		switch(estado) {
			case 0:
				if(stat(path, &fileStat) < 0) { //saber info sobre um ficheiro atraves do seu path
					printf("Erro no stat()");
					return -1;
				}

				if((ficheiro.nome = (unsigned char *) malloc(strlen(path)+1)) == NULL) {
					printf("erro no malloc para o nome do ficheiro");
					return -1;
				}

				strcpy((char *) ficheiro.nome, path);

				abrir_path = open(path, O_RDONLY);
				if(abrir_path<0){
					printf("ERRO NO ABRIR_PATH");
					return -1;
				}

				printf("\nABRINDO O LLOPEN() NO TX\n");
		
       			 aux = llopen(fd_port, TX);
				
				if(aux == -5) { //verificação de desconexão
					estado=0;
					break;
				}	
				if(aux < 0) {
					printf("Erro no llopen()");
					return -1;
				}
				if(aux == 0) {
					printf( "\n\nLLOPEN FUNCIONOU COM SUCESSO NO TX.\n\n");
				}

					if((ficheiro.tamanho = getFileLength(abrir_path)) < 0) {
					return -1;
				}
				
	
			case 1:
				//TVL É PARA OS PACOTES START E END
				//PACOTE START 2
				tvl[0].T = 0; //Tamanho
				tvl[0].L = sizeof(ficheiro.tamanho);
				tvl[0].V = (unsigned char *) malloc(tvl[0].L);
				
				tvl[1].T = 1; //Nome
				tvl[1].L = strlen((char *) ficheiro.nome) + 1;
				tvl[1].V = (unsigned char *) malloc((int) tvl[1].L);
				memcpy(tvl[1].V, ficheiro.nome, (int) tvl[1].L);
				
				aux = construir_pacoteTVL(C_START, pacote, tvl);
				if(aux < 1){
					printf("\n\nerro no construir_pacoteTVL\n\n");
					return -1;
				}
			
				aux = llwrite(fd_port, pacote, aux);

				if(aux == -5) { // verificação de desconexão
					estado=0;
					break;
				}

				if(aux < 0) {
					printf("\n\nerro no llwrite no pacote start\n\n");
					return -1;
				}
				estado = 2;
				break;

			case 2: //PACOTE DE DADOS 1
				while(count_bytes2 < ficheiro.tamanho) {
					
					if(count_bytes2 + TAM_BUF-4 < ficheiro.tamanho) {
						ler = TAM_BUF-4;
						count_bytes2 = count_bytes2+ler;
					} else {
						//ler o ultimo byte
						ler = ficheiro.tamanho - count_bytes2;
						count_bytes2 = count_bytes2+ler;
					}
					if((aux = read(abrir_path, buff, ler)) < 0) {
						printf("erro no read()");
						return -1;
					}

					if((aux = construir_pacoteDados(buff, pacote, aux, &seq_num)) < 0) { //coloca os dados do buff num pacote
						printf("\n\nerro no construir_pacoteDados\n\n");
						return -1;
					}
					aux = llwrite(fd_port, pacote, ler+4); //retorna número de caracteres escritos do pacote para a porta
					if(aux == -5) { // verificação de desconecção
						estado=0;
						break;
					}
					if(aux < 0) {
						printf("erro llwrite() pacote dados");
						return -1;
					} else {
						count_bytes =count_bytes + aux;
					}
					//limpar o buffer
					for(int clr = 0; clr < TAM_BUF; clr++) {
						buff[clr] = 0;
					}
				}
				estado=3;
			break;

			case 3:
			//PACOTE END 3
				aux = construir_pacoteTVL(C_END, pacote, tvl);
				if(aux < 1){
					printf("erro no construir_pacoteTVL");
					return -1;
				}

				aux = llwrite(fd_port, pacote, aux);
				if(aux == -5) { // verifivação de desconexão
					estado=0;
					break;
				}
				if(aux < 0) {
					printf("erro no llwrite pacote end");
					return -1;
				}
				estado = 4;
				break;


			break;
			//close por parte do TX
			case 4:
				printf("\n\n ABRINDO O LLCLOSE NO TX.\n");

				aux = llclose(fd_port, TX);
				if(aux == -5) { // verifivação de desconexão
					estado=0;
					break;
				}

				if(aux < 0) {
					printf("Erro no llclose() - transmissor");
					return -1;
				}
				if(aux == 0) {
					printf("\n\n LLCLOSE FUNCIONOU COM SUCESSO NO TX.\n\n");
				}

				ciclo = 1;
				break;

			default:
				break;
		}
	}
	
	return 0;
}

int recetor(int fd_port) {
	parametros tvl_start[tamanho_trama], tvl_end[tamanho_trama];
	data dados_app;
	detalhes start, end;
	unsigned char buff[TAM_BUF];
	unsigned char pacote[TAM_BUF-1];   //nao contem o campo C
	int output=0, aux=0, ciclo=0, estado=0, i=0, j=0, change=0, count_bytes=0, seq_N=255; 
	int ciclo_1=0, ciclo_2=0;

	printf( "\n\t ABRINDO LLOPEN NO RX\n\n");

	test: aux = llopen(fd_port, RX);
	if(aux == -5) { // verificação de desconexão
		estado=0;
		goto test;
	}
	if(aux < 0) {
		printf("Erro no llopen() - recetor");
		return -1;
	}
	if(aux == 0) {
		printf("\n\n LLOPEN FUNCIONOU COM SUCESSO NO RX.\n\n");
		
	}
	while (!ciclo_1) {
		switch (estado) {
			case 0: 
			//LEITURA DO PACOTE
				if (!ciclo_2) {

					aux = llread(fd_port, buff);
					if(aux == -5) { // verificação de desconexão
						estado=0;
						goto test;
						break;
					}
					if (aux == erro) { //erro =-1 que é quando no llread lê um DISC (fim)

						ciclo_1 = 1;

						break;
					} else if (aux < 0) {
						printf("erro no llread() - recetor");
						return -1;
					}
					ciclo_2 = 1;
				}
				if (ciclo_2) {
					//Escrita do que leu do buffer no pacote (no buf é excluido o campo C)
					for (i = 0; i < aux-1; i++) {
						pacote[i] = buff[i+1]; 
					}
					//SE FOR Pacote de Controlo Start
					if (buff[0] == C_START){
						pacote_tvl(pacote, tvl_start);

					} else if (buff[0] == C_DADOS) {
						ciclo_2 = 1;
						estado = 1;
						break;
					} else {
						printf("\n\nErro no campo C_Dados\n");
						return -1;
					}
					ciclo_2 = 0;
				}

				//O NOME QUE DEMOS TEM DE SE ALTERAR PARA start.file_name
				if((output = open("penguin.gif", O_CREAT | O_WRONLY, S_IROTH | S_IWOTH | S_IXOTH | S_IXGRP | S_IWGRP | S_IRGRP | S_IXUSR | S_IWUSR | S_IRUSR)) < 0) {
					printf("Erro no open do ficheiro no recetor");
					return -1;
				}
				
				break;


				case 1: 
				//Pacote de dados
				if (!ciclo_2) {

					aux = llread(fd_port, buff);
					if(aux == -5) { // verificação de desconexão
						estado=0;
						goto test;
						break;
					}
					if (aux == erro) { //Leu o DISC - Fim da leitura
						ciclo_1 = 1;
						break;
					} else if (aux == erro_duplicada) {
						break; //se receber frame repetida no llread retorna -2 (trama duplicada); ignora a leitura do duplicado; volta a ler a proxima
					} else if (aux < 0) {
						printf("erro no llread()");
						return -1;
					}
					ciclo_2 = 1;
				}

				if (ciclo_2) {
					//Leitura do pacote de Dados
					for (i = 0; i < aux-1; i++) {
						pacote[i] = buff[i+1];
					}
					if (buff[0] == C_DADOS) { //RECEBEU FLAG DE DADOS (1) - JA LEU DO BUFFER E GUARDOU NO PACOTE E VAI PREENCHER DEPOIS COM A FUNÇÃO EM BAIXO NO DADOS_APP
						
						//o tamanho recebido do llread da porta e guardado no buff é diferente do esperado 256*L2 +L1)
						if ((aux-4) != (256*(int)pacote[1] + (int)pacote[2])) {
							printf("O tamanho máximo do buffer foi ultrapassado. ERRO!!!");
						}

						
						pacotedados(pacote, &dados_app);  //alterar o pacote para uma estrutura do tipo data
					} else if (buff[0] == C_END) {
						ciclo_2 = 1;
						estado = 2; //salta para o case 2 - pacote END
						break;
					}
					//C – campo de controlo (valor: 1 – dados)
					//N – número de sequência (módulo 255)
					//L2 L1 –indica o número de octetos(K) do campo de dados
					//P1 ... PK – campo de dados do pacote (K octetos)
					
					if (((int)dados_app.N - seq_N == 1) || (seq_N - (int)dados_app.N == 255)) {
						seq_N = (int)dados_app.N; //nr de sequencia
						if (seq_N == 256) {
							seq_N = 0;
						}
					} else {
						printf("\n\nErro no número de sequencia.\n");
                        return -1;
					}
					//write(int output, const void *buf(o que vai ser escrito), size_t nbytes(256*L2 +L1));
					if ((aux = write(output, dados_app.dados, 256*(int)dados_app.L2 + (int)dados_app.L1)) < 0) {
						printf("write()");
						return -1;
					}

					for (i = 0; i < TAM_BUF; i++) {
						buff[i] = 0;
					}

					ciclo_2 = 0;
				}
				break;

				case 2: 
				//PACOTE END
				if (!ciclo_2) {
					aux = llread(fd_port, buff);
					if(aux == -5) { // verificação de desconexão
						estado=0;
						goto test;
						break;
					}
					if (aux == erro) { //Leu o DISC - Fim da leitura
						ciclo_1 = 1;
						break;
					} else if (aux < 0) {
						printf("erro no llread()");
						return -1;
					}
					ciclo_2 = 1;
				}

				if (ciclo_2) {
					//Leitura do pacote END
					for (i = 0; i < aux-1; i++) {
						pacote[i] = buff[i+1];
					}
					if (buff[0] == C_END){
						pacote_tvl(pacote, tvl_end);
					}
					ciclo_2 = 0;
				}
				break;
		}
	}

	printf( "\n\n ABRINDO LLCLOSE NO RX.\n\n");

	aux = llclose(fd_port, RX);
	if(aux == -5) { //verificação de desconexão
			estado=0;
			goto test;
	}
	if(aux < 0) {
		printf("Erro no llclose()");
		return -1;
	}
	if(aux == 0) {
		printf("\n\n LLCLOSE FUNCIONOU COM SUCESSO NO RX.\n\n");
	}
	if(close(output) < 0) {
		printf("erro no close do ficheiro");
		return -1;
	}

	return 0;
}


int main(int argc, char** argv) {

	int fd_port;
	struct termios oldtio;
	char buf[100];

	if (argc < 2) {
		printf("\n\tex: nserial /dev/ttyS0\n");
		exit(-1);
	}

	if((fd_port = porta(argv[1], &oldtio)) < 0) {
		printf("Erro na porta");
		exit(-1);
	}

	printf("RX OU TX?\n\n");
	if(fgets(buf, sizeof(buf), stdin) == 0){
		exit(-1);
	}
	if((strncmp(buf, "TX", 2) == 0)){
		if(transmissor(fd_port, argv[2]) < 0) {
			printf("Erro no transmissor");
			exit(-1);
		}
	} else if((strncmp(buf, "RX", 2) == 0)) {
		if(recetor(fd_port) < 0) {
			printf("Erro no recetor");
			exit(-1);
		}
	} else {
		printf("\nNão foi inserido correntamente TX/RX.\n");
		exit(-1);
	}

	sleep(1); 
	return 0;

} 
