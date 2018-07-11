/*
 * coordinador.h
 *
 *  Created on: 2 may. 2018
 *      Author: utnso
 */

#ifndef COORDINADOR_H_
#define COORDINADOR_H_

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
	int rango_inicio;
	int rango_fin;
	int estado; // 1 = activa, 0 = inactiva
	t_list* claves_asignadas;
} __attribute__((packed)) t_instancia;

t_control_configuracion cargarConfiguracion();
void establecerProtocoloDistribucion();
void* establecerConexion(void* parametros);
void atenderInstancia(int socketInstancia);
void atenderESI(int socketESI);
int procesarPaquete(char* paquete);
bool instanciaTieneLaClave(void* nodo);
bool claveEsLaActual(void* nodo);
void loguearOperacion(uint32_t esi_ID, char* paquete);
t_instancia* algoritmoDeDistribucion();
t_instancia* algoritmoEL();
t_instancia* algoritmoLSU();
bool comparadorEntradasLibres(void* nodo1, void* nodo2);
t_instancia* algoritmoKE();
bool buscadorDeRango(void* nodo);
void finalizar();

#endif /* COORDINADOR_H_ */
