/*
 ============================================================================
 Name        : esi.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

/*
 * Modelo ejemplo de un Cliente que envia mensajes a un Server.
 *
 * 	No se contemplan el manejo de errores en el sistema por una cuestion didactica. Tener en cuenta esto al desarrollar.
 */

#include "esi.h"

t_log* logger;
t_config* config;
uint32_t miID;
bool error_config = false;
char* ip_coordinador;
char* ip_planificador;
char* port_coordinador;
char* port_planificador;
int socketCoordinador, socketPlanificador;
FILE *fp;
uint32_t respuesta;

const int ABORTA_ESI = -1;
const int TERMINA_ESI = 0;
const int PAQUETE_OK = 1;

t_esi_operacion parsearLineaScript(FILE* fp) {
	char * line = NULL;
	size_t len = 0;

	getline(&line, &len, fp);
	printf("%s", line);
	t_esi_operacion parsed = parse(line);

	if (line) free(line);

	return parsed;
}

t_control_configuracion cargarConfiguracion() {
	error_config = false;

	// Se crea una estructura de datos que contendra todos lo datos de mi CFG que lea la funcion config_create
	config = conectarAlArchivo(logger, "/home/utnso/workspace/tp-2018-1c-El-Rejunte/esi/config_esi.cfg", &error_config);

	// Obtiene los datos para conectarse al coordinador y al planificador
	ip_coordinador = obtenerCampoString(logger, config, "IP_COORDINADOR", &error_config);
	ip_planificador = obtenerCampoString(logger, config, "IP_PLANIFICADOR", &error_config);
	port_coordinador = obtenerCampoString(logger, config, "PORT_COORDINADOR", &error_config);
	port_planificador = obtenerCampoString(logger, config, "PORT_PLANIFICADOR",	&error_config);

	// Valido posibles errores
	if (error_config) {
		log_error(logger, "No se pudieron obtener todos los datos correspondientes");
		return CONFIGURACION_ERROR;
	}
	return CONFIGURACION_OK;
}

void finalizar() {
	if (fp) fclose(fp);
	if (socketCoordinador > 0) finalizarSocket(socketCoordinador);
	if (socketPlanificador > 0) finalizarSocket(socketPlanificador);
	log_destroy(logger);
	finalizarConexionArchivo(config);
}

int main(int argc, char* argv[]) { // Recibe por parametro el path que se guarda en arv[1]
	logger = log_create("esi.log", "ESI", true, LOG_LEVEL_INFO);

	if (cargarConfiguracion() == CONFIGURACION_ERROR) {
		log_error(logger, "No se pudo cargar la configuracion");
		finalizar(); // Si hubo error, se corta la ejecucion.
		return EXIT_FAILURE;
	}

	// Abro el fichero del script
	fp = fopen(argv[1], "r");
	if (!fp) {
		log_error(logger, "Error al abrir el archivo");
		finalizar();
		return EXIT_FAILURE;
	}

	log_info(logger, "Me conecto como cliente al Coordinador y al Planificador");
	// Me conecto como cliente al Coordinador y al Planificador
	socketCoordinador = conectarComoCliente(logger, ip_coordinador, port_coordinador);
	uint32_t handshake = ESI;
	send(socketCoordinador, &handshake, sizeof(uint32_t), 0);

	socketPlanificador = conectarComoCliente(logger, ip_planificador, port_planificador);
	// El planificador me asigna mi ID
	recv(socketPlanificador, &miID, sizeof(uint32_t), 0);
	log_info(logger, "El Planificador me asigno mi ID: %d", miID);

	log_info(logger, "Le aviso al Coordinador que soy el ESI %d", miID);
	send(socketCoordinador, &miID, sizeof(uint32_t), 0);

	recv(socketCoordinador, &respuesta, sizeof(uint32_t), 0);
	if (respuesta == PAQUETE_OK) log_info(logger, "El Coordinador informa que me detecto correctamente");

	uint32_t orden;

	while(!feof(fp)) {
		log_info(logger, "Espero a que el Planificador me ordene parsear una instruccion");
		recv(socketPlanificador, &orden, sizeof(uint32_t), 0);

		//if (orden == SIGUIENTE_INSTRUCCION) {
		if (orden == 1) {
			log_info(logger, "El planificador me pide que parsee la siguiente instruccion");
			// Se parsea la instruccion que se le enviara al coordinador
			t_esi_operacion instruccion = parsearLineaScript(fp);
			log_info(logger, "La instruccion fue parseada");
			// Se empaqueta la instruccion
			char* paquete = empaquetarInstruccion(instruccion, logger);

			log_info(logger, "Envio la instruccion al Cooordinador");
			uint32_t tam_paquete = strlen(paquete);
			send(socketCoordinador, &tam_paquete, sizeof(uint32_t), 0); // Envio el header
			send(socketCoordinador, paquete, tam_paquete, 0); // Envio el paquete
			destruirPaquete(paquete);

			recv(socketCoordinador, &respuesta, sizeof(uint32_t), 0);
			if (respuesta == PAQUETE_OK) log_info(logger, "El Coordinador informa que el paquete llego correctamente");

			recv(socketCoordinador, &respuesta, sizeof(uint32_t), 0);

			if (feof(fp)) {
				log_info(logger, "El Coordinador informa el resultado de la instruccion");
				log_warning(logger, "Le aviso al Planificador que no tengo mas instrucciones para ejecutar");
				send(socketPlanificador, &TERMINA_ESI, sizeof(uint32_t), 0);
				break;
			}

			if (respuesta == PAQUETE_OK) {
				log_info(logger, "El Coordinador informa que la instruccion se proceso satisfactoriamente");
				log_info(logger, "Le aviso al Planificador que la instruccion pudo ser procesada");
				send(socketPlanificador, &PAQUETE_OK, sizeof(uint32_t), 0);
			} else {
				log_error(logger, "El Coordinador informa que la instruccion no se pudo procesar");
				log_error(logger, "SE ABORTA EL ESI");
				send(socketPlanificador, &ABORTA_ESI, sizeof(uint32_t), 0);
				finalizar();
				return EXIT_FAILURE;
			}
		}
	}

	finalizar();
	return EXIT_SUCCESS;
}
