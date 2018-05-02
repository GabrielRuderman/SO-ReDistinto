/*
 ============================================================================
 Name        : planificador.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

/*
 * Modelo ejemplo de un Cliente que envia mensajes a un Server.
 *
 * 	No se contemplan el manejo de errores en el sistema por una cuestion didactica. Tener en cuenta esto al desarrollar.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>
#include <commons/log.h>
#include <commons/config.h>
#include "../../mySocket/src/accesoConfiguracion.h"
#include "../../mySocket/src/socket.h"

void imprimir_menu() {
	int seleccion, clave, id, recurso;

	printf("\nSeleccione una operacion (1-7):\n");
	printf("\n   1. Pausar/Continuar");
	printf("\n   2. Bloquear <clave> <id>");
	printf("\n   3. Desbloquear <clave>");
	printf("\n   4. Listar <recurso>");
	printf("\n   5. Kill <ID>");
	printf("\n   6. Status <clave>");
	printf("\n   7. Deadlock");

	printf("\n\nSeleccion: ");
	scanf("%d", &seleccion);
	printf("\n");

	switch (seleccion) {
	case 1:
		printf("Pausar/Continuar ACTIVADO");
		printf("\nGracias, se esta procesando su solicitud...");
		break;

	case 2:
		printf("Bloquear ACTIVADO");
		printf("\nIngrese clave: ");
		scanf("%d", &clave);
		printf("Ingrese ID: ");
		scanf("%d", &id);
		printf("Gracias, se esta procesando su solicitud...");
		break;

	case 3:
		printf("Desbloquear ACTIVADO");
		printf("Ingrese clave: ");
		scanf("%d", &clave);
		printf("Gracias, se esta procesando su solicitud...");
		break;

	case 4:
		printf("Listar ACTIVADO");
		printf("Ingrese recurso: ");
		scanf("%d", &recurso);
		printf("Gracias, se esta procesando su solicitud...");
		break;

	case 5:
		printf("Kill ACTIVADO");
		printf("Ingrese ID: ");
		scanf("%d", &id);
		printf("Gracias, se esta procesando su solicitud...");
		break;

	case 6:
		printf("Status ACTIVADO");
		printf("\nIngrese clave: ");
		scanf("%d", &clave);
		printf("Gracias, se esta procesando su solicitud...");
		break;

	case 7:
		printf("Deadlock ACTIVADO");
		printf("\nGracias, se esta procesando su solicitud...");
		break;
	}
	printf("\n");
}

int main() {
	char* ip;
	char* port;
	int packagesize;
	bool error_config = false;

	t_log* logger = log_create("coordinador_planificador.log", "Planificador",
	true, LOG_LEVEL_INFO);

	// Importo los datos del archivo de configuracion
	t_config* config = conectarAlArchivo(logger,
			"../config_coordinador_planificador.cfg", &error_config);

	ip = obtenerCampoString(logger, config, "IP", &error_config);
	port = obtenerCampoString(logger, config, "PORT", &error_config);
	packagesize = obtenerCampoInt(logger, config, "PACKAGESIZE",
			&error_config);

	// Valido si hubo errores
	if (!error_config) {
		log_info(logger, "ENCONTRO LOS DATOS DE CONFIG !!!");
	} else {
		//return EXIT_FAILURE; // Si hubo error, se corta la ejecucion.
	}

	//int socketServidor = conectarComoCliente(logger, "127.0.0.1", port);
	//enviarMensaje(logger, socketServidor, packagesize);
	imprimir_menu();
	return EXIT_SUCCESS;
}
