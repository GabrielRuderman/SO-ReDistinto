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
#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <readline/readline.h>
#include <signal.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include "../../biblioteca-El-Rejunte/src/miAccesoConfiguracion.h"
#include "../../biblioteca-El-Rejunte/src/misSockets.h"
#include "../../biblioteca-El-Rejunte/src/miSerializador.h"
#include "../../coordinador/src/coordinador.h"

typedef struct {
	char* clave;
	char* mapa_archivo;
	int fd;
	int entrada_asociada;
	int size_valor_almacenado;
	int entradas_ocupadas;
	int ultima_referencia;
} __attribute__((packed)) t_entrada;

t_control_configuracion cargarConfiguracion();
void establecerProtocoloReemplazo();
void inicializarBloqueInstancia();
int iniciarDirectorio();
void llenarAlmacenamiento(t_entrada* entrada);
t_entrada* crearEntradaDesdeArchivo(char* archivo);
void compactarAlmacenamiento();
void actualizarEntradaAsociada(void* nodo);
void dumpAutomatico();
void dumpeoConReturnVoid(void* nodo);
int dumpearClave(t_entrada* entrada);
void imprimirTablaDeEntradas(t_list* tabla);
t_instruccion* recibirInstruccion(int socketCoorinador);
int obtenerEntradasAOcupar(char* valor);
int hayEntradasContiguas();
void escribirEntrada(t_entrada* entrada, char* valor);
void liberarEntrada(t_entrada* entrada);
int validarArgumentosInstruccion(t_instruccion* instruccion); // Creo que despues se borra esta funcion
int procesar(t_instruccion* instruccion);
int operacion_SET(t_instruccion* instruccion);
int operacion_SET_reemplazo(t_entrada* entrada, char* valor);
int operacion_STORE(char* clave);
bool comparadorClaveActual(void* nodo);
void algoritmoDeReemplazo();
bool valorEsAtomico(void* nodo);
void quitarEntrada(t_entrada* entrada_a_reemplazar);
bool comparadorClaveReemplazo(void* nodo);
t_entrada* algoritmoCircular(t_list* tabla_entradas_atomicas);
bool buscadorEntradaConPuntero();
t_entrada* algoritmoLRU(t_list* tabla_entradas_atomicas);
bool masTiempoReferenciada(void* nodo1, void* nodo2);
t_entrada* algoritmoBSU(t_list* tabla_entradas_atomicas);
bool mayorValorAlmacenado(void* nodo1, void* nodo2);
void actualizarCantidadEntradasLibres();
bool comparadorClaveInstruccion(void* nodo);
void signalHandler(int senal);
void finalizar(int cod);
void destruirEntrada(void* nodo);

#endif /* INSTANCIA_H_ */
