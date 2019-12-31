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
#include "biblio.h"
#include "camada_de_ligacao.h"
#define erro -1

int N_trama_ler = 0;  		//Numero da trama a ler
int N_trama_escrever  = 0;	//Numero da trama a escrever
int trama_cont=0;			//frames enviados

//LLOPEN PRONTO
int llopen(int port, int Tx_Rx) { // Cria a ligação entre os 2 pontos

	int estado = 0, aux = 0, ciclo1 = 0, time_out_cont = 0, aux_trama = 0;
	unsigned char SET[5];
	unsigned char UA[5];
	
	unsigned char trama_recebida[trama_length ];

	//TRAMAS DE CONTROLO SET E UA DO TIPO [F A C Bcc F]
	trama_inicial(SET, FR_A_TX, FR_C_SET);
	trama_inicial(UA, FR_A_TX, FR_C_UA);

	if(Tx_Rx == TX) { //TX=1
		while (!ciclo1) {
			switch (estado) {
				case 0: 
					//Inicio da conexão - envio do SET
					tcflush(port, TCIOFLUSH); 
					aux = write(port, SET, 5);
					if(aux < 0) {  
						printf("Erro na escrita do set");
						return -1;
					}
					estado = 1;
					break;

				case 1:  
					//Receber UA
					aux_trama = ler_trama(port, trama_recebida);
					if(aux_trama==-5) return -5; // cabo com má conexão
					if(aux_trama == erro) {
						if(time_out_cont < 3) {
							printf("NENHUM UA FOI RECEBIDO + 1 time_out\n");
							sleep(1);
							time_out_cont++;
							estado = 0; //volta ao estado 0
						} else {
							printf("NENHUM UA FOI RECEBIDO - APÓS AS 3 TENTATIVAS\n");
							return -1; //ultrapassadas as 3 tentativas
						}
					} else {
						if(memcmp(trama_recebida, UA, 5) == 0) { //comparar os primeiros 5 bits da trama e do UA (se =0 é igual)
							printf("A conexão foi estabelecida.\n");
							ciclo1 = 1;
						} else {
							estado = 0; //volta ao estado 0
						}
					}
					break;

				default:
					break;
			}
		}

	} else if(Tx_Rx == RX) { //RX=0
		while(!ciclo1) {
			switch(estado) {
				case 0: 
				//RECEBER O SET
					aux_trama = ler_trama(port, trama_recebida);
					if(aux_trama==-5) return -5; // cabo com má conexão
					if(aux_trama == erro) {
						if(time_out_cont < 3) {
							printf("\nNada foi recebido no RX.\n");
							sleep(1);
							time_out_cont++;
							estado = 0;
						} else {
							printf("Nenhum SET foi recebido após as 3 tentativas\n");
							return -1; //ultrapassadas as 3 tentativas
						}
					} else {
						
						if(memcmp(trama_recebida, SET, 5) == 0) {
							estado = 1;
						}
					}
					break;

				case 1:  
					
					tcflush(port, TCIOFLUSH); 
					//ENVIO DA TRAMA UA 
					aux = write(port, UA, 5);
					if(aux < 0) { 
						printf("Erro na escrita do UA");
						return -1; 
					}
					printf("\nA conexão foi estabelecida.\n");
					ciclo1 = 1;
					break;

				default:
					break;
			}
		}
	}

	return 0;

} 
//LLREAD PRONTO
int llread(int port, unsigned char *buffer) { // Vai ser retornado o comprimento da trama sem os comprimentos dos cabeçalhos

  int estado = 0, time_out_cont = 0, i = 0, j = 0, ciclo1 = 0, aux_trama = 0;
	unsigned char RR[5], REJ[5], RR_PERDIDO[5], DISC[5];
	unsigned char trama_recebida[trama_length ]; //[F A C BCC F]=[0 1 2 3 4]
	unsigned char BCC2 = 0;

	//DISC
	trama_inicial(DISC, FR_A_TX, FR_C_DISC);

	while(!ciclo1) {
		switch(estado) {
			case 0:  
				aux_trama = ler_trama(port, trama_recebida);
				if(aux_trama==-5) return -5; // cabo com má conecção
				if(aux_trama == erro) {
					if(time_out_cont < 3) { 
						printf("Nada recebido + 1 time_out\n");
						sleep(1);
						time_out_cont++;
						estado = 6;
					} else {
						printf("Nada recebido após as 3 tentativas\n");
						return -3;
					}
				} else {
					if (trama_recebida[2] != FR_C_SET && trama_recebida[2] != FR_C_UA && trama_recebida[2] != FR_C_DISC) {
					}
					estado = 1;
				}
				break;

			case 1: 
			//Tramas de Supervisão(S) e Não Numeradas(U)
					//BCC1=A^C
				if(trama_recebida[3] != (trama_recebida[1]^trama_recebida[2])) {  
					estado = 6;  //ENVIADO rej(rejeição/ACK NEGATIVO)
					break;
				}
				estado = 2;
				break;

			case 2: 
			//Ver se recebeu um disc
				if(memcmp(trama_recebida, DISC, 5) == 0) {
					return -1;
				} else {
					estado = 3;
				}
				break;

			case 3: 
			//destuffing ; VER Transparency(guião ingles)
			//evitar o falso reconhecimento de uma flag no interior de uma trama
				for (i = 4; i < aux_trama - 1; i++) {
					if((trama_recebida[i] == ESC) && ((trama_recebida[i+1] == FR_F_AUX) || (trama_recebida[i+1] == ESC_AUX))) {
						if(trama_recebida[i+1] == FR_F_AUX) {
							trama_recebida[i] = FR_F;
						} else if(trama_recebida[i+1] == ESC_AUX) {
							trama_recebida[i] = ESC;
						}
						for (j = i + 1; j < aux_trama - 1; j++){
							trama_recebida[j] = trama_recebida[j+1];
						}
						aux_trama--;
					}
				}
				estado = 4;
				break;

			case 4: 
				BCC2 = trama_recebida[4]; //Bcc2: Tramas de Informação (I) - guião inglês COMPARAR SE É IGUAL AO CALCULO DO XOR
				for (i = 5; i < aux_trama - 2; i++) {
					BCC2 = BCC2 ^ trama_recebida[i];
				}
				if (BCC2 != trama_recebida[aux_trama-2]) {
					estado = 6; // SALTA PARA REJ
					break;
				}
				estado = 5;
				break;

			case 5:  
				if(trama_recebida[2] == FR_C_SEND0 && N_trama_ler == 0) {
					N_trama_ler = 1;
					trama_inicial(RR, FR_A_TX, FR_C_RR1);
				} else if (trama_recebida[2] == FR_C_SEND1 && N_trama_ler == 1) {
					N_trama_ler = 0;
					trama_inicial(RR, FR_A_TX, FR_C_RR0);
				} else {
					estado = 7;  
					break;
				}

				for (i = 4, j = 0; i < aux_trama - 2; i++, j++) {
					buffer[j] = trama_recebida[i];
				}
				//enviar RR - correu tudo bem
				tcflush(port, TCIOFLUSH); 
				if (write(port, RR, 5) < 0) {
					printf("Erro na escrita do RR");
					return -3;
				}
				ciclo1 = 1;
				break;

			case 6: 
			//envia rej
				if (aux_trama == erro) {
					if(N_trama_ler == 0) {
						trama_inicial(REJ, FR_A_TX, FR_C_REJ0);
					} else if (N_trama_ler == 1) {
						trama_inicial(REJ, FR_A_TX, FR_C_REJ1);
					}
				} else {
					if(trama_recebida[2] == FR_C_SEND0 && N_trama_ler == 0) {
						trama_inicial(REJ, FR_A_TX, FR_C_REJ0);
					} else if (trama_recebida[2] == FR_C_SEND1 && N_trama_ler == 1) {
						trama_inicial(REJ, FR_A_TX, FR_C_REJ1);
					}
				}

				//ENVIAR REJ - REJEIÇÃO
				tcflush(port, TCIOFLUSH); 
				if (write(port, REJ, 5) < 0) {
					printf("Erro na escrita do REJ");
					return -3;
				}
				estado = 0; 
				break;

			case 7: //RR PERDIDO / TRAMA DUPLICADA
				if (trama_recebida[2] == FR_C_SEND0 && N_trama_ler == 1) {
						trama_inicial(RR_PERDIDO, FR_A_TX, FR_C_RR1);
				} else if (trama_recebida[2] == FR_C_SEND1 && N_trama_ler == 0) {
						trama_inicial(RR_PERDIDO, FR_A_TX, FR_C_RR0);
				}

				tcflush(port, TCIOFLUSH); 
				if (write(port, RR_PERDIDO, 5) < 0) {
					printf("Trama duplicada devido a RR perdido");
					return -3;
				}
				memset(trama_recebida, 0, trama_length );  
				return -2;

			default:
				break;
		}
	}

	return (aux_trama - 6);  

} 
//LLWRITE PRONTO
int llwrite(int port, unsigned char* buffer, int length) {

	int estado = 0, aux = 0, time_out_cont = 0, ciclo1 = 0, enviado = 0, trama_I = 0;
	unsigned char RR[5], REJ[5];
	unsigned char trama_enviada[trama_length];
	unsigned char trama_recebida[trama_length];

	//PARA CADA TRAMA RR E REJ TEMOS 2 CASOS EM QUE O N=1 E N=0
	if(N_trama_escrever == 1) {
		trama_inicial(RR, FR_A_TX,FR_C_RR0);
		trama_inicial(REJ, FR_A_TX, FR_C_REJ1);
	} else if(N_trama_escrever == 0) {
		trama_inicial(RR, FR_A_TX, FR_C_RR1);
		trama_inicial(REJ, FR_A_TX, FR_C_REJ0);
	} else {
		return -1;
	}

	trama_I  = tramatipo_I(trama_enviada, buffer, length, N_trama_escrever); //criar uma trama do tipo I

	while(!ciclo1) {
		switch(estado) {
			case 0: 
				enviado = write(port, trama_enviada, trama_I);
				estado = 1;
				break;

			case 1: 
			//vê se recebe alguma coisa na porta
				aux = ler_trama(port, trama_recebida);
				if(aux==-5) return -5; // cabo com má conecção
				if(aux == erro) {
					if(time_out_cont < 3) {  
						printf("Não recebemos nada na porta");
						sleep(1);
						time_out_cont++;
						estado = 0;
					} else {  
						printf("time out excedido - 3 tentativas falhadas");
						return -1;
					}
				} else {
					trama_cont++;
					estado = 2;
				}
				break;

			case 2: 
				//rr RECEBIDO
				if(memcmp(RR, trama_recebida, 5) == 0) {
					N_trama_escrever  = 1 - N_trama_escrever ;
					ciclo1 = 1;
				} else if(memcmp(REJ, trama_recebida, 5) == 0){   //COMPARA SE É UM rej E É ENVIADA NOVAMENTE A TRAMA I
					printf("\n\nREJ RECEBIDO. TRAMA I ENVIADA NOVAMENTE\n\n"); //QUANDO HÁ ERRO - CABO DESLIGADO
					estado = 0;
				} else {  
					printf("\n\n nada recebido\n\n");
					time_out_cont++;
					estado = 0;
				}
				break;

			default:
				break;
		}
	}

	return enviado;

} 
//LLCLOSE PRONTO
int llclose(int port, int Tx_Rx) { // Fecha a logação entre os 2 pontos

	int estado = 0, aux = 0, time_out_cont = 0, ciclo1 = 0, aux_trama = 0;
	unsigned char DISC[5], UA[5];
	unsigned char trama_recebida[trama_length];

	trama_inicial(DISC, FR_A_TX, FR_C_DISC);
	trama_inicial(UA, FR_A_TX, FR_C_UA);

	if (Tx_Rx == TX) {
		while (!ciclo1) {
			switch (estado) {
				case 0: 
				//Envia o DISC
					tcflush(port, TCIOFLUSH);  
					aux = write(port, DISC, 5);
					if( aux< 0) {  
						printf("\nErro na escrita do DISC para a porta\n");
						return -1;
					}
					estado = 1;
					break;

				case 1:  
				//RECEBER DISC
					aux_trama = ler_trama(port, trama_recebida);
					if(aux_trama==-5) return -5; // cabo com má conecção
					if(aux_trama == erro) {
						if(time_out_cont < 3) {
							printf("\ndisc não foi recebido no RX\n");
							sleep(1);
							time_out_cont++;
							estado = 0;
						} else {
							printf("\ndisc não foi recebido no TX após as 3 tentativas.\n");
							return -1;
						}
					} else {
						
						if(memcmp(trama_recebida, DISC, 5) == 0) {
							estado = 2;
						} else {
							estado = 0;
						}
					}
					break;

				case 2: 
				//ENVIAR UA Sabendo que foi recebido o DISC
					tcflush(port, TCIOFLUSH);  
					aux = write(port, UA, 5);
					if(aux < 0) {  
						printf("\nUA NAO FOI ESCRITO NA PORTA\n");
						return -1;
					}
					printf("\nConexão terminada. TOP!\n");
					ciclo1 = 1;
					break;

				default:
					break;
			}
		}
		return 0;
	} else if (Tx_Rx == RX) {
		while (!ciclo1) {
			switch (estado) {
				case 0:
				//Envia o DISC
					tcflush(port, TCIOFLUSH);  
					aux = write(port, DISC, 5);
					if( aux< 0) {   
						printf("\nErro na escrita do DISC para a porta\n");
						return -1;
					}
					estado = 1;
					break;

				case 1:  
				//RECEBER UA
					aux_trama = ler_trama(port, trama_recebida);
					if(aux_trama==-5) return -5; // cabo com má conecção
					if(aux_trama == erro) {
						if(time_out_cont < 3) {
							printf("\nUA não foi recebido no RX.\n");
							sleep(1);
							time_out_cont++;
							estado = 0;
						} else {
							printf("\nUA não foi recebido no RX após esaux_tramaadas as 3 tentativas.\n");
							return -1; //ultrapassadas as 3 tentativas
						}
					} else {
						if(memcmp(trama_recebida, UA, 5) == 0) { //comparar os primeiros 5 bits da trama e do UA (se =0 é igual)
							printf("\nConexão terminada. TOP\n");
							ciclo1 = 1;
						} else {
							estado = 0;
						}
					}
					break;

				default:
					break;
			}
		}
		
	return 0;
	}

	return -1;

} 
