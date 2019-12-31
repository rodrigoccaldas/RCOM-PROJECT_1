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
#include "biblio.h"

int ler_trama(int port, unsigned char *trama) {// le a trama da porta

	int done = 0, res = 0, b = 0, cont=0;
	unsigned char get;
	int aux=0;
	while(!done) {
		read_verificacao:	aux=read(port, &get, 1); // verifica quantos bytes foram lidos
	 	if(aux<0) return -5; // deu erro para 
		if(aux==0){ // leu zero
			cont++;
			sleep(0.01);
			if(cont==30) return -1;
			goto read_verificacao;	
		}
		//*** Nesta zona da funcao retorna um erro ou detem 1 byte ***//
		if(get == FR_F) {
			if(b == 0) {
				trama[b++] = get; 
			} else {
				if(trama[b-1] == FR_F) {
					memset(trama, 0, trama_length );
					b = 0;
					trama[b++] = FR_F;
				} else {
					trama[b++] = get;
					done = 1;
				}
			}
		} else {
			if(b > 0) {
				trama[b++] = get;
			}
		}
	}

	return b;  //retorna o tamanho da trama

} 

void trama_inicial(unsigned char *trama, unsigned char A, unsigned char C) {// forma a trama inicial -- camada data_link || introduz na trama

	trama[0] = FR_F;
	trama[1] = A;
	trama[2] = C;
	trama[3] = trama[1]^trama[2];
	trama[4] = FR_F;

} 

int tramatipo_I(unsigned char* trama, unsigned char* buff, int tamanho, int N_trama_escrever) { // onde é feito o byte stuffing 

	int b=0, i=0;
	//TRAMAS DE CONTROLO SET E UA DO TIPO [F A C Bcc F]
	unsigned char BCC2 = 0;
	//Flag inicial
	trama[b++] = FR_F; 
	//A 						
	trama[b++] = FR_A_TX;  					

	if(N_trama_escrever == 0) {   				//C
		trama[b++] = FR_C_SEND0;
	} else if(N_trama_escrever == 1) {
		trama[b++] = FR_C_SEND1;
	}

	trama[b++] = trama[1]^trama[2];    // BCC1=A^C  calculo do BCC1
			
			//Byte stuffing BCC1
	for(i = 0; i < tamanho; i++){    		
		BCC2 = BCC2 ^ buff[i];   			//BCC2=D1^D2^D3^... ^Dn  calculo do BCC2
		if(buff[i] == FR_F) {
			trama[b++] = ESC;
			trama[b++] = FR_F_AUX;
		} else if(buff[i] == ESC) {
			trama[b++] = ESC;
			trama[b++] = ESC_AUX;
		} else {
			trama[b++] = buff[i];
		}
	}
 		//Byte stuffing BCC2
	if(BCC2 == FR_F) {  					
		trama[b++] = ESC;
		trama[b++] = FR_F_AUX;
	} else if(BCC2 == ESC) {
		trama[b++] = ESC;
		trama[b++] = ESC_AUX;
	} else {
		trama[b++] = BCC2;  			 
	}
		
	trama[b] = FR_F;  						

	return b+1; // retorna o tamanho da trama
}

int construir_pacoteTVL(unsigned char C, unsigned char* pacote, parametros* tvl) { //preenche pacote com o campo C, os TLVs de tvl e retorna o seu tamanho como inteiro

	int i = 0, j = 0, b = 0;

	pacote[b++] = C;
	for(i = 0; i < tamanho_trama; i++) {
		pacote[b++] = tvl[i].T;
		pacote[b++] = tvl[i].L;
		for(j = 0; j < (int) tvl[i].L; j++) {
			pacote[b++] = tvl[i].V[j];
		}
	}

	return b;  // retorna o seu tamanho como inteiro

} 

int construir_pacoteDados(unsigned char* buff, unsigned char* pacote, int tamanho_pacote, int* seq_num) { //coloca os dados de buff, com tamanho tamanho_pacote, num pacote pacote com um número de sequência seq_num passado por referência, e incrementa-o
	//	[0 1 2 3 4]=[C N L2 L1 P(DADOS)]
	int i = 0, x = 0;
	pacote[0] = C_DADOS;                       
	pacote[1] = (char) (*seq_num)++;         
	if((*seq_num) == 256) {
		*seq_num = 0;
	}

	x = tamanho_pacote % 256;
	pacote[2] = (tamanho_pacote - x) / 256;       
	pacote[3] = x;                        

	for(i = 0; i < tamanho_pacote; i++) {
		pacote[i+4] = buff[i];         
	}
	return i+4;

}

void pacote_tvl(unsigned char *pacote, parametros *tvl) {  //preenche os TLVs de tvl com os dados em pacote

	int i = 0, j = 0, tamanho = 0, k = 0;
	while (k < tamanho_trama) {
		tvl[k].T = pacote[i];
		tvl[k].L = pacote[++i];
		tamanho = (int)(tvl[k].L);
		tvl[k].V = (unsigned char *) malloc(tamanho);
		for (j = 0; j < tamanho; j++){
			tvl[k].V[j] = pacote[++i];
		}
		i++;
		k++;
	}

}

void pacotedados(unsigned char *pacote, data *pacote_data) { //alterar o pacote para uma estrutura pacote_data, do tipo data

	int i=0;
	int j=0;

	(*pacote_data).N = pacote[0]; //N
	(*pacote_data).L2 = pacote[1]; //L2
	(*pacote_data).L1 = pacote[2]; //L1
	(*pacote_data).dados = (unsigned char*) malloc(256*(int)pacote[1] + (int)pacote[2]);
	
	
	for (j = 0, i = 3; j < 256*(int)(*pacote_data).L2 + (int)(*pacote_data).L1; j++, i++) { //DADOS
		(*pacote_data).dados[j] = pacote[i];
	}

}

int porta(char *port, struct termios *oldtio) { // set da porta e retorna o seu apontador 

	if((strcmp("/dev/ttyS0", port) != 0) && (strcmp("/dev/ttyS1", port) != 0)) {
		perror("changePort(): wrong argument for port");
		return -1;
	}

	/*
		Open serial port device for reading and writing and not as controlling tty
		because we don't want to get killed if linenoise sends CTRL-C.
	*/
	struct termios newtio;

	int fd;
	if ((fd = open(port, O_RDWR | O_NOCTTY )) < 0) {
		perror(port);
		return -1;
	}

	if ( tcgetattr(fd, oldtio) == -1) { /* save current port settings */
		perror("tcgetattr");
		return -1;
	}

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;

	/* set input mode (non-canonical, no echo,...) */
	newtio.c_lflag = 0;

	newtio.c_cc[VTIME]    = 1;   /* inter-character timer unused (estava a 0)*/
	newtio.c_cc[VMIN]     = 0;   /* blocking read until 5 chars received (estava a 5)*/

	/*
		VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
		leitura do(s) proximo(s) caracter(es)
	*/

	tcflush(fd, TCIOFLUSH);

	if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
		perror("tcsetattr");
		return -1;
	}

	printf("\nNew termios structure set\n");

	return fd; //retorna o seu apontador

}

int getFileLength(int fd) { // retorna o tamanho do ficheiro

	int length = 0;

	if((length = lseek(fd, 0, SEEK_END)) < 0) {
		perror("lseek()");
		return -1;
	}
	if(lseek(fd, 0, SEEK_SET) < 0) {
		perror("lseek()");
		return -1;
	}

	return length;

} 

