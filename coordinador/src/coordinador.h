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
#include <commons/collections/queue.h>
#include <parsi/parser.h>
#include "../../biblioteca-El-Rejunte/src/miAccesoConfiguracion.h"
#include "../../biblioteca-El-Rejunte/src/misSockets.h"
#include "../../biblioteca-El-Rejunte/src/miSerializador.h"

typedef struct {
	int tid;
	int socket;
	t_list* esis_asignados;
} __attribute__((packed)) t_tcb;

int cargarConfiguracion();
void establecerProtocoloDistribucion();
void* establecerConexion(void* parametros);
void atenderESI(int socketESI);
void loguearOperacion(uint32_t id, char* paquete);
int enviarAInstancia(char* paquete, uint32_t tam_paquete);
void atenderInstancia(int socketInstancia);
t_tcb* algoritmoDeDistribucion();
void finalizar();

#endif /* COORDINADOR_H_ */
