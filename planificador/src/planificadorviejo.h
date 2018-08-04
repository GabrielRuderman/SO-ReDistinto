/*
 * planificador.h
 *
 *  Created on: 2 may. 2018
 *      Author: utnso
 */

#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/queue.h>
#include "../../biblioteca-El-Rejunte/src/miAccesoConfiguracion.h"
#include "../../biblioteca-El-Rejunte/src/misSockets.h"

typedef struct {
	int pid; //pid_t
	int socket;
} __attribute__((packed)) t_pcb;

int imprimirMenu();
int cargarConfiguracion();
void* administrarHilosESI(void* socketDeEscucha);
void* procesarESI(void* socketESI);

#endif /* PLANIFICADOR_H_ */
