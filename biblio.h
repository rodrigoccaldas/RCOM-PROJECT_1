#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>


#define BAUDRATE B115200
#define TX 1
#define RX 0

#define TAM_BUF       65539           // k = 256 × (28 − 1) + (28 − 1) = 65535 + 4 (C, N, L2 e L1)
//tamanho_max  de uma trama: (65539 + 1) × 2 + 5 = 131085 devido a efeitos de byte stuffing e cabeçalho da camada de ligação de dados
#define trama_length  	  TAM_BUF*2+7	  //tam max trama
#define tamanho_trama		  	 2 //nome e tamanho

#define FR_F       	  0x7E   
#define FR_F_AUX 	  0x5E  	
#define ESC           0x7D  	
#define ESC_AUX       0x5D  	
#define FR_A_TX       0x03    //Comandos enviados pelo Emissor e Respostas enviadas pelo Receptor
#define FR_A_RX       0x01    //Comandos enviados pelo Receptor e Respostas enviadas pelo Emissor
#define FR_C_SET      0x03    
#define FR_C_DISC     0x0B    
#define FR_C_UA       0x07    
#define FR_C_RR0      0x05   
#define FR_C_RR1      0x85   
#define FR_C_REJ0     0x01    
#define FR_C_REJ1     0x81    
#define FR_C_SEND0    0x00    //Campo de Controlo que recebe da trama I e depois altera 0 
#define FR_C_SEND1    0x40    //Campo de Controlo que recebe da trama I e depois altera 1
#define C_START	  	  0x02		
#define C_DADOS		  0x01		
#define C_END		  0x03		

#define erro  -1


typedef struct {
	unsigned char* nome; 
	int tamanho;
} detalhes;

//SLIDE 23
typedef struct {
	unsigned char T; //Type	
	unsigned char L; //Length
	unsigned char *V; //Value
} parametros;


					//N – número de sequência (módulo 255)
					//L2 L1 –indica o número de octetos(K) do campo de dados
					//P1 ... PK – campo de dados do pacote (K octetos)

typedef struct { //pacote de dados
	unsigned char N;
	unsigned char L1;
	unsigned char L2;
	unsigned char *dados;
} data;


int ler_trama(int port, unsigned char *trama);// le a trama
void trama_inicial(unsigned char* trama, unsigned char A, unsigned char C);// forma a trama inicial -- camada data_link || introduz na trama
int tramatipo_I(unsigned char* trama, unsigned char* buff, int length, int sendNumber);// onde é feito o byte stuffing 
int construir_pacoteTVL(unsigned char C, unsigned char* pacote, parametros* tvl); //preenche pacote com o campo C, os TLVs de tvl e retorna o seu tamanho como inteiro
int construir_pacoteDados(unsigned char* buff, unsigned char* pacote, int tamanho_pacote, int* seq_num);//coloca os dados de buff, com tamanho tamanho_pacote, num pacote pacote com um número de sequência seq_num passado por referência, e incrementa-o
void pacote_tvl(unsigned char *pacote, parametros *tvl); //preenche os TLVs de tvl com os dados em pacote
void pacotedados(unsigned char *pacote, data *pacote_data); //alterar o pacote para uma estrutura pacote_data, do tipo data
int porta(char *port, struct termios *oldtio); // set da porta e retorna o seu apontador 
int getFileLength(int fd);// retorna o tamanho do ficheiro



