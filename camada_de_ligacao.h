int llopen(int port, int Tx_Rx); // Cria a ligação entre os 2 pontos
int llwrite(int port, unsigned char* buffer, int length); // Vai ser retornado o comprimento da trama sem os comprimentos dos cabeçalhos
int llread(int port, unsigned char* buffer);
int llclose(int port, int Tx_Rx); // Fecha a logação entre os 2 pontos


