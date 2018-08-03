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
#include <ctype.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include "../../biblioteca-El-Rejunte/src/miAccesoConfiguracion.h"
#include "../../biblioteca-El-Rejunte/src/misSockets.h"
#include "../../biblioteca-El-Rejunte/src/miSerializador.h"

enum handshake {
	ESI = 1,
	INSTANCIA = 2,
	PLANIFICADOR = 3
};

typedef struct {
	uint32_t id;
	int socket;
	int entradas_libres; // se actualizan a medida que la Instancia procesa
	int rango_inicio;
	int rango_fin;
	int estado; // 1 = activa, 0 = inactiva
	t_list* claves_asignadas;
} __attribute__((packed)) t_instancia;

t_control_configuracion cargarConfiguracion();
void establecerProtocoloDistribucion();
void atenderConsola();
void establecerConexion(void* parametros);
void atenderInstancia(int socketInstancia);
void solicitarCompactacionAInstancias(int id_actual);
bool existeInstanciaID(void* nodo);
bool instanciaEstaActiva(void* nodo);
void atenderESI(int socketESI);
int procesarPaquete(char* paquete, t_instruccion* instruccion, uint32_t esi_ID);
bool instanciaTieneLaClave(void* nodo);
bool claveEsLaActual(void* nodo);
void loguearOperacion(uint32_t esi_ID, t_instruccion* instruccion);
t_instancia* algoritmoDeDistribucion();
t_instancia* algoritmoEL();
t_instancia* algoritmoLSU();
bool comparadorEntradasLibres(void* nodo1, void* nodo2);
t_instancia* algoritmoKE();
bool buscadorDeRango(void* nodo);
void finalizar(int cod);

#endif /* COORDINADOR_H_ */
