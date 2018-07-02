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
	ACTIVA,
	INACTIVA
};

typedef enum {
	EL = 0,
	LSU = 1,
	KE = 3
} t_distribucion;

t_log* logger;
t_log* logger_operaciones;
bool error_config;
char* ip;
char* port;
char* algoritmo_distribucion;
t_distribucion protocolo_distribucion;
int retardo;
uint32_t cant_entradas, tam_entradas;
t_list* tabla_instancias;
int socketDeEscucha;
int socketPlanificador;
char* clave_actual;

const uint32_t PAQUETE_OK = 1;
const int TAM_MAXIMO_CLAVE = 40;

/*
t_instancia* algoritmoLSU(cola_istancias, instancia, clave) {
	analizar tamaño de entradas();
	analizar tamanño de instancias(); //tamaño = cantidad de entradas libres
	if(hay entradas libres){
		if (tamEntLibre == tamLoQQuieroGuardar) // si lo quiero guardar es atómico
			asignar clave en ésta entrada();
		else if (tamEntLibre < tamLoQQuieroGuardar) // si lo que quiero guardar ocupa más de una entrada
			buscar espacio continuo() //dos entradas libres contiguas
			si hay
				asignar clave en estas entradas()
		   	si no hay
				compactar o posiblemente seguir buscando
	}
}
t_instancia* algoritmoKE(cola_instancias, instancia, char clave) {
	inicial = getChar("clave"); // tomar primer caracter clave EN MINUSCULA, ésto
	inicialEnMinuscula = tolower(inicial) // convierte un tipo de dato caracter a minuscula (A-Z a a-z).
	verificar donde guardar(inicialEnMinuscula == inicialInstancia) // inicial debera ser un numero, ejemplo "a" es 97
	if (está la instancia con la misma inicial) {
		guardar en esa instancia
	} else {
		ACA NO SE SABE QUE HACE
	}
}
*/

t_instancia* algoritmoEL() {
	t_instancia* instancia = list_remove(tabla_instancias, 0);
	list_add_in_index(tabla_instancias, list_size(tabla_instancias), instancia);
	return instancia;
}

bool tieneAlgunaEntradaDisponible(t_instancia* instancia){

  if((instancia->entradas_libres > 0 )&& (instancia->estado == 1) > 0){
	  return true;
  } else {
	  return false;
  }
};

t_instancia* algoritmoLSU(){
	t_instancia* instancia;
	while(!(list_is_empty(tabla_instancias))){ //lista esta vacia
		return instancia = list_remove_by_condition(tabla_instancias, (void*)tieneAlgunaEntradaDisponible); // devuelvo la instancia con mas entradas libres
	};
	return instancia = NULL; // si no hay instancia en tabla_instancias devuelvo una lista para llenar.
};


t_instancia* algoritmoDeDistribucion(char* c) {
	// implementar
	// paso 1: hay que hacer un switch de la variable ya cargada: algoritmo_distribucion
	// paso 2: implementar si es EL (Equitative Load) que TCB devuelve de la tabla_instancias
	// obs: hacer el case de los demas casos pero sin implementacion

	log_info(logger, "Aguarde mientras se busca una Instancia");
	while (list_is_empty(tabla_instancias)) {
		sleep(4); // Lo pongo para que la espera activa no sea tan densa
		log_warning(logger, "No hay instancias disponibles. Reintentando...");
	}

	switch (protocolo_distribucion) {
	case LSU: // LSU
		return algoritmoLSU();

	case KE: // KE
		//return algoritmoKE();

	default: // Equitative Load
		return algoritmoEL();
	}
}

void loguearOperacion(uint32_t esi_ID, char* paquete) {
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
	string_append(&cadena_log_operaciones, paquete);
	log_info(logger_operaciones, cadena_log_operaciones);
}

bool claveEsLaActual(void* nodo) {
	char* clave = (char*) nodo;
	return strcmp(clave, clave_actual) == 0;
}

bool instanciaTieneLaClave(void* nodo) {
	t_instancia* instancia = (t_instancia*) nodo;
	return list_any_satisfy(instancia->claves_asignadas, claveEsLaActual);
}

int procesarPaquete(char* paquete) {
	t_instruccion* instruccion = desempaquetarInstruccion(paquete, logger);

	if (strlen(instruccion->clave) > TAM_MAXIMO_CLAVE) {
		log_error(logger, "Error de Tamano de Clave");
		return -1;
	}

	log_info(logger, "El Coordinador esta chequeando si la clave ya existe");
	t_instancia* instancia = (t_instancia*) list_find(tabla_instancias, instanciaTieneLaClave);

	if (!instancia) {
		log_info(logger, "La clave %s no esta en ninguna Instancia", instruccion->clave);
	} else {
		log_info(logger, "La clave %s esta asignada a Instancia %d", instancia->id);
	}

	if (instancia->estado == INACTIVA) {
		log_error(logger, "Error de Clave Inaccesible");
		return -1;
	}

	int opGET = 1;

	if (instruccion->operacion == opGET) {

		// existe => no hago nada
		// no existe => la creo

		if (!instancia) {
			log_info(logger, "La clave %s no existe en ninguna Instancia", instruccion->clave);
			log_info(logger, "Escojo una Instancia segun el algoritmo %s", algoritmo_distribucion);
			t_instancia* instancia = algoritmoDeDistribucion(instruccion->clave);
			log_info(logger, "La Instancia sera la %d", instancia->id);
			list_add(instancia->claves_asignadas, instruccion->clave);
			log_info(logger, "La clave %s fue asignada a la Instancia %d", instruccion->clave, instancia->id);
		} else {
			log_info(logger, "Como la clave ya esta asignada no hago nada");
		}
	} else { // SET o STORE

		// existe => la envio a Instancia
		// no existe => Error de Clave no Identificada

		if (instancia) {
			log_info(logger, "Le envio a Instancia %d el paquete", instancia->id);
			uint32_t tam_paquete = strlen(paquete);
			send(instancia->socket, &tam_paquete, sizeof(uint32_t), 0);
			send(instancia->socket, &paquete, tam_paquete, 0);

			// La Instancia me devuelve la cantidad de entradas libres que tiene
			uint32_t respuesta;
			recv(instancia->socket, &respuesta, sizeof(uint32_t), 0);
			instancia->entradas_libres = respuesta;
			log_info(logger, "La Instancia %d me informa que le quedan %d entradas libres", instancia->id, respuesta);

		} else {
			log_error(logger, "Error de Clave no Identificada");
			return -1;
		}
	}
	return 1;
}

void atenderESI(int socketESI) {
	// ---------- COORDINADOR - ESI ----------
	// ANALIZAR CONCURRENCIA CON SEMAFOROS MUTEX

	uint32_t esi_ID;
	recv(socketESI, &esi_ID, sizeof(uint32_t), 0);
	log_info(logger, "Se ha conectado un ESI con ID: %d", esi_ID);
	send(socketESI, &PAQUETE_OK, sizeof(uint32_t), 0);

	while(1) {
		uint32_t tam_paquete;
		recv(socketESI, &tam_paquete, sizeof(uint32_t), 0); // Recibo el header
		char* paquete = (char*) malloc(sizeof(char) * tam_paquete);
		recv(socketESI, paquete, tam_paquete, 0);
		log_info(logger, "El ESI %d me envia un paquete", esi_ID);

		sleep(retardo * 0.001); // Retardo ficticio

		log_info(logger, "Le informo al ESI %d que el paquete llego correctamente", esi_ID);
		send(socketESI, &PAQUETE_OK, sizeof(uint32_t), 0); // Envio respuesta al ESI

		// ---------- COORDINADOR - PLANIFICADOR ----------
		// Aca el Coordinador le va a mandar el paquete al Planificador
		// Esto es para consultar si puede utilizar los recursos que pide

		log_info(logger, "Le consulto al Planificador si ESI %d puede hacer uso del recurso", esi_ID);
		send(socketPlanificador, &tam_paquete, sizeof(uint32_t), 0);
		send(socketPlanificador, &paquete, tam_paquete, 0);

		uint32_t respuesta;
		recv(socketPlanificador, &respuesta, sizeof(uint32_t), 0);

		if (respuesta == SE_EJECUTA_ESI) {
			log_info(logger, "El Planificador me informa que el ESI %d puede utilizar el recurso", esi_ID);
			if (procesarPaquete(paquete) == -1) { // Hay que abortar el ESI
				log_error(logger, "Se aborta ESI %d y le aviso al Planificador", esi_ID);
				//send(socketPlanificador, &ABORTAR_ESI, sizeof(uint32_t), 0);
				break;
			}
			loguearOperacion(esi_ID, paquete);

			log_info(logger, "Le aviso al ESI %d que la instruccion se ejecuto satisfactoriamente", esi_ID);
			send(socketESI, &PAQUETE_OK, sizeof(uint32_t), 0);
		}
		destruirPaquete(paquete);
	}
}

void atenderInstancia(int socketInstancia) {
	// Recibo la ID
	uint32_t instancia_ID;
	recv(socketInstancia, &instancia_ID, sizeof(uint32_t), 0);
	log_info(logger, "Es la Instancia %d", instancia_ID);

	log_info(logger, "Busco si ya fue creada en la Tabla de Instancias");
	//t_instancia* instancia = list_find(tabla_instancias, existeID);
	t_instancia* instancia = NULL; // no va
	if (instancia) {
		log_info(logger, "La Instancia %d ya existia, la pongo ACTIVA", instancia_ID);
		instancia->estado = ACTIVA;
		return;
	}

	// Guarda el struct de la Instancia en mi lista
	t_instancia* unaInstancia = (t_instancia*) malloc(sizeof(t_instancia));
	unaInstancia->id = instancia_ID;
	unaInstancia->socket = socketInstancia;
	unaInstancia->entradas_libres = cant_entradas;
	unaInstancia->estado = ACTIVA;
	unaInstancia->claves_asignadas = list_create();

	log_info(logger, "Envio a la Instancia su cantidad de entradas");
	send(socketInstancia, &cant_entradas, sizeof(uint32_t), 0);

	log_info(logger, "Envio a la Instancia el tamaño de las entradas");
	send(socketInstancia, &tam_entradas, sizeof(uint32_t), 0);

	list_add(tabla_instancias, unaInstancia);
	log_info(logger, "Instancia agregada a la Tabla de Instancias");

	printf("La cantidad de instancias actual es %d\n", list_size(tabla_instancias));
}

void* establecerConexion(void* socketCliente) {
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
		send(*(int*) socketCliente, &PAQUETE_OK, sizeof(uint32_t), 0);
	} else {
		log_error(logger, "No se pudo reconocer al cliente");
	}

	return NULL;
}

// Protocolo numerico de ALGORITMO_DISTRIBUCION
void establecerProtocoloDistribucion() {
	if (strcmp(algoritmo_distribucion, "LSU")) {
		protocolo_distribucion = LSU;
	} else if (strcmp(algoritmo_distribucion, "KE")) {
		protocolo_distribucion = KE;
	} else {
		protocolo_distribucion = EL; // Equitative Load
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
	t_config* config = conectarAlArchivo(logger, "/home/utnso/workspace/tp-2018-1c-El-Rejunte/coordinador/config_coordinador.cfg", &error_config);

	ip = obtenerCampoString(logger, config, "IP", &error_config);
	port = obtenerCampoString(logger, config, "PORT", &error_config);
	algoritmo_distribucion = obtenerCampoString(logger, config, "ALGORITMO_DISTRIBUCION", &error_config);
	cant_entradas = obtenerCampoInt(logger, config, "CANT_ENTRADAS", &error_config);
	tam_entradas = obtenerCampoInt(logger, config, "TAM_ENTRADAS", &error_config);
	retardo = obtenerCampoInt(logger, config, "RETARDO", &error_config);

	establecerProtocoloDistribucion();

	// Valido si hubo errores
	if (error_config) {
		log_error(logger, "No se pudieron obtener todos los datos correspondientes");
		return CONFIGURACION_ERROR;
	}
	return CONFIGURACION_OK;
}

void finalizar() {
	finalizarSocket(socketDeEscucha);
	log_destroy(logger_operaciones);
	log_destroy(logger);
}

int main() { // ip y puerto son char* porque en la biblioteca se los necesita de ese tipo
	error_config = false;

	/*
	 * Se crea el logger, es una estructura a la cual se le da forma con la biblioca "log.h", me sirve para
	 * comunicar distintos tipos de mensajes que emite el S.O. como ser: WARNINGS, ERRORS, INFO.
	 */
	logger = log_create("coordinador.log", "Coordinador", true, LOG_LEVEL_INFO);
	logger_operaciones = log_create("log_operaciones.log", "Log de Operaciones", true, LOG_LEVEL_INFO);

	if (cargarConfiguracion() == CONFIGURACION_ERROR) {
		log_error(logger, "No se pudo cargar la configuracion");
		finalizar();
		return EXIT_FAILURE;
	}

	log_info(logger, "Se crea la Tabla de Instancias");
	tabla_instancias = list_create();

	socketDeEscucha = conectarComoServidor(logger, ip, port);

	while (1) { // Infinitamente escucha a la espera de que se conecte alguien
		int socketCliente = escucharCliente(logger, socketDeEscucha);
		pthread_t unHilo; // Cada conexion la delega en un hilo
		pthread_create(&unHilo, NULL, establecerConexion, (void*) &socketCliente);
	}

	return EXIT_SUCCESS;
}

