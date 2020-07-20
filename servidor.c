// diretiva de compilação gcc arquivo_fonte.c -o arquivo_objeto -lpthread

// thread -> compartilhamento de memória
// uso pesado de memória
// usar semáfotos ou mutexes para gerenciar
// funcionamento: um processo cliente, um processo pai que gera varias threads_servidores. Cada thread
// servidora atende um cliente
// como rodar: ./servidor 3(nº de clientes)
//rodar o cliente como: ./cliente 3
#include<stdio.h>
#include<sys/ioctl.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<sched.h>
#include<pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include <sys/ioctl.h> /*Organiza a fila de conexões*/

/*estrutura usada para armazenar os dados de um cliente*/
struct cliente{
	char apelido[20];
	int ativo;/*variável que identifica se o cliente está ativo = 1, ou inativo = 0 OBS: TALVEZ NÃO PRECISE*/
	int sock;/*Obtem o descritor de um cliente*/
	int master;/*Essa variável é usada para identificar se o usuário tem poder sobre os demais, sendo 1 ou 0*/
}typedef CLIENTES;

/*Declaração de variáveis globais*/
CLIENTES cli[20];
int total=0;
char topico[50] = "VAZIO";

/*===============================Declaração de protótipo de métodos==================================*/
void* thread_proc(void *arg);

void inicial();/*Seta 0 no atributo ativo da estrutura CLIENTES*/
void adiciona_cliente(int sock);/*Adiciona um novo clinete no vetor*/
void envia_msg_para_todos(int sock, char* msg);/*Envia uma mensagem do cliente para os demais clientes, menos o próprio cliente que digitou*/
void remove_cliente(int sock);/*Remove o cliente e mostra aos usuários*/
void get_cmd(char *msgUser, char *cmd); /*Obtem o comando que o usuario digitou para a mensagem*/
void SystemMsg (int n, int sock); /*Envia uma msg de sistema para o cliente*/
int isMaster (int sock);/*Verifica se o cliente é master, retorna 1 ou 0*/
void CutMsg(char *msg, char *novaMsg, int tamCmd); /*corta as mensagens que os clientes enviam
										junto com o comando*/
int Msg_Priv (char *msg, int sock);/*Uma função para auxiliar quando há uma mensagem privada
									pois precisamos verificar quem eh o usuário que é para enviar 
									a mensagem, assim temos como atributo da função uma string contendo
									o nome do usuario e logo após a mensagem. Sucesso retorna 1, fracasso retorna 0*/
int get_user_sock(char *apelido);/*Essa função retorna o descritor do socket do usuario
								com o *apeldo especificado*/
int get_pos_sock(int sock);/*Retorna a posição de um cliente baseado no descritor do socket*/
void Altera_Master(int sock, char *apelido); /*Essa função irah fazer com que um novo usuário
					tenha poder administrativo dentro do chat. Sucesso retorna 1, fracasso retorna 0*/
int Kick_User(int sock, char *apelido); /*Remove um usuario do chat . Sucesso retorna 1, fracasso retorna 0*/
void *thread_send();
void envia_sem_formato(int sock, char *buffer);


int main(int argc, char *argv[])
{
    struct sockaddr_in servidor, cliente;
    fd_set readset, testset;
    int meu_socket, novo_socket;
    char buffer[25];
    int ouvir_socket;
    int n_lidos;
    int resultado;
    int nfilhos=5;
    pthread_t  thread_id[20], thread2;
    int total_thread = 0;
    int x, valor, i;

    //Variáveis interessantes
    

    if(argc > 1){
        nfilhos = atoi(argv[1]);
    }

    meu_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    /* SO_REUSEADDR -> durante a compilação é comum estartar e parar várias vezes o computador
    O linux tende a manter o endereço e a porta reservada para aplicação. Essa opção libera esses argumetnos.
    A função setsocketopt define as opções de argumento no nível de protocolo. O nível de argumento especifica
    o nível de protocolo onde as opções residem. Para especificar as opções no nivel socket usa-se o argumento
    SOL_SOCKET
    */
    valor = 1;
    resultado = setsockopt(meu_socket, SOL_SOCKET, SO_REUSEADDR, &valor, sizeof(valor));
    if(resultado < 0)
    {
        perror("Servidor");
        return 0;
    }

    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(10000);
    servidor.sin_addr.s_addr = INADDR_ANY;

    resultado = bind(meu_socket, (struct sockaddr *)&servidor, sizeof(servidor));

    if(resultado < 0)
    {
        perror("Bind");
        return 0;
    }

    resultado = listen(meu_socket, nfilhos);

    if(resultado < 0)
    {
        perror("Listen");
        return 0;
    }
    x = sizeof(cliente);
    printf("Agurdando Conexoes!!\n");
    while ((novo_socket = accept(meu_socket, (struct sockaddr *)&cliente, (socklen_t *)&x)))
    {
		adiciona_cliente(novo_socket);
	    	resultado = pthread_create(&thread_id[total_thread++], NULL, thread_proc, (void *)&novo_socket);
		if(resultado != 0 )
		{
		    printf("Não foi possível criar a thread!\n");
		    return 0;
		}

		resultado = pthread_create(&thread2, NULL, thread_send, NULL);
		if(resultado != 0)
		{
			printf("Nao foi possível criar a thread2");
			exit(1);
		}

    }
	
    /*for(i = 0; i < total_thread; i++ )
	{
	     if ( pthread_join(thread_id[i], NULL) == 0 )
	     {
		pthread_exit(&thread_id[i]);
		//Joga a thread da última posição na posição da thread que acabou de ser encerrada
		//e decrementa do total de threads
		thread_id[i] = thread_id[total_thread--];
	     }		
	}
    sched_yield(); // a função sched_yield() força a thread que está rodando a abandonar o processador
        // até que ela se torne novamente cabeça da lista de threads. Não leva argumentos*/
  return 0;
} // fim do main

void* thread_proc(void *arg)
{
    int ouve, sock;
    char buffer[100];/*Mensagem recebida do cliente*/
    char bufferCortado[100];/*como os dados sempre vêm com inicial de um comando
    						precisamos cortar a mensagem e deixar só os dados que 
    						nos interessa*/
    char mensagem[100];/*faz o formato de "topico: conteúdo", pois na string topico nao tem o nome incial*/
    char cmd[5]; /*guarda o comando que o usuário digitou*/
    char *alteraTopic = "ALTERACAO DE TOPICO!!!";
    int ler_dados;
    int i;

    sock = *(int *)arg;
     
    if( (send(sock, topico, strlen(topico), 0) ) < 0)
    {
	  	perror("SendTopic");
        exit(0);
    }


     while( (ler_dados = recv(sock, buffer, 100, 0)) > 0 )/*fica recebendo mensagem do cliente*/
     {
		buffer[ler_dados] = '\0';
		if(get_pos_sock(sock)>= 0)
		{
			if((strcmp(buffer,"QUIT")) == 0){//Ultima solução para desconectar foi fazer esse if else, 
											//antes estava sozinho, só o if, depois viria o if do TOPIC
				remove_cliente(sock);
			
			}else{
				/*Fazendo o controle da mensagem que o usuário enviou,
				verificando qual o comando que foi recebido*/
				get_cmd(buffer, cmd);
				bzero(bufferCortado,100);
				if( (strcmp(cmd,"TOPIC")) == 0)
				{
					if(isMaster(sock))
					{
						bzero(topico, 50);
						CutMsg(buffer, topico, 5);
						sprintf(buffer,"ALTERAÇÃO DE TOPICO PARA: %s",topico);
						envia_msg_para_todos(sock,buffer);
						SystemMsg(100, sock);
					}else{
						SystemMsg(203, sock);
					}
				}else{
					if((strcmp(cmd,"MSG")) == 0)
					{
						if(total > 1){
							CutMsg(buffer, bufferCortado, 3);
							envia_msg_para_todos(sock,bufferCortado);
							SystemMsg(100, sock);
						}else
						{
							SystemMsg(100, sock);
						}
					}else{
						if((strcmp(cmd,"PMSG")) == 0){
							CutMsg(buffer, bufferCortado, 4);
							if (Msg_Priv(bufferCortado, sock))
								SystemMsg(100, sock);
						}else{
							if((strcmp(cmd,"OP")) == 0)
							{
								if (isMaster(sock))
								{
									CutMsg(buffer, bufferCortado, 2);
									Altera_Master(sock, bufferCortado);
								}else{
									SystemMsg(203, sock);
								}
							}else{
								if((strcmp(cmd,"KICK")) == 0)
								{
									if( isMaster(sock))
									{
										CutMsg(buffer, bufferCortado, 4);
										if(Kick_User(sock, bufferCortado))
											SystemMsg(100, sock);
									}else{
										SystemMsg(203, sock);
									}
								}
							}
						}
					}
				}
			}
		}else{
			SystemMsg(500, sock);
			close(sock);
			pthread_exit(NULL);
		}
		
		/*else{
			printf("Cliente %d >> %s\n",sock,buffer);
			envia_msg_para_todos(sock, buffer);
		}*/
		bzero(buffer,100);
		__fpurge(stdin);
     }
     pthread_exit(NULL);
}

void *thread_send()
{
	char buffer[200];
	char bufferCortado[200];
	char cmd[6];
	int size;
	int i;

	do{
		printf(">> ");
		fgets(buffer,200,stdin);
		size = strlen(buffer);
		buffer[size - 1] = '\0';
		get_cmd(buffer,cmd);
		if(strcmp(cmd,"PMSG") == 0)
		{
			CutMsg(buffer, bufferCortado, 4);
			Msg_Priv(bufferCortado, 0);
		}else{
			if(strcmp(cmd,"TOPIC") == 0)
			{
				bzero(topico,50);
				CutMsg(buffer, topico, 5);
				sprintf(buffer,"ALTERAÇÃO DE TOPICO PARA: %s",topico);
				envia_sem_formato(0,buffer);
			}else{
				if((strcmp(cmd,"MSG")) == 0)
				{
					if(total > 0){
						CutMsg(buffer, bufferCortado, 3);
						envia_sem_formato(0,bufferCortado);
					}
				}else{
					if((strcmp(cmd,"KICK")) == 0)
					{
						CutMsg(buffer, bufferCortado, 4);
						Kick_User(0, bufferCortado);		
					}else{
						if((strcmp(cmd,"OP")) == 0)
						{
							CutMsg(buffer, bufferCortado, 2);
							Altera_Master(0, bufferCortado);
						}else{
							if((strcmp(cmd,"QUIT")) == 0){//Ultima solução para desconectar foi fazer esse if else, 
											//antes estava sozinho, só o if, depois viria o if do TOPIC
								envia_sem_formato(0,"SERVIDOR DESATIVADO!!!\n");
								while(total > 0 )
									remove_cliente(cli[total-1].sock);
							
							}
						}
					}
				}
			}
		}
		bzero(bufferCortado,200);
	}while((strcmp(buffer,"QUIT")) != 0);

	pthread_exit(NULL);
}

void inicial()
{
	for(int i = 0; i < 20; i++)
	{
		cli[i].ativo = 0;
	}
}

void adiciona_cliente(int sock)
{
	char apelido[20];
	char apelidoComArroba[21] = {'@'};
	int ler_dados;
	char buffer[100];
		
	cli[total].master = 0;
	if(total == 0)
		cli[total].master = 1;

	ler_dados = recv(sock, apelido, 20, 0);/*Obtem o apelido do usuário*/
	if(ler_dados < 0 )
	{
		perror("Recebendo Apelido");
	}
	apelido[ler_dados] = '\0';
	strcat(apelidoComArroba, apelido);
	sprintf(buffer,"%s CONECTADO!!\n",apelidoComArroba);
	envia_sem_formato(sock,buffer);
	cli[total].sock = sock;/*o número de */
	strcpy(cli[total].apelido, apelidoComArroba);/*Copia para a estrutura cliente o apelido da nova conexão*/
	cli[total].ativo = 1;
	total++;
}


void envia_msg_para_todos(int sock, char* msg)
{
	int i=0;
	char buffer[100];

	/*trato a mensagem para aparecer o apelido do usuário na mensagem*/
	while(cli[i].sock != sock) i++;
	sprintf(buffer,"%s diz: %s",cli[i].apelido,msg);	
	printf("%s\n",buffer);
	for(i = 0; i < total; i++)
	{
		if(cli[i].sock != sock && cli[i].ativo == 1)
		{
			if( (send(cli[i].sock, buffer, strlen(buffer), 0)) < 0)
			{
				SystemMsg(999, sock);
			}
		}
	}
}

void envia_sem_formato(int sock, char *buffer)
{
	for(int i = 0; i < total; i++)
	{
		if(cli[i].sock != sock && cli[i].ativo == 1)
		{
			if( (send(cli[i].sock, buffer, strlen(buffer), 0)) < 0)
			{
				if(sock != 0)//servidor
					SystemMsg(999, sock);	
			}
		}
	}
}

void remove_cliente(int sock)
{
	char string[50];
	char aviso[50];
	int pos;/*obtem a posicao do usuario que vai ser removido*/
	int master;/*obtem o valor do atr master do cliente para verificar se ele saiu sem dar o previlégio para alguém*/
	int novo_master;/*Obtem a posicao do novo usuario master*/

	pos = get_pos_sock(sock);
	if(pos < 0)
	{
		SystemMsg(999, sock);
	}else{
		printf("%s DESCONECTOU!!\n",cli[pos].apelido);
		sprintf(aviso,"\nO user %s/#%d se desconectou!!\n",cli[pos].apelido,cli[pos].sock);
		cli[pos].ativo = 0; /*nao estará ativo*/
		master = cli[pos].master;
		if(total > 1){		
			envia_msg_para_todos(cli[pos].sock, aviso);/*envia msg para os demais clientes informando
							//que se desconectou*/
			cli[pos] = cli[total - 1];
			if(master == 1)
			{
				novo_master = rand() % total-1;
				cli[novo_master].master = 1;
				sprintf(string,"\nNovo usuário Master = %s",cli[novo_master].apelido);
				envia_msg_para_todos(sock, string);
			}
		}
		total--;
	}	
	close(sock);
}

void get_cmd(char *msgUser, char *cmd)
{
	int i;
	for(i = 0; msgUser[i] != ' '; i++)
	{
		cmd[i] = msgUser[i];
	}
	cmd[i] = '\0';
}

void SystemMsg (int n, int sock)
{
	switch (n)
	{
		case 100://0k
			send(sock,"100 - OK",8,0);
		break;

		case 200://apelido desconhecido
			send(sock,"200 - APELIDO DESCONHECIDO",26,0);
		break;

		case 201://apelido invalido
			send(sock,"201 - APELIDO INVALIDO",22,0);
		break;

		case 202://apelido desconhecido
			send(sock,"202 - APELIDO DESCONHECIDO",26,0);
		break;

		case 203:// Negado
			send(sock,"203 - NEGADO",12,0);
		break;

		case 999:// ERRO DESCONHECIDO
			send(sock,"999 - ERRO DESCONHECIDO",23,0);
		break;
		case 500:
			send(sock,"500 - VOCÊ ESTÁ BANIDO, SAIA E ENTRE NOVAMENTE",46,0);
		break;
	}
}

int isMaster (int sock)
{
	int i;

	for(i = 0; i < total; i++)
	{
		if (cli[i].sock == sock && cli[i].master == 1)
		{
			return 1;
		}
	}
	return 0;
}

void CutMsg(char *msg, char *novaMsg, int tamCmd)
{
	int tam, i;

	tam = strlen(msg);
	for(i = tamCmd + 1; i < strlen(msg); i++)
	{
		novaMsg[i - (tamCmd + 1)] = msg[i];
	}
	novaMsg[i] = '\0';
}

int get_pos_sock(int sock)
{
	int i=0;

	for(i = 0; i < total; i ++)
	{
		if(cli[i].sock == sock && cli[i].ativo == 1)
			return i;
	}
	return -1;
}

int get_user_sock(char *apelido)
{
	int i = 0;

	for(i = 0; i < total; i++)
	{
		if((strcmp(apelido, cli[i].apelido)) == 0 && cli[i].ativo == 1)
			return cli[i].sock;
	}
	return -1;
}

int Msg_Priv (char *msg, int sock)
{
	char User[20];
	char Dados[100];
	char buffer[100];
	int sock_en;/*Obtem o descritor de socket do usuario destino*/
	int posicao;/*Obtem a posicao do descritor sock_en*/

	get_cmd(msg,User);/*como o get_cmd, devolve o comando inicial de uma string,
					também servirá para retornar o nome do usuário que é a primeira
					palavra da string*/
	sock_en = get_user_sock(User);
	if(sock_en < 0){
		SystemMsg(202, sock);
		return 0;
	}else{
		bzero(buffer, 100);
		bzero(Dados, 100);
		CutMsg(msg,Dados,strlen(User));
		strcpy(buffer,Dados);
		if(sock!=0)//esse "descritor" de socket=0 é sempre do servidor
			sprintf(buffer, "PV - %s diz: %s",cli[get_pos_sock(sock)].apelido, Dados);
		posicao = get_pos_sock(sock_en);
		printf("%s PARA: %s\n",buffer,cli[posicao].apelido);
		if((send(sock_en, buffer, strlen(buffer), 0)) < 0)
		{
			SystemMsg(999, sock);
			return 0;
		}
		return 1;
	}
}

void Altera_Master(int sock, char *apelido)
{	
	int sd;/*Obtem o descritor do socket de um cliente*/
	int pos;/*Obtem a posicao do cliente*/
	int i = 0;
	char msg[100];
	int verficador = 0; /*variável que verifica se entrou no if master == 1, dentro do for,
				já que na hora que faço a exclusão pelo usuario master, pode ser que eu 
				me auto kick, e como só vou setar a o atr ativo nesse momento, preciso verificar 
				se foi encontrado mesmo ou não alguém com */

	sd = get_user_sock(apelido);
	if(sd < 0)
	{
		SystemMsg(202, sock);

	}else{
		/*Retira o previlégio do usuário que é master*/
		for(i = 0; i < total; i++)
		{
			if(cli[i].master == 1 && cli[i].ativo == 1)
				cli[i].master = 0;
		}
		/*Atualiza para um novo usuário master*/
		pos = get_pos_sock(sd);
		cli[pos].master = 1;
		sprintf(msg,"ATENÇÃO: ALTERAÇÃO DE PREVILÉGIO.\n USUARIO MASTER = %s",cli[pos].apelido);
		envia_msg_para_todos(sock,msg);
		SystemMsg(100, sock);
	}
}

int Kick_User(int sock, char *apelido)
{
	int sd;
	int pos;
	char buffer[50];

	sd = get_user_sock(apelido);
	if(sd < 0)//se sd>=0 e sd != sock | sd < 0 || sd == sock
	{
		SystemMsg(202, sock);
		return 0;
	}else{
		if(sock != 0 && sd == sock)//caso o usuário seja master e queira kickar ele próprio. 
									//obs: sock == 0 é o servidor
		{
			SystemMsg(999, sock);
			return 0;
		}
		pos = get_pos_sock(sd);
		cli[pos].ativo = 0;
		sprintf(buffer,"\nATENÇÃO!!!!!!\n%s -> BANIDO DO CHAT\n",cli[pos].apelido);
		envia_msg_para_todos(sock, buffer);
		cli[pos] = cli[total-1];
		total--;
		return 1;
	}
}