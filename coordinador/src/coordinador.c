/*
============================================================================
 Name        : coordinador.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include "coordinador.h"

enum estado_instancia {
	ACTIVA = 1,
	INACTIVA = 0
};

typedef enum {
	EL = 0,
	LSU = 1,
	KE = 3
} t_distribucion;

enum chequeo_planificador {
	SE_EJECUTA_ESI = 1,
	SE_BLOQUEA_ESI = 0
};

t_log* logger;
t_log* logger_operaciones;
t_config* config;
bool error_config;
char* ip;
char* ip_planificador;
char* port;
char* port_planificador;
char* algoritmo_distribucion;
t_distribucion protocolo_distribucion;
int masEntradasLibres;
bool simulacion_activada;
int retardo;
uint32_t cant_entradas, tam_entradas;
t_list* tabla_instancias;
int socketDeEscucha;
int socketPlanificador;
int socketConsola;
char* clave_actual;
char* clave_reemplazada;
char* clave_inaccesible;
uint32_t instancia_ID;
pthread_mutex_t mutexNuevaInstancia = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexStatusClave = PTHREAD_MUTEX_INITIALIZER;

const int TAM_MAXIMO_CLAVE = 40;

const uint32_t ABORTA_ESI = -2;
const uint32_t BLOQUEA_ESI = -1;
const uint32_t PAQUETE_OK = 1;
const uint32_t PAQUETE_ERROR = -1;
const uint32_t PETICION_ESPECIAL = -4;
const uint32_t DESBLOQUEA_ESI = -3;
const uint32_t TERMINA_ESI = 0;
const uint32_t CHEQUEO_INSTANCIA_ACTIVA = 0;
const uint32_t PEDIDO_COMPACTACION = -5;

int chequearEstadoInstancia(t_instancia* instancia) {
	if (send(instancia->socket, &CHEQUEO_INSTANCIA_ACTIVA, sizeof(uint32_t), MSG_NOSIGNAL) < 1) return INACTIVA;
	uint32_t respuesta;
	if (recv(instancia->socket, &respuesta, sizeof(uint32_t), 0) < 1) return INACTIVA;
	if (respuesta != CHEQUEO_INSTANCIA_ACTIVA) return INACTIVA;
	return ACTIVA;
}

bool comparadorEntradasLibres(void* nodo1, void* nodo2) {
	t_instancia* instancia1 = (t_instancia*) nodo1;
	t_instancia* instancia2 = (t_instancia*) nodo2;
	return instancia1->entradas_libres > instancia2->entradas_libres;
}

bool igualAMasEntradasLibres(void* nodo) {
	t_instancia* instancia = (t_instancia*) nodo;
	return instancia->entradas_libres == masEntradasLibres;
}

t_instancia* algoritmoLSU() {
	list_sort(tabla_instancias, comparadorEntradasLibres);
	/*
	 * Lo que hace ahora es similar a la implementacion de EL pero el list_add lo hace en cualquier lado
	 * Esto es mejor asi, porque en promedio al disminuir la entradas_libres posteriormente si esta en
	 * la mitad de la lista entonces el ordenamiento es mas optimo
	 */
	t_instancia* instancia;
	int i = 0;
	do {
		instancia = list_get(tabla_instancias, i);
		instancia->estado = chequearEstadoInstancia(instancia);
		i++;
	} while (instancia->estado == INACTIVA);

	masEntradasLibres = instancia->entradas_libres;

	t_list* tabla_instancias_empatadas = list_filter(tabla_instancias, igualAMasEntradasLibres);
	if (list_size(tabla_instancias_empatadas) > 1) {
		log_warning(logger, "Hubo empate, utilizo el algoritmo Equitative Load");
		instancia = algoritmoEL(tabla_instancias_empatadas);
	}

	return instancia;
}

bool buscadorDeRango(void* nodo) {
	t_instancia* instancia = (t_instancia*) nodo;

	int caracter_inicial = tolower(clave_actual[0]);

	return ((instancia->estado == ACTIVA) && (instancia->rango_inicio <= caracter_inicial) && (instancia->rango_fin >= caracter_inicial));
}

t_instancia* algoritmoKE() {
	/*
	 * Distribucion de rangos en las Instancias: esta distribucion la aplico siempre
	 * ya que puede haber ingresado una nueva Instancia
	 */
	int letra_inicio = 'a'; // a
	int letra_fin = 'z'; // z

	int rango_letras = letra_fin - letra_inicio + 1; // 26 letras (sin la ñ)
	//log_trace(logger, "RANGO_LETRAS: %d", rango_letras);
	int cant_instancias = list_count_satisfying(tabla_instancias, instanciaEstaActiva);
	//log_trace(logger, "CANT_INSTANCIAS: %d", cant_instancias);

	div_t division = div(rango_letras, cant_instancias);
	int asignacion = division.quot;
	if (division.rem > 0) asignacion++;
	//log_trace(logger, "ASIGNACION: %d", asignacion);

	int i;
	t_instancia* instancia;
	int letra_actual = letra_inicio;
	for (i = 0; i < cant_instancias - 1; i++) {
		instancia = list_get(tabla_instancias, i);
		//log_trace(logger, "INSTANCIA %d", instancia->id);
		instancia->rango_inicio = letra_actual;
		//log_trace(logger, "CARACTER INICIO = %c", instancia->rango_inicio);
		instancia->rango_fin = letra_actual + asignacion - 1;
		//log_trace(logger, "CARACTER FIN: %c", instancia->rango_fin);
		letra_actual = instancia->rango_fin + 1;
	}
	instancia = list_get(tabla_instancias, i);
	instancia->rango_inicio = letra_actual;
	instancia->rango_fin = letra_fin;
	//log_trace(logger, "INSTANCIA %d", instancia->id);
	//log_trace(logger, "RANGO INICIO = %c", instancia->rango_inicio);
	//log_trace(logger, "RANGO FIN = %c", instancia->rango_fin);

	// Busco la instancia correspondiente
	t_instancia* instanciaAsignada;
	do {
		instanciaAsignada = list_find(tabla_instancias, buscadorDeRango);
		instancia->estado = chequearEstadoInstancia(instanciaAsignada);
	} while (instancia->estado == INACTIVA);
	return instanciaAsignada;
}


t_instancia* algoritmoEL(t_list* tabla) {
	t_instancia* instancia;
	int i = 0;
	do {
		if (simulacion_activada) {
			instancia = list_get(tabla, i); // Si es simulacion por STATUS CLAVE no corro la Instancia de lugar
			i++;
		} else {
			instancia = list_remove(tabla, 0);
			list_add_in_index(tabla, list_size(tabla_instancias), instancia);
		}
		/*int res = recv(instancia->socket, NULL, 0, MSG_DONTWAIT);
		if (res < 1) { // Si se desconecto me manda basura
			instancia->estado = INACTIVA;
		}*/
		instancia->estado = chequearEstadoInstancia(instancia);
	} while (instancia->estado == INACTIVA);
	return instancia;
}

t_instancia* algoritmoDeDistribucion() {
	if (simulacion_activada) {
		log_info(logger, "Aguarde mientras se simula la asignacion de una Instancia");
	} else {
		log_info(logger, "Aguarde mientras se busca una Instancia");
	}
	while (list_is_empty(tabla_instancias)) {
		if (simulacion_activada) return NULL;
		log_warning(logger, "No hay Instancias disponibles. Reintentando...");
		sleep(4); // Lo pongo para que la espera activa no sea tan densa
	}

	switch (protocolo_distribucion) {
	case LSU:
		return algoritmoLSU();

	case KE:
		return algoritmoKE();

	default: // Equitative Load
		return algoritmoEL(tabla_instancias);
	}
}

void loguearOperacion(uint32_t esi_ID, t_instruccion* instruccion) {
	log_info(logger, "Logueo la operacion en el Log de Operaciones");

	/*
	 * Log de Operaciones (Ejemplo)
	 * ESI 		Operación
	 * ESI 1 	SET materias:K3001 Fisica 2
	 * ESI 1 	STORE materias:K3001
	 * ESI 2 	SET materias:K3002 Economia
	 */

	char* cadena_log_operaciones = string_new();
	string_append(&cadena_log_operaciones, "ESI ");
	string_append_with_format(&cadena_log_operaciones, "%d", esi_ID);
	string_append(&cadena_log_operaciones, "	");
	switch (instruccion->operacion) {
	case opGET:
		string_append(&cadena_log_operaciones, "GET ");
		string_append(&cadena_log_operaciones, instruccion->clave);
		break;
	case opSET:
		string_append(&cadena_log_operaciones, "SET ");
		string_append(&cadena_log_operaciones, instruccion->clave);
		string_append(&cadena_log_operaciones, " ");
		string_append(&cadena_log_operaciones, instruccion->valor);
		break;
	case opSTORE:
		string_append(&cadena_log_operaciones, "STORE ");
		string_append(&cadena_log_operaciones, instruccion->clave);
		break;
	}
	log_info(logger_operaciones, cadena_log_operaciones);
	free(cadena_log_operaciones);
}

bool claveEsLaInaccesible(void* nodo) {
	char* clave = (char*) nodo;
	return strcmp(clave, clave_inaccesible) == 0;
}

bool claveEsLaReemplazada(void* nodo) {
	char* clave = (char*) nodo;
	return strcmp(clave, clave_reemplazada) == 0;
}

bool claveEsLaActual(void* nodo) {
	char* clave = (char*) nodo;
	return strcmp(clave, clave_actual) == 0;
}

bool instanciaTieneLaClave(void* nodo) {
	t_instancia* instancia = (t_instancia*) nodo;
	return list_any_satisfy(instancia->claves_asignadas, claveEsLaActual);
}

void solicitarCompactacionAInstancias(int ID_actual) {
	for (int i = 0; i < list_size(tabla_instancias); i++) {
		t_instancia* instancia = list_get(tabla_instancias, i);
		if (instancia->id != ID_actual) {
			instancia->estado = chequearEstadoInstancia(instancia);
			if (instancia->estado == ACTIVA) {
				send(instancia->socket, &PEDIDO_COMPACTACION, sizeof(uint32_t), 0);
			}
		}
	}
}

int procesarPaquete(char* paquete, t_instruccion* instruccion, uint32_t esi_ID) {
	if (strlen(instruccion->clave) > TAM_MAXIMO_CLAVE) {
		log_error(logger, "Error de Tamano de Clave");
		return -1;
	}

	log_info(logger, "El Coordinador esta chequeando si la clave ya existe...");

	free(clave_actual);
	clave_actual = string_new();
	string_append(&clave_actual, instruccion->clave);
	t_instancia* instancia = (t_instancia*) list_find(tabla_instancias, instanciaTieneLaClave);

	if (!instancia) {
		if (instruccion->operacion == opSTORE) {
			log_error(logger, "Error de Clave Inaccesible");
			return -1;
		}
		log_info(logger, "La clave %s no esta en ninguna Instancia", instruccion->clave);
		log_info(logger, "Escojo una Instancia segun el algoritmo %s", algoritmo_distribucion);
		instancia = algoritmoDeDistribucion();
		log_info(logger, "La Instancia sera la %d", instancia->id);
		char* nueva_clave = string_new();
		string_append(&nueva_clave, instruccion->clave);
		//nueva_clave[strlen(instruccion->clave)] = '\0';
		list_add(instancia->claves_asignadas, nueva_clave);
		log_info(logger, "La clave %s fue asignada a la Instancia %d", instruccion->clave, instancia->id);
	} else {
		log_info(logger, "La clave %s esta asignada a Instancia %d", instruccion->clave, instancia->id);
		instancia->estado = chequearEstadoInstancia(instancia);
		if (instancia->estado == INACTIVA) {
			/*
			 * Durante la ejecución del Sistema, existe la posibilidad de que una o más Instancias dejen de estar
			 * disponibles para el Coordinador de Re Distinto. Ante éstas situaciones, el Coordinador deberá ajustar
			 * su distribución de forma acorde a la cantidad de Instancias disponibles. No se deberá eliminar una
			 * clave de las tablas del coordinador si la instancia de desconectó; solo se elimina cuando un ESI
			 * intenta acceder a ella; de tal forma, si la instancia se reincorpora previo al uso de la clave;
			 * la desconexión sera transparente para el/los ESI que deseen operar con dicha clave.
			 */

			clave_inaccesible = string_new();
			string_append(&clave_inaccesible, instruccion->clave);
			list_remove_and_destroy_by_condition(instancia->claves_asignadas, claveEsLaInaccesible, free);
			free(clave_inaccesible);

			log_error(logger, "Error de Clave Inaccesible");
			return -1;
		}
	}

	log_info(logger, "Le envio a Instancia %d el paquete", instancia->id);
	uint32_t tam_paquete = strlen(paquete) + 1;
	send(instancia->socket, &tam_paquete, sizeof(uint32_t), 0);
	send(instancia->socket, paquete, tam_paquete, MSG_NOSIGNAL);

	uint32_t cant_claves_reemplazadas;
	recv(instancia->socket, &cant_claves_reemplazadas, sizeof(uint32_t), 0);

	if (cant_claves_reemplazadas == PAQUETE_ERROR) {
		log_error(logger, "La Instancia me avisa que no pudo procesar la instruccion");
		list_remove_by_condition(instancia->claves_asignadas, claveEsLaActual);
		return -1;
	} else if (cant_claves_reemplazadas == PEDIDO_COMPACTACION) {
		log_warning(logger, "La Instancia me solicita compactar, se indica a todas que lo hagan");
		send(instancia->socket, &PAQUETE_OK, sizeof(uint32_t), 0);
		solicitarCompactacionAInstancias(instancia->id);
		recv(instancia->socket, &cant_claves_reemplazadas, sizeof(uint32_t), 0);
	}

	if (cant_claves_reemplazadas > 0) {
		for (int i = 0; i < cant_claves_reemplazadas; i++) {
			uint32_t tam_clave_reemplazada;
			recv(instancia->socket, &tam_clave_reemplazada, sizeof(uint32_t), 0);
			clave_reemplazada = malloc(sizeof(char) * tam_clave_reemplazada);
			recv(instancia->socket, clave_reemplazada, tam_clave_reemplazada, 0);
			log_warning(logger, "Se informa reemplazo de la clave: %s", clave_reemplazada);

			list_remove_by_condition(instancia->claves_asignadas, claveEsLaReemplazada);

			free(clave_reemplazada);
		}
	}

	// La Instancia me devuelve la cantidad de entradas libres que tiene
	uint32_t entradas_libres;
	recv(instancia->socket, &entradas_libres, sizeof(uint32_t), 0);

	if (entradas_libres == PAQUETE_ERROR) {
		log_error(logger, "La Instancia me avisa que no pudo procesar la instruccion");
		return -1;
	}

	instancia->entradas_libres = entradas_libres;
	log_info(logger, "La Instancia %d me informa que le quedan %d entradas libres", instancia->id, entradas_libres);

	if ((instruccion->operacion == opSET) && !list_any_satisfy(instancia->claves_asignadas, claveEsLaActual)) {
		char* clave_cargada = string_new();
		string_append(&clave_cargada, instruccion->clave);
		list_add(instancia->claves_asignadas, clave_cargada);
	}

	loguearOperacion(esi_ID, instruccion);
	return 1;
}

void atenderESI(int socketESI) {
	// ---------- COORDINADOR - ESI ----------
	// ANALIZAR CONCURRENCIA CON SEMAFOROS MUTEX

	uint32_t esi_ID;
	if (recv(socketESI, &esi_ID, sizeof(uint32_t), 0) < 1) {
		log_error(logger, "El ESI no tiene permiso de conexion");
		return;
	}
	log_info(logger, "Se ha conectado un ESI con ID: %d", esi_ID);
	send(socketESI, &PAQUETE_OK, sizeof(uint32_t), 0);

	while (1) {
		bool recien_desbloqueado = false;
		uint32_t tam_paquete;
		if (recv(socketESI, &tam_paquete, sizeof(uint32_t), 0) < 1) {
			log_error(logger, "Error de Comunicacion: conexion rota con el ESI %d", esi_ID);
			return;
		}

		if (tam_paquete == DESBLOQUEA_ESI) {
			log_warning(logger, "El ESI %d se ha desbloqueado", esi_ID);
			recien_desbloqueado = true;
		} else if (tam_paquete == TERMINA_ESI) {
			log_warning(logger, "El ESI %d ha finalizado", esi_ID);
			break;
		}

		if (recien_desbloqueado) recv(socketESI, &tam_paquete, sizeof(uint32_t), 0);
		char* paquete = (char*) malloc(sizeof(char) * tam_paquete);
		recv(socketESI, paquete, tam_paquete, 0);
		log_info(logger, "El ESI %d me envia un paquete", esi_ID);
		log_debug(logger, "%s", paquete);

		sleep(retardo * 0.001); // Retardo ficticio

		log_info(logger, "Le informo al ESI %d que el paquete llego correctamente", esi_ID);
		send(socketESI, &PAQUETE_OK, sizeof(uint32_t), 0); // Envio respuesta al ESI

		// ---------- COORDINADOR - PLANIFICADOR ----------
		// Aca el Coordinador le va a mandar el paquete al Planificador
		// Esto es para consultar si puede utilizar los recursos que pide

		t_instruccion* instruccion = desempaquetarInstruccion(paquete, logger);

		uint32_t respuesta_permiso;
		if (!recien_desbloqueado) {
			log_info(logger, "Le consulto al Planificador si ESI %d puede hacer uso del recurso", esi_ID);
			uint32_t operacion = instruccion->operacion;
			send(socketPlanificador, &operacion, sizeof(uint32_t), 0);
			string_append(&(instruccion->clave), "\0");
			uint32_t tam_clave = strlen(instruccion->clave) + 1;
			send(socketPlanificador, &tam_clave, sizeof(uint32_t), 0);
			send(socketPlanificador, instruccion->clave, tam_clave, 0);

			if (operacion == opSET) {
				string_append(&(instruccion->valor), "\0");
				uint32_t tam_valor = strlen(instruccion->valor) + 1;
				send(socketPlanificador, &tam_valor, sizeof(uint32_t), 0);
				send(socketPlanificador, instruccion->valor, tam_valor, 0);
			}

			recv(socketPlanificador, &respuesta_permiso, sizeof(uint32_t), 0);
		} else {
			respuesta_permiso = SE_EJECUTA_ESI;
		}

		if (respuesta_permiso == SE_EJECUTA_ESI) {
			log_info(logger, "El Planificador me autoriza a que el ESI %d pueda utilizar el recurso", esi_ID);
			if (instruccion->operacion != opGET) {
				pthread_mutex_lock(&mutexStatusClave);
				int resultado = procesarPaquete(paquete, instruccion, esi_ID);
				pthread_mutex_unlock(&mutexStatusClave);

				if (resultado == -1) { // Hay que abortar el ESI
					log_error(logger, "Se aborta el ESI %d", esi_ID);
					send(socketESI, &ABORTA_ESI, sizeof(uint32_t), 0);
					break;
				}
			}
			log_info(logger, "Le aviso al ESI %d que la instruccion se ejecuto satisfactoriamente", esi_ID);
			send(socketESI, &PAQUETE_OK, sizeof(uint32_t), 0);
		} else if (respuesta_permiso == SE_BLOQUEA_ESI) {
			log_warning(logger, "El Planificador me informa que el ESI %d no tiene permisos y se bloquea", esi_ID);
			send(socketESI, &BLOQUEA_ESI, sizeof(uint32_t), 0);
		} else {
			log_error(logger, "El Planificador me informa que el ESI %d se aborta", esi_ID);
			send(socketESI, &ABORTA_ESI, sizeof(uint32_t), 0);
			break;
		}
		destruirPaquete(paquete);
		destruirInstruccion(instruccion);
	}
	finalizarSocket(socketESI);
}

bool instanciaEstaActiva(void* nodo) {
	t_instancia* instancia = (t_instancia*) nodo;
	return instancia->estado;
}

bool existeInstanciaID(void* nodo) {
	t_instancia* instancia = (t_instancia*) nodo;
	return (instancia->id == instancia_ID);
}

void atenderInstancia(int socketInstancia) {

	/*
	 * TODO: cuando una Instancia INACTIVA se conecta, necesita saber cuales claves tenia asignadas
	 * Si lee su archivo montaje, seguramente se va a encontrar con claves que ya habian sido reemplazadas
	 * Entonces, el Coordinador debe informarle a la Instancia ACTIVADA cuáles claves tenía!
	 * Cada vez que la Instancia termina de ejecutar deberá enviar al Coordinador aviso sobre si hubo
	 * o no reemplazo de una clave, y si es asi cual clave se reemplazo.
	 * Con este dato, el Coordinador podra mantener actualizado en la Tabla de Entradas las claves actuales
	 * y su cantidad para cada Instancia :) :) :) :)
	 * PREGUNTA: si al Coordinador le llega un GET de una clave que ya había sido asignada, pero se reemplazo,
	 * entonces se asigna nuevamente a la Instancia que mande el algoritmo de distribucion o se debe reasignar
	 * a la Instancia original? RTA: ALGORITMO DE DISTRIBUCION OTRA VEZ!
	 */

	// Recibo la ID
	pthread_mutex_lock(&mutexNuevaInstancia);

	recv(socketInstancia, &instancia_ID, sizeof(uint32_t), 0);
	log_info(logger, "Es la Instancia %d", instancia_ID);

	log_info(logger, "Busco si ya fue creada en la Tabla de Instancias");
	t_instancia* instancia = list_find(tabla_instancias, existeInstanciaID);

	pthread_mutex_unlock(&mutexNuevaInstancia);

	if (instancia) {
		instancia->estado = chequearEstadoInstancia(instancia);

		if (instancia->estado == ACTIVA) {
			log_error(logger, "No puede estar iniciada 2 veces la misma Instancia, se aborta la ultima");
			finalizarSocket(socketInstancia);
			return;
		}
		log_info(logger, "La Instancia %d ya existia, la pongo ACTIVA", instancia_ID);
		instancia->socket = socketInstancia;
		instancia->estado = ACTIVA;
	} else {
		// Guarda el struct de la Instancia en mi lista
		instancia = (t_instancia*) malloc(sizeof(t_instancia));
		instancia->id = instancia_ID;
		instancia->socket = socketInstancia;
		instancia->entradas_libres = cant_entradas;
		instancia->estado = ACTIVA;
		instancia->claves_asignadas = list_create();

		list_add(tabla_instancias, instancia);
		log_info(logger, "Instancia %d agregada a la Tabla de Instancias", instancia_ID);
	}

	log_info(logger, "Envio a la Instancia su cantidad de entradas");
	send(socketInstancia, &cant_entradas, sizeof(uint32_t), 0);

	log_info(logger, "Envio a la Instancia el tamaño de las entradas");
	send(socketInstancia, &tam_entradas, sizeof(uint32_t), 0);

	uint32_t cant_claves_cargadas = list_size(instancia->claves_asignadas);
	send(socketInstancia, &cant_claves_cargadas, sizeof(uint32_t), 0);
	for (int i = 0; i < list_size(instancia->claves_asignadas); i++) {
		char* clave_cargada = list_get(instancia->claves_asignadas, i);
		uint32_t tam_clave_cargada = strlen(clave_cargada) + 1;
		send(socketInstancia, &tam_clave_cargada, sizeof(uint32_t), 0);
		send(socketInstancia, clave_cargada, tam_clave_cargada, 0);
	}

	log_debug(logger, "La cantidad de instancias actual es %d", list_count_satisfying(tabla_instancias, instanciaEstaActiva));
}

void atenderConsola() {
	socketConsola = conectarComoCliente(logger, ip_planificador, port_planificador);
	if (socketConsola < 0) {
		log_error(logger, "No se puede atender la Consola");
		return;
	}
	/*
	 * Instancia actual en la cual se encuentra la clave. (En caso de que la clave no se encuentre en una instancia,
	 * no se debe mostrar este valor)
	 * Instancia en la cual se guardaría actualmente la clave (Calcular este valor mediante el algoritmo de distribución,
	 * pero sin afectar la distribución actual de las claves).
	 */

	while (1) {
		uint32_t tam_clave;
		int res1 = recv(socketConsola, &tam_clave, sizeof(uint32_t), 0);

		pthread_mutex_lock(&mutexStatusClave);

		free(clave_actual);
		clave_actual = malloc(sizeof(char) * tam_clave);
		int res2 = recv(socketConsola, clave_actual, sizeof(char) * tam_clave, 0);

		if (res1 < 1 || res2 < 1) {
			log_error(logger, "Error de Comunicacion: conexion con el Planificador rota");
			finalizarSocket(socketConsola);
			break;
		}

		log_debug(logger, "STATUS CLAVE de %s", clave_actual);

		t_instancia* instancia_posta = (t_instancia*) list_find(tabla_instancias, instanciaTieneLaClave); // Instancia actual
		if (!instancia_posta) {
			log_info(logger, "La clave %s no tiene una Instancia asignada", clave_actual);
			send(socketConsola, &PAQUETE_ERROR, sizeof(uint32_t), 0);
		} else {
			log_info(logger, "La clave %s corresponde a la Instancia %d", clave_actual, instancia_posta->id);
			send(socketConsola, &(instancia_posta->id), sizeof(uint32_t), 0);
		}
		simulacion_activada = true;
		t_instancia* instancia_simulada = algoritmoDeDistribucion(); // Instancia simulada
		if (!instancia_posta) {
			log_warning(logger, "No hay Instancias conectadas");
			send(socketConsola, &PAQUETE_ERROR, sizeof(uint32_t), 0);
		} else {
			log_info(logger, "Si se simula el algoritmo %s, la clave %s seria asignada a la Instancia %d", algoritmo_distribucion, clave_actual, instancia_simulada->id);
			send(socketConsola, &(instancia_simulada->id), sizeof(uint32_t), 0);
		}
		simulacion_activada = false;
		send(socketConsola, &(instancia_simulada->id), sizeof(uint32_t), 0);
		pthread_mutex_unlock(&mutexStatusClave);
	}
	pthread_mutex_unlock(&mutexStatusClave);
}

void establecerConexion(void* socketCliente) {
	log_info(logger, "Cliente conectado");

	/* Aca se utiliza el concepto de handshake.
	 * Cada Cliente manda un identificador para avisarle al Servidor
	 * quien es el que se conecta. Esto hay que hacerlo ya que nuestro
	 * Servidor es multicliente, y a cada cliente lo atiende con un
	 * hilo distinto => para saber cada hilo que ejecutar tiene que
	 * saber con quien se esta comunicando =)
	 */

	uint32_t handshake;
	recv(*(int*) socketCliente, &handshake, sizeof(uint32_t), 0);
	if (handshake == ESI) {
		log_info(logger, "El cliente es ESI");
		atenderESI(*(int*) socketCliente);
	} else if (handshake == INSTANCIA) {
		log_info(logger, "El cliente es una Instancia");
		atenderInstancia(*(int*) socketCliente);
	} else if (handshake == PLANIFICADOR) {
		log_info(logger, "El cliente es el Planificador");
		socketPlanificador = *(int*) socketCliente;
		send(socketPlanificador, &PAQUETE_OK, sizeof(uint32_t), 0);

		// Me conecto como cliente al Planificador para las peticiones de consola
		pthread_t hiloConsola;
		pthread_create(&hiloConsola, NULL, (void*) atenderConsola, NULL);
	} else {
		log_error(logger, "No se pudo reconocer al cliente");
	}
}

// Protocolo numerico de ALGORITMO_DISTRIBUCION
void establecerProtocoloDistribucion() {
	if (strcmp(algoritmo_distribucion, "LSU") == 0) {
		protocolo_distribucion = LSU;
	} else if (strcmp(algoritmo_distribucion, "KE") == 0) {
		protocolo_distribucion = KE;
	} else {
		protocolo_distribucion = EL;
	}
}

t_control_configuracion cargarConfiguracion() {
	error_config = false;

	/*
	 * Se crea en la carpeta Coordinador un archivo "config_coordinador.cfg", la idea es que utilizando la
	 * biblioteca "config.h" se maneje ese CFG con el fin de que el proceso Coordinador obtenga la información
	 * necesaria para establecer su socket. El CFG contiene los campos IP, PUERTO, BACKLOG, PACKAGESIZE.
	 */

	// Importo los datos del archivo de configuracion
	config = conectarAlArchivo(logger, "/home/utnso/workspace/tp-2018-1c-El-Rejunte/coordinador/config_coordinador.cfg", &error_config);

	ip = obtenerCampoString(logger, config, "IP", &error_config);
	port = obtenerCampoString(logger, config, "PORT", &error_config);
	algoritmo_distribucion = obtenerCampoString(logger, config, "ALGORITMO_DISTRIBUCION", &error_config);
	cant_entradas = obtenerCampoInt(logger, config, "CANT_ENTRADAS", &error_config);
	tam_entradas = obtenerCampoInt(logger, config, "TAM_ENTRADAS", &error_config);
	retardo = obtenerCampoInt(logger, config, "RETARDO", &error_config);
	ip_planificador = obtenerCampoString(logger, config, "IP_PLANIFICADOR", &error_config);
	port_planificador = obtenerCampoString(logger, config, "PORT_PLANIFICADOR", &error_config);

	establecerProtocoloDistribucion();

	// Valido si hubo errores
	if (error_config) {
		log_error(logger, "No se pudieron obtener todos los datos correspondientes");
		return CONFIGURACION_ERROR;
	}
	return CONFIGURACION_OK;
}

void destruirInstancia(void* nodo) {
	t_instancia* instancia = (t_instancia*) nodo;
	list_destroy_and_destroy_elements(instancia->claves_asignadas, free);
	finalizarSocket(instancia->socket);
}

void finalizar(int cod) {
	if (socketDeEscucha > 0) finalizarSocket(socketDeEscucha);
	if (socketPlanificador > 0) finalizarSocket(socketPlanificador);
	if (socketConsola > 0) finalizarSocket(socketConsola);
	list_destroy_and_destroy_elements(tabla_instancias, destruirInstancia);
	if (clave_actual != NULL) free(clave_actual);
	log_destroy(logger_operaciones);
	log_destroy(logger);
	finalizarConexionArchivo(config);
	exit(cod);
}

void signalHandler(int senal) {
	finalizar(EXIT_FAILURE);
}

int main() { // ip y puerto son char* porque en la biblioteca se los necesita de ese tipo
	signal(SIGINT, signalHandler);
	simulacion_activada = false;

	/*
	 * Se crea el logger, es una estructura a la cual se le da forma con la biblioca "log.h", me sirve para
	 * comunicar distintos tipos de mensajes que emite el S.O. como ser: WARNINGS, ERRORS, INFO.
	 */
	logger = log_create("coordinador.log", "Coordinador", true, LOG_LEVEL_TRACE);
	logger_operaciones = log_create("log_operaciones.log", "Log de Operaciones", true, LOG_LEVEL_INFO);

	if (cargarConfiguracion() == CONFIGURACION_ERROR) {
		log_error(logger, "No se pudo cargar la configuracion");
		finalizar(EXIT_FAILURE);
	}

	log_info(logger, "Se crea la Tabla de Instancias");
	tabla_instancias = list_create();

	socketDeEscucha = conectarComoServidor(logger, ip, port);

	while (1) { // Infinitamente escucha a la espera de que se conecte alguien
		int socketCliente = escucharCliente(logger, socketDeEscucha);
		pthread_t unHilo; // Cada conexion la delega en un hilo
		pthread_create(&unHilo, NULL, (void*) establecerConexion, (void*) &socketCliente);
	}

	finalizar(EXIT_SUCCESS);
}

