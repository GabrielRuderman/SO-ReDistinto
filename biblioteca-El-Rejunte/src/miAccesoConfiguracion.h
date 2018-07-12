/*
 * accesoConfiguracion.h
 *
 *  Created on: 1 may. 2018
 *      Author: utnso
 */

#include <stdio.h>
#include <commons/log.h>
#include <commons/config.h>

#ifndef SRC_ACCESOCONFIGURACION_H_
#define SRC_ACCESOCONFIGURACION_H_

typedef enum {
	CONFIGURACION_OK,
	CONFIGURACION_ERROR
} t_control_configuracion;

t_config* conectarAlArchivo(t_log* logger, char* path, bool* error_config);
char* obtenerCampoString(t_log* logger, t_config* config, char* campo, bool* error_config);
int obtenerCampoInt(t_log* logger, t_config* config, char* campo, bool* error_config);
void finalizarConexionArchivo(t_config* config);

#endif /* SRC_ACCESOCONFIGURACION_H_ */
