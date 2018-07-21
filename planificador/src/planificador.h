#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_


/*
 *  ______                                _________    ________           ______      ______    _______
 * |      | |          ||     ||    |  | |          | |            ||    |      |    |      |  |       |
 * |      | |         |  |    | |   |  | |          | |           |  |   |       |  |        | |        |
 * |______| |        |    |   |  |  |  | |______    | |          |    |  |        | |        | |_______|
 * |        |        |____|   |   | |  | |          | |          |____|  |        | |        | |   \
 * |        |       |      |  |   | |  | |          | |         |      | |        | |        | |    \
 * |        |       |      |  |    ||  | |          | |         |      | |       |  |        | |     \
 * |        |______ |      |  |     |  | |          | |________ |      | |______|    |______|  |      \
 *
 *
 * by Gaspi *
 *
 */



// -------------------------------- INCLUDES --------------------------------- //


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include "../../biblioteca-El-Rejunte/src/misSockets.h"
#include <readline/readline.h>
#include <pthread.h>



// ---------------------------- CONSTANTES DE PLANIFICACION -------------------------- //

extern char * SJF;
extern char * HRRN;
extern char * SJFConDesalojo;
extern char * HRRNConDesalojo;
extern char * RUTA_CONFIGURACION ;


// ---------------------------- KEYS PARA ARCHIVO DE CONFIGURACION -----------------------//


extern char * KEY_PUERTO_CLIENTE;
extern char * KEY_ALGORITMO_PLANIFICACION ;
extern char * KEY_ESTIMACION_INICIAL ;
extern char * KEY_IP_COORDINADOR ;
extern char * KEY_PUERTO_COORDINADOR;
extern char * KEY_IP;
extern char * KEY_PUERTO;
extern char * KEY_CLAVES_BLOQUEADAS ;
extern char * KEY_CONSTANTE_ESTIMACION ;
extern char * KEY_IP_PROPIA;
extern char * KEY_PUERTO_PROPIO;
extern char * PAUSEAR_PLANIFICACION;
extern char* REANUDAR_PLANIFICACION ;
extern char* BLOQUEAR_ESI ;
extern char* DESBLOQUEAR_ESI ;
extern char* LISTAR_POR_RECURSO ;
extern char* KILL_ESI ;
extern char* STATUS_ESI ;
extern char* COMPROBAR_DEADLOCK;


// ---------------------------------- VARIABLES DE LOS SOCKETS ------------------------------- //


extern int socketDeEscucha;
char * ipPropia;
char * puertoPropio;
extern int backlog;
extern int CONTINUAR;
extern int FINALIZAR;
extern uint32_t idESI;



// ---------------------------------- GLOBALES -------------------------------------//


char * algoritmoDePlanificacion;
int rafaga;
t_config * archivoConfiguracion;
t_log * logPlanificador;
t_queue * colaListos;
t_list * listaFinalizados;
t_list * listaRecursos;
int puertoEscucha;
int alfa;
int estimacionInicial;
char * ipCoordinador;
char * puertoCoordinador;
char ** clavesBloqueadas;
extern char * rutaLog;
bool recursoGenericoEnUso;
pthread_t  hiloEscuchaConsola;
pthread_t hiloEscuchaESI;
pthread_t * hiloPlanifica;
int claveActual;
t_list * deadlockeados;
int claveParaBloquearESI;
char * claveParaBloquearRecurso;
int socketCoordinador;
extern pthread_mutex_t mutexColaListos;
extern pthread_mutex_t mutexAsesino;
extern pthread_mutex_t mutexComunicacion;
extern bool pausearPlanificacion;
extern bool matarESI;
extern int claveMatar;
extern bool bloquearESIActual;


// ------------------------------ ESTRUCTURAS -------------------------------//

typedef struct{

	uint32_t id;
	int rafagasRealizadas;
	int rafagaAnterior;
	int estimacionAnterior;
	int estimacionSiguiente;
	int tiempoEspera;
	bool bloqueadoPorUsuario;
	bool recienDesbloqueadoPorRecurso;
	bool recienDesalojado;
	t_list * recursosAsignado;
	char * recursoPedido;
	int proximaOperacion;
	bool recienLlegado;
	float tiempoRespuesta;
}ESI;


typedef struct{ // en la lista de subrecursos: futbol, basquet..

	int estado;
	char * clave;
	int operacion;
	t_queue * ESIEncolados;
	char * valor;

} t_recurso;




// ------------------------------------- FUNCIONES ----------------------------------- //

void configurar ();
void liberarGlobales ();
void estimarRafagaSiguiente(int tiempoAnterior);
ESI * crearESI(uint32_t clave);
void ESI_destroy(ESI * estructura);
void liberarRecursos(ESI * esi);
ESI* estimarProximaRafaga(ESI* proceso );
bool compararClaves (ESI * esi);
void comprobarDeadlock();
t_recurso * crearRecurso (char * id);
void crearSubrecurso (char* claveRecurso, char * claveSubrecurso);
extern void recursoDestroy(t_recurso * recurso);
extern void lanzarConsola();
extern void bloquearESI(char * claveRecurso, ESI * esi);
extern void escucharNuevosESIS();
extern void bloquearRecurso(char * claveRecurso);
extern void desbloquearRecurso(char * claveRecurso);
extern bool validarPedido (char * recurso, ESI * esi);
extern bool recursoEnLista(ESI * esi);
extern void limpiarRecienLlegados();
extern ESI * buscarESI(int clave);
extern void planificacionSJF(bool desalojo);
extern void estimarTiempos();
extern void armarColaListos(ESI * esi);
extern void planificacionHRRN(bool desalojo);
extern void estimarYCalcularTiempos(ESI * esi);
extern float calcularTiempoEspera (float espera, int estimacionSiguiente);
extern void armarCola (ESI * esi);
extern void sumarTiemposEspera ();
extern void aumentarEspera();
extern void listarBloqueados(char * clave);
extern bool encontrarVictima (ESI * esi);
extern void seekAndDestroyESI(int clave);
extern void statusClave(char * clave);
extern void cargarValor(char * clave, char * valor);
bool buscarEnBloqueados(int id);
void chequearDependenciaDeClave(char * recurso1, char* recurso2, int esi, t_list * dl);
t_recurso * traerRecurso(char * clave);
bool buscarYMatarEnCola(int clave);
bool idEnLista(t_list * lista, ESI * id);
bool ordenarESIS(void* nodo1, void* nodo2);
bool ordenarESISHRRN(void* nodo1, void* nodo2);
#endif /* PLANIFICADOR_H_ */
