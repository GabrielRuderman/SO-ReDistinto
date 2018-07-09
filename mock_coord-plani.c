/*
 ============================================================================
 Name        : mock_coordinador.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include "../../biblioteca-El-Rejunte/src/miAccesoConfiguracion.h"
#include "../../biblioteca-El-Rejunte/src/misSockets.h"
#include "../../biblioteca-El-Rejunte/src/miSerializador.h"

typedef enum {
	CONFIGURACION_OK,
	CONFIGURACION_ERROR
} t_control_configuracion;

enum handshake {
	ESI = 1,
	INSTANCIA = 2,
	PLANIFICADOR = 3
};

enum chequeo_planificador {
	SE_EJECUTA_ESI = 1,
	SE_BLOQUEA_ESI = 0
};

typedef struct {
	int id;
	int socket;
	int entradas_libres; // se actualizan a medida que la Instancia procesa
	int estado; // 1 = activa, 0 = inactiva
	t_list* claves_asignadas;
} __attribute__((packed)) t_instancia;

enum estado_instancia {
	ACTIVA,
	INACTIVA
};

typedef enum {
	EL = 0,
	LSU = 1,
	KE = 3
} t_distribucion;

t_log* logger;
t_log* logger_operaciones;
bool error_config;
char* ip;
char* port;
char* algoritmo_distribucion;
t_distribucion protocolo_distribucion;
int retardo;
uint32_t cant_entradas, tam_entradas;
t_list* tabla_instancias;
int socketDeEscucha;
int socketPlanificador;
char* clave_actual;

const uint32_t PAQUETE_OK = 1;
const int TAM_MAXIMO_CLAVE = 40;

void atenderESI(int socketESI) {
	// ---------- COORDINADOR - ESI ----------
	// ANALIZAR CONCURRENCIA CON SEMAFOROS MUTEX

	uint32_t esi_ID = 10; // le asigno ID
	send(socketESI, &esi_ID, sizeof(uint32_t), 0);
	log_info(logger, "Se ha conectado un ESI con ID: %d", esi_ID);
	//send(socketESI, &PAQUETE_OK, sizeof(uint32_t), 0);

	while(1) {
		uint32_t avanzar = 1;
		send(socketESI, &avanzar, sizeof(uint32_t), 0);
		uint32_t tam_paquete;
		recv(socketESI, &tam_paquete, sizeof(uint32_t), 0); // Recibo el header
		char* paquete = (char*) malloc(sizeof(char) * tam_paquete);
		recv(socketESI, paquete, tam_paquete, 0);
		log_info(logger, "El ESI %d me envia un paquete", esi_ID);

		sleep(retardo * 0.001); // Retardo ficticio

		log_info(logger, "Le informo al ESI %d que el paquete llego correctamente", esi_ID);
		send(socketESI, &PAQUETE_OK, sizeof(uint32_t), 0); // Envio respuesta al ESI

		// ---------- COORDINADOR - PLANIFICADOR ----------
		// Aca el Coordinador le va a mandar el paquete al Planificador
		// Esto es para consultar si puede utilizar los recursos que pide

		log_info(logger, "Le consulto al Planificador si ESI %d puede hacer uso del recurso", esi_ID);
		//send(socketPlanificador, &tam_paquete, sizeof(uint32_t), 0);
		//send(socketPlanificador, &paquete, tam_paquete, 0);

		uint32_t respuesta = 1; // le dejo 1
		//recv(socketPlanificador, &respuesta, sizeof(uint32_t), 0);

		/*if (respuesta == SE_EJECUTA_ESI) {
			log_info(logger, "El Planificador me informa que el ESI %d puede utilizar el recurso", esi_ID);
			if (procesarPaquete(paquete) == -1) { // Hay que abortar el ESI
				log_error(logger, "Se aborta el ESI %d", esi_ID);
				//send(socketESI, &ABORTAR_ESI, sizeof(uint32_t), 0);
				break;
			}
			loguearOperacion(esi_ID, paquete);

			log_info(logger, "Le aviso al ESI %d que la instruccion se ejecuto satisfactoriamente", esi_ID);
			send(socketESI, &PAQUETE_OK, sizeof(uint32_t), 0);
		}*/

		log_info(logger, "Le aviso al ESI %d que la instruccion se ejecuto satisfactoriamente", esi_ID);
		send(socketESI, &PAQUETE_OK, sizeof(uint32_t), 0);

		destruirPaquete(paquete);
	}
}

void* establecerConexion(void* socketCliente) {
	log_info(logger, "Cliente conectado");

	/* Aca se utiliza el concepto de handshake.
	 * Cada Cliente manda un identificador para avisarle al Servidor
	 * quien es el que se conecta. Esto hay que hacerlo ya que nuestro
	 * Servidor es multicliente, y a cada cliente lo atiende con un
	 * hilo distinto => para saber cada hilo que ejecutar tiene que
	 * saber con quien se esta comunicando =)
	 */

	uint32_t handshake;
	recv(*(int*) socketCliente, &handshake, sizeof(uint32_t), 0);
	if (handshake == ESI) {
		log_info(logger, "El cliente es ESI");
		atenderESI(*(int*) socketCliente);
	/*} else if (handshake == INSTANCIA) {
		log_info(logger, "El cliente es una Instancia");
		atenderInstancia(*(int*) socketCliente);*/
	} else if (handshake == PLANIFICADOR) {
		log_info(logger, "El cliente es el Planificador");
		send(*(int*) socketCliente, &PAQUETE_OK, sizeof(uint32_t), 0);
	} else {
		log_error(logger, "No se pudo reconocer al cliente");
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
	t_config* config = conectarAlArchivo(logger, "/home/utnso/workspace/tp-2018-1c-El-Rejunte/coordinador/config_coordinador.cfg", &error_config);

	ip = obtenerCampoString(logger, config, "IP", &error_config);
	port = obtenerCampoString(logger, config, "PORT", &error_config);
	algoritmo_distribucion = obtenerCampoString(logger, config, "ALGORITMO_DISTRIBUCION", &error_config);
	cant_entradas = obtenerCampoInt(logger, config, "CANT_ENTRADAS", &error_config);
	tam_entradas = obtenerCampoInt(logger, config, "TAM_ENTRADAS", &error_config);
	retardo = obtenerCampoInt(logger, config, "RETARDO", &error_config);

	//establecerProtocoloDistribucion();

	// Valido si hubo errores
	if (error_config) {
		log_error(logger, "No se pudieron obtener todos los datos correspondientes");
		return CONFIGURACION_ERROR;
	}
	return CONFIGURACION_OK;
}

int main() {
	if (cargarConfiguracion() == CONFIGURACION_ERROR) {
		log_error(logger, "No se pudo cargar la configuracion");
		//finalizar();
		return EXIT_FAILURE;
	}

	socketDeEscucha = conectarComoServidor(logger, ip, port);

	while (1) { // Infinitamente escucha a la espera de que se conecte alguien
		int socketCliente = escucharCliente(logger, socketDeEscucha);
		pthread_t unHilo; // Cada conexion la delega en un hilo
		pthread_create(&unHilo, NULL, establecerConexion, (void*) &socketCliente);
	}


	return EXIT_SUCCESS;
}
