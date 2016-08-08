/*
 ============================================================================
 Name        : main.c
 Author      : Andres
 Version     :
 Copyright   : Your copyright notice
 Description : Envia y recibe datos por puerto serie, y los envia al proceso
 	 	 	   usado por fastCGI.
 	 	 	   Este proceso va a actuar como servidor para el socket Unix
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdbool.h>
#include <errno.h>

#define SERIAL_PORT "/dev/ttyGS0"
#define SOCKET_PATH "/tmp/fastcgi_chat.socket"


void error(const char *);

int main(void) {

	int sockFd, newsockFd, servlen;
	struct sockaddr_un cli_addr, serv_addr;
	socklen_t clilen;
	int serialFd;
	struct termios serialConf;
	int n;
	char buf[82];
	bool terminate = false;


	/* Abrimos el puerto serie*/
	if ((serialFd = open(SERIAL_PORT, O_RDWR | O_NOCTTY | O_NDELAY))<0){
		error("No se pudo abrir el Puerto Serie.\n");
	}

	/* Configuramos el puerto serie */
	tcgetattr(serialFd, &serialConf);
	serialConf.c_cflag = B115200 | CS8 | CREAD | CLOCAL;
	serialConf.c_iflag = IGNPAR | ICRNL;
	tcflush(serialFd, TCIFLUSH);
	fcntl(serialFd, F_SETFL, O_NONBLOCK); // Setiamos como non blocking al puerto serie
	tcsetattr(serialFd, TCSANOW, &serialConf);

	printf("Puerto Seria Abierto!\n");


	/* Creamos el Socket y esperamos una coneccion */
	if ((sockFd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		error("creating socket");

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, SOCKET_PATH);
	servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

	if( bind( sockFd, (struct sockaddr*)&serv_addr, servlen) < 0)
		error("binding socket");

	listen(sockFd, 5);
	clilen = sizeof(cli_addr);
	newsockFd = accept( sockFd, (struct sockaddr*)&cli_addr, &clilen);

	if(newsockFd < 0)
		error("accepting");

	fcntl(newsockFd, F_SETFL, O_NONBLOCK); // Setiamos como non blocking al socket
	printf("Una conexion fue establecida\n");


	char *tk;
	while( !terminate )
	{
		/* Leemos el puerto serie para nuevos datos */
		if( read(serialFd, &buf, 80) > 0)
		{
			strtok(buf, "\n");
			write(STDOUT_FILENO, &buf, strlen(buf)); // Lo que recibo lo saco por la consola
			write(newsockFd, buf, strlen(buf));      // y por el socket para el proceso fastCGI

			tk = strtok(buf, " \n");
			if( strcmp(tk, "exit") == 0 ) terminate = true;
			memset(buf, 0, 82);
		}
		else if ( errno != EAGAIN ) {
			terminate = true;
		}
		/* Leemos el socket*/
		if( (n = read(newsockFd,buf,80)) > 0 )
		{
			write(STDOUT_FILENO, buf, n); // Lo que recibo lo saco por la consola
			write(serialFd, buf, n);	  // y por el puerto serie

			tk = strtok(buf," \n");
			if( strcmp( tk, "exit") == 0 ) terminate = true;
			memset(buf, 0, 82);
		}
		else if ( errno != EAGAIN ) {
			terminate = true;
		}

		usleep(100*1000);
	}// while( !terminate )

	close(newsockFd);
	close(sockFd);
	remove(SOCKET_PATH);

	return 0;
}

void error(const char *msg)
{
	remove(SOCKET_PATH);
    perror(msg);
    exit(0);
}

