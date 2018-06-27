/*
 * miSerializador.c
 *
 *  Created on: 21 may. 2018
 *      Author: utnso
 */

#include "miSerializador.h"

// Analizar la opcion de enviar un paquete del estilo char* = "1 clave valor" y la funcion strtok

char* empaquetarInstruccion(t_esi_operacion instruccion, t_log* logger) {
	log_info(logger, "Empaqueto la instruccion");
	char* buffer = string_new();

	switch (instruccion.keyword) {
	case GET:
		string_append(&buffer, "1-");
		string_append(&buffer, instruccion.argumentos.GET.clave);
		/*buffer = (char*) malloc(strlen("1-") + strlen(instruccion.argumentos.GET.clave) + 1);
		strcpy(buffer, "1-");
		strcpy(buffer + strlen("1-"), instruccion.argumentos.GET.clave);*/

		break;
	case SET:
		buffer = (char*) malloc(strlen("2-") + strlen(instruccion.argumentos.SET.clave) + strlen("-") + strlen(instruccion.argumentos.SET.valor) + 1);
		strcpy(buffer, "2-");
		strcpy(buffer + strlen("2-"), instruccion.argumentos.SET.clave);
		strcpy(buffer + strlen("2-") + strlen(instruccion.argumentos.SET.clave), "-");
		strcpy(buffer + strlen("2-") + strlen(instruccion.argumentos.SET.clave) + strlen("-"), instruccion.argumentos.SET.valor);

		break;
	case STORE:
		buffer = (char*) malloc(strlen("3-") + strlen(instruccion.argumentos.STORE.clave) + 1);
		strcpy(buffer, "3-");
		strcpy(buffer + strlen("3-"), instruccion.argumentos.STORE.clave);

		break;
	default:
		log_error(logger, "No se pudo empaquetar la instruccion");
		return NULL;
	}

	log_info(logger, "La instruccion fue empaquetada");
	printf("El paquete a enviar es: %s\n", buffer);

	destruir_operacion(instruccion);
	return buffer;
}

t_instruccion* desempaquetarInstruccion(char* buffer, t_log* logger) {
	log_info(logger, "Desempaqueto la instruccion");
	t_instruccion* instruccionMutada = (t_instruccion*) malloc(sizeof(t_instruccion));

	printf("El paquete recibido es: %s\n", buffer);

	char** vector_componentes_buffer = string_split(buffer, "-");

	/*
	int i = 0;
	while (vector_componentes_buffer[i] != NULL) {
		printf("%s\n", vector_componentes_buffer[i]);
		i++;
	}
	*/

	instruccionMutada->operacion = atoi(vector_componentes_buffer[0]);
	instruccionMutada->clave = vector_componentes_buffer[1];

	/*
	 * instruccionMutada->operacion == 1, GET
	 * instruccionMutada->operacion == 2, SET
	 * instruccionMutada->operacion == 3, STORE
	 */

	if (instruccionMutada->operacion == opSET) {
		instruccionMutada->valor = vector_componentes_buffer[2];
	}

	//destruirVectorComponentesBuffer(vector_componentes_buffer, instruccionMutada->operacion);

	return instruccionMutada;
}

void destruirVectorComponentesBuffer(char** vector_componentes_buffer, int operacion) {
	free(vector_componentes_buffer[0]);
	free(vector_componentes_buffer[1]);
	if (operacion == opSET) free(vector_componentes_buffer[2]);
	free(vector_componentes_buffer);
}

void destruirPaquete(void* paquete) {
	free(paquete);
}

void destruirInstruccion(t_instruccion* instruccion) {
	free(instruccion->clave);
	if (instruccion->operacion == opSET) free(instruccion->valor);
	free(instruccion);
}
