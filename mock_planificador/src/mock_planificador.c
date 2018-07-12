/*
 ============================================================================
 Name        : mock_planificador.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <commons/log.h>
#include <commons/config.h>
#include "../../biblioteca-El-Rejunte/src/miAccesoConfiguracion.h"
#include "../../biblioteca-El-Rejunte/src/misSockets.h"
#include "../../biblioteca-El-Rejunte/src/miSerializador.h"
#include "../../coordinador/src/coordinador.h";

enum chequeo_planificador {
	SE_EJECUTA_ESI = 1,
	SE_BLOQUEA_ESI = 0
};

t_log* logger;
t_config* config;
bool error_config;
char* ip;
char* port;
char* ip_coordinador;
char* port_coordinador;
uint32_t socketCoordinador, socketDeEscucha;
uint32_t orden;

const uint32_t SIGUIENTE_INSTRUCCION = 1;

void* establecerConexion(void* socketESI) {
	log_info(logger, "Se ha conectado un ESI, se le esta asignando una ID...");
	// Asigno ID
	uint32_t esi_ID = *(int*) socketESI;
	send(*(int*) socketESI, &esi_ID, sizeof(uint32_t), 0);
	log_info(logger, "El ESI tiene la ID %d", esi_ID);

	uint32_t respuestaESI = 1;
	while (respuestaESI > 0) {
		send(*(int*) socketESI, &SIGUIENTE_INSTRUCCION, sizeof(uint32_t), 0);
		log_info(logger, "Le aviso al ESI que envie su proxima instruccion al Coordinador");

		t_instruccion* instruccion = desempaquetarInstruccion(recibirPaquete(*(int*) socketESI), logger);

		log_info(logger, "El Coordinador me consulta si el ESI %d puede ejecutar la instruccion", esi_ID);

		/*
		 * EVALUO SI SE PUEDE BLABLABLA... SE PUDO! =D
		 */
		orden = SE_EJECUTA_ESI;
		send(socketCoordinador, &orden, sizeof(uint32_t), 0);
		log_info(logger, "La operacion es valida, aviso al Coordinador que puede continuar");

		recv(*(int*) socketESI, &respuestaESI, sizeof(uint32_t), 0);
		log_info(logger, "El ESI me informa el resultado:");

		switch(respuestaESI) {
		case -1:
			log_error(logger, "Se aborta el ESI");
			return NULL;

		case 0:
			log_warning(logger, "El ESI termino de ejecutar");
			return NULL;

		default:
			log_warning(logger, "La instruccion fue ejecutada correctamente");
			break;
		}
	}

	return NULL;
}

t_control_configuracion cargarConfiguracion() {
	error_config = false;

	/*
	 * Se crea en la carpeta Coordinador un archivo "config_coordinador.cfg", la idea es que utilizando la
	 * biblioteca "config.h" se maneje ese CFG con el fin de que el proceso Coordinador obtenga la información
	 * necesaria para establecer su socket. El CFG contiene los campos IP, PUERTO, BACKLOG, PACKAGESIZE.
	 */

	// Importo los datos del archivo de configuracion
	t_config* config = conectarAlArchivo(logger, "/home/utnso/workspace/tp-2018-1c-El-Rejunte/mock_planificador/config_planificador.cfg", &error_config);

	ip = obtenerCampoString(logger, config, "IP", &error_config);
	port = obtenerCampoString(logger, config, "PORT", &error_config);
	ip_coordinador = obtenerCampoString(logger, config, "IP_COORDINADOR", &error_config);
	port_coordinador = obtenerCampoString(logger, config, "PORT_COORDINADOR", &error_config);

	// Valido si hubo errores
	if (error_config) {
		log_error(logger, "No se pudieron obtener todos los datos correspondientes");
		return CONFIGURACION_ERROR;
	}
	return CONFIGURACION_OK;
}

int main(void) {
	error_config = false;

	/*
	 * Se crea el logger, es una estructura a la cual se le da forma con la biblioca "log.h", me sirve para
	 * comunicar distintos tipos de mensajes que emite el S.O. como ser: WARNINGS, ERRORS, INFO.
	 */
	logger = log_create("mock_planificador.log", "Mock_Planificador", true, LOG_LEVEL_INFO);

	if (cargarConfiguracion() == CONFIGURACION_ERROR) {
		log_error(logger, "No se pudo cargar la configuracion");
		return EXIT_FAILURE;
	}

	socketCoordinador = conectarComoCliente(logger, ip_coordinador, port_coordinador);
	uint32_t handshake = PLANIFICADOR;
	send(socketCoordinador, &handshake, sizeof(uint32_t), 0);

	socketDeEscucha = conectarComoServidor(logger, ip, port);

	while (1) { // Infinitamente escucha a la espera de que se conecte alguien
		int socketCliente = escucharCliente(logger, socketDeEscucha);
		pthread_t unHilo; // Cada conexion la delega en un hilo
		pthread_create(&unHilo, NULL, establecerConexion, (void*) &socketCliente);
	}
}
