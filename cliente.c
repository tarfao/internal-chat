/*Cliente genérico para acesso aos servidores multiplexing, fork e threads
--> Executar como: ./cliente 5 (5 é o numero de clientes)
=====================================================================================*/

//Demais biblio

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>


void* thread_recv(void *arg);/*Cria uma thread para controlar o recebimento de mensagens*/
void* thread_send(void *arg);/*Cria uma thread para controlar o envio de mensagem*/

int main(int argc, char* argv[])
{
	int pid;
	int x, sock, resultado;
	struct sockaddr_in servidor;
	char buffer[100]; 
	char bufferin[100];/*Buffer para pegar o que o usuario digita*/
	char apelido[20];/*Apelido do usuario*/
	char IP_SERVIDOR[15]="127.0.0.1";/*localhost*/
	pthread_t thread1, thread2; /*Para criação das duas threads, recv e send*/

	servidor.sin_port = 0;
	servidor.sin_family = AF_INET;
	servidor.sin_addr.s_addr = INADDR_ANY;

	if(argc < 3)
	{
		printf("Forma de execucao: %s neymar 192.168.2.1\n",argv[0]);
		exit(1);
	}
	strcpy(apelido,argv[1]);
	strcpy(IP_SERVIDOR,argv[2]);/*Obtendo o endereço de IP do servidor caso digitou*/

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Socket");
		exit(1);
	}

	if((bind(sock, (const struct sockaddr *)&servidor, sizeof(servidor))) == -1)
	{
		perror("Bind");
		exit(1);
	}

	servidor.sin_port = htons(10000);
	servidor.sin_addr.s_addr = inet_addr(IP_SERVIDOR);

	if((connect(sock, (const struct sockaddr *)&servidor, sizeof(servidor))) == -1)
	{
		perror("Conexão");
		exit(1);
	}
	printf("Conexao estabelecida!!\n");
	printf("Apelido = %s\n",apelido);
	printf("IP = %s\n",IP_SERVIDOR);
	/*Enviando o apelido*/
	if ((send(sock, apelido, strlen(apelido), 0)) < 0)
	{
		perror("SendApelido");
		exit(1);
	}

	/*Cria a thread para receber mensagens*/
	sleep(0.5);//isso é porque essa segunda thread faz mais rápido e acaba escrevendo ">>" no terminal, 
		   //e quero que apareça somente os traços e após isso o topico.
	resultado = pthread_create(&thread1, NULL, thread_recv, (void *)&sock);
	if(resultado != 0)
	{
		printf("Nao foi possível criar a thread1");
		exit(1);
	}
	/*Cria a thread para enviar mensagens*/
	resultado = pthread_create(&thread2, NULL, thread_send, (void *)&sock);
	if(resultado != 0)
	{
		printf("Nao foi possível criar a thread2");
		exit(1);
	}

	pthread_exit(&thread2);
	pthread_exit(&thread1);

	close(sock);


	wait(NULL);
	return 0;
}

void* thread_recv(void *arg)
{
	int sock; //recebe o descritor de socket
	int recebidos;
	char buffer[100];


	sock = *(int *)arg;
	printf("=============================================================\n");
	recebidos = recv(sock,buffer,sizeof(buffer), 0);
	buffer[recebidos] = '\0';
	printf("Tópico: %s\n",buffer);
	bzero(buffer,100);
	while(recebidos = recv(sock,buffer,sizeof(buffer), 0))
	{
		if(recebidos > 0)
		{
			buffer[recebidos] = '\0';
			printf("%s\n",buffer);
			bzero(buffer,100);
		}
	}
	
}

void* thread_send(void *arg)
{
	int sock;/*Recebe o descritor do socket*/
	char buffer[100];
	int ler_dados;

	sock = *(int *)arg;
	do	
	{
		bzero(buffer, 100);
		printf(">> ");
		fgets(buffer,100,stdin);
		buffer[strlen(buffer) - 1] = '\0';/*retira o quebra linha */
		ler_dados = send(sock,buffer,strlen(buffer),0);
		if(ler_dados < 0)
		{
			perror("Sendc");
			exit(1);
		}
	}while(strcmp(buffer,"QUIT")!=0);
	sleep(2);/*Parada para fazer uma verificação, se o erro do servidor estava quando 
		o cliente para de funcionar primeiro que o servidor, e o servidor queria manter contato,
		mas como o cliente já tinha fechado a conexão ele estaria procurando por usuário que não existe*/	
	pthread_exit(NULL);
}


