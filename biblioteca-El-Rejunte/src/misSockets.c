/*
 ============================================================================
 Name        : socket.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include "misSockets.h"

/*
 * -----------------------------------------------------------------------------------------------------------------
 *
 * 										FUNCIONES DE SOCKETS PARA UN SERVIDOR
 *
 * -----------------------------------------------------------------------------------------------------------------
 */

int conectarComoServidor(t_log* logger, const char* ip, const char* puerto,
		int backlog) {

	/*
	 *  ¿Quien soy? ¿Donde estoy? ¿Existo?
	 *
	 *  Estas y otras preguntas existenciales son resueltas getaddrinfo();
	 *
	 *  Obtiene los datos de la direccion de red y lo guarda en serverInfo.
	 *#include "misHilos.c"
	 */
	struct addrinfo hints;
	struct addrinfo *serverInfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		// No importa si uso IPv4 o IPv6
	hints.ai_flags = atoi(ip);		// Asigna el address que le envia el proceso
	hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

	getaddrinfo(ip, puerto, &hints, &serverInfo);

	/*
	 * 	Descubiertos los misterios de la vida (por lo menos, para la conexion de red actual), necesito enterarme de alguna forma
	 * 	cuales son las conexiones que quieren establecer conmigo.
	 *
	 * 	Para ello, y basandome en el postulado de que en Linux TODO es un archivo, voy a utilizar... Si, un archivo!
	 *
	 * 	Mediante socket(), obtengo el File Descriptor que me proporciona el sistema (un integer identificador).
	 *
	 */
	/* Necesitamos un socket que escuche las conecciones entrantes */
	int listenningSocket;
	listenningSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype,
			serverInfo->ai_protocol);

	/*
	 * 	Perfecto, ya tengo un archivo que puedo utilizar para analizar las conexiones entrantes. Pero... ¿Por donde?
	 *
	 * 	Necesito decirle al sistema que voy a utilizar el archivo que me proporciono para escuchar las conexiones por un puerto especifico.
	 *
	 * 				OJO! Todavia no estoy escuchando las conexiones entrantes!
	 *
	 */
	bind(listenningSocket, serverInfo->ai_addr, serverInfo->ai_addrlen);
	freeaddrinfo(serverInfo); // Ya no lo vamos a necesitar

	/*
	 * 	Ya tengo un medio de comunicacion (el socket) y le dije por que "telefono" tiene que esperar las llamadas.
	 */
	log_info(logger, "Servidor conectado.");
	return listenningSocket;
}

void* establecerComunicacion(void* parametros_sinCastear) {
	t_parametros* parametros = (t_parametros*) parametros_sinCastear;
	escucharCliente(parametros->logger, parametros->socketDeEscucha, parametros->backlog);
	return NULL;
}

int escucharCliente(t_log* logger, int listenningSocket, int backlog) {
	log_info(logger, "Listo para escuchar a cualquier Cliente...");
	listen(listenningSocket, backlog); // IMPORTANTE: listen() es una syscall BLOQUEANTE.

	/*
	 * 	El sistema esperara hasta que reciba una conexion entrante...
	 * 	...
	 * 	...
	 * 	BING!!! Nos estan llamando! ¿Y ahora?
	 *
	 *	Aceptamos la conexion entrante, y creamos un nuevo socket mediante el cual nos podamos comunicar (que no es mas que un archivo).
	 *
	 *	¿Por que crear un nuevo socket? Porque el anterior lo necesitamos para escuchar las conexiones entrantes. De la misma forma que
	 *	uno no puede estar hablando por telefono a la vez que esta esperando que lo llamen, un socket no se puede encargar de escuchar
	 *	las conexiones entrantes y ademas comunicarse con un cliente.
	 *
	 *			Nota: Para que el listenningSocket vuelva a esperar conexiones, necesitariamos volver a decirle que escuche, con listen();
	 *				En este ejemplo nos dedicamos unicamente a trabajar con el cliente y no escuchamos mas conexiones.
	 *
	 */
	struct sockaddr_in addr; // Esta estructura contendra los datos de la conexion del cliente. IP, puerto, etc.
	socklen_t addrlen = sizeof(addr);

	int socketCliente = accept(listenningSocket, (struct sockaddr *) &addr,
			&addrlen);

	return socketCliente;
}

/*
 * -----------------------------------------------------------------------------------------------------------------
 *
 * 										FUNCIONES DE SOCKETS PARA UN CLIENTE
 *
 * -----------------------------------------------------------------------------------------------------------------
 */

int conectarComoCliente(t_log* logger, const char* ip, const char* puerto) {

	/*
	 *  ¿Quien soy? ¿Donde estoy? ¿Existo?
	 *
	 *  Estas y otras preguntas existenciales son resueltas getaddrinfo();
	 *
	 *  Obtiene los datos de la direccion de red y lo guarda en serverInfo.
	 *
	 */
	struct addrinfo hints;
	struct addrinfo *serverInfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // Permite que la maquina se encargue de verificar si usamos IPv4 o IPv6
	hints.ai_socktype = SOCK_STREAM;	// Indica que usaremos el protocolo TCP

	getaddrinfo(ip, puerto, &hints, &serverInfo);// Carga en serverInfo los datos de la conexion

	/*
	 * 	Ya se quien y a donde me tengo que conectar... ¿Y ahora?
	 *	Tengo que encontrar una forma por la que conectarme al server... Ya se! Un socket!
	 *
	 * 	Obtiene un socket (un file descriptor -todo en linux es un archivo-), utilizando la estructura serverInfo que generamos antes.
	 *
	 */
	int serverSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype,
			serverInfo->ai_protocol);

	/*
	 * 	Perfecto, ya tengo el medio para conectarme (el archivo), y ya se lo pedi al sistema.
	 * 	Ahora me conecto!
	 *
	 */
	int res = connect(serverSocket, serverInfo->ai_addr,
			serverInfo->ai_addrlen);
	freeaddrinfo(serverInfo);	// No lo necesitamos mas

	if (res < 0) log_error(logger, "No me pude conectar al servidor");

	/*
	 *	Estoy conectado!
	 */
	log_info(logger, "Cliente conectado.");
	return serverSocket;
}

/*
 * -----------------------------------------------------------------------------------------------------------------
 *
 * 										FUNCIONES DE SOCKETS COMUNES
 *
 * -----------------------------------------------------------------------------------------------------------------
 */

void enviarMensaje(t_log* logger, int serverSocket, int packagesize) {
	/*	Enviar datos!
	 *
	 *	Vamos a crear un paquete (en este caso solo un conjunto de caracteres) de size PACKAGESIZE, que le enviare al servidor.
	 *
	 *	Aprovechando el standard imput/output, guardamos en el paquete las cosas que ingrese el usuario en la consola.
	 *	Ademas, contamos con la verificacion de que el usuario escriba "exit" para dejar de transmitir.
	 *
	 */
	bool enviar = true;
	char message[packagesize];

	log_info(logger, "Ya puede enviar mensajes. Escriba 'exit' para salir.");

	while (enviar) {
		fgets(message, packagesize, stdin);	// Lee una linea en el stdin (lo que escribimos en la consola) hasta encontrar un \n (y lo incluye) o llegar a PACKAGESIZE.
		if (strcmp(message, "exit\n") == 0) {
			enviar = false;			// Chequeo que el usuario no quiera salir
		}
		if (enviar)
			send(serverSocket, message, strlen(message) + 1, 0); // Solo envio si el usuario no quiere salir.
	}

	/*
	 *	Listo! Cree un medio de comunicacion con el servidor, me conecte con el y le envie cosas...
	 *
	 *	Asique ahora solo me queda cerrar la conexion con un close();
	 */
}

void recibirMensaje(t_log* logger, int socketCliente, int packagesize) {
	/*
	 * 	Ya estamos listos para recibir paquetes de nuestro cliente...
	 *
	 * 	Vamos a ESPERAR (ergo, funcion bloqueante) que nos manden los paquetes, y los imprimieremos por pantalla.
	 *
	 *	Cuando el cliente cierra la conexion, recv() devolvera 0.
	 */
	char package[packagesize];
	int status = 1;		// Estructura que maneja el status de los recieve.

	log_info(logger, "Esperando mensajes:\n");

	while (status != 0) {
		status = recv(socketCliente, (void*) package, packagesize, 0);
		if (status != 0)
			printf("%s", package);
	}
}

int finalizarSocket(int socket) { // FUNCION COMUN A SERVIDOR Y CLIENTE
	return close(socket);
}

