/*
 * instancia.h
 *
 *  Created on: 2 may. 2018
 *      Author: utnso
 */

#ifndef INSTANCIA_H_
#define INSTANCIA_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include "../../biblioteca-El-Rejunte/src/miAccesoConfiguracion.h"
#include "../../biblioteca-El-Rejunte/src/misSockets.h"
#include "../../biblioteca-El-Rejunte/src/miSerializador.h"
#include "../../coordinador/src/coordinador.h"

typedef struct {
	char* clave;
	int entrada_asociada;
	int size_valor_almacenado;
	int cantOperaciones; // cantidad de operaciones que se llevan en una entrada necesario para el algoritmo LRU
} __attribute__((packed)) t_entrada;

t_control_configuracion cargarConfiguracion();
void generarTablaDeEntradas();
void abrirArchivoInstancia(int* fileDescriptor);
void imprimirTablaDeEntradas();
t_instruccion* recibirInstruccion(int socketCoorinador);
void imprimirArgumentosInstruccion(t_instruccion* instruccion); // Creo que despues se borra esta funcion
void procesar(t_instruccion* instruccion);
void setClaveValor(t_entrada* entrada, char* valor);
void operacionStore(char* clave);
bool comparadorDeClaves(void* estructura);
void finalizar();

#endif /* INSTANCIA_H_ */
