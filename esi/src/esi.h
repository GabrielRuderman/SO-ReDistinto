/*
 * esi.h
 *
 *  Created on: 2 may. 2018
 *      Author: utnso
 */

#ifndef ESI_H_
#define ESI_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <commons/log.h>
#include <commons/config.h>
#include <parsi/parser.h>
#include "../../biblioteca-El-Rejunte/src/miAccesoConfiguracion.h"
#include "../../biblioteca-El-Rejunte/src/misSockets.h"
#include "../../biblioteca-El-Rejunte/src/miSerializador.h"
#include "../../coordinador/src/coordinador.h"

t_control_configuracion cargarConfiguracion();
t_esi_operacion parsearLineaScript(FILE* fp);
void signalHandler(int senal);
void finalizar(int cod);

#endif /* ESI_H_ */
