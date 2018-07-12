/*
 * miSerializador.h
 *
 *  Created on: 21 may. 2018
 *      Author: utnso
 */

#include <stdint.h>
#include <string.h>
#include <commons/log.h>
#include <parsi/parser.h>

#ifndef SRC_MISERIALIZADOR_H_
#define SRC_MISERIALIZADOR_H_

enum operacion {
	opGET = 1,
	opSET = 2,
	opSTORE = 3
};

typedef struct {
	uint32_t  operacion;
	char* clave;
	char* valor;
} t_instruccion;

char* empaquetarInstruccion(t_instruccion* instruccion, t_log* logger);
t_instruccion* desempaquetarInstruccion(char* paqueteSerializado, t_log* logger);
void destruirVectorComponentesBuffer(char** vector_componentes_buffer);
void destruirPaquete(char* paquete);
void destruirInstruccion(t_instruccion* instruccion);

#endif /* SRC_MISERIALIZADOR_H_ */
