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

/* INFORMES AL PLANIFICADOR Y COORDINADOR */
const uint32_t TERMINA_ESI = 0;

/* PEDIDOS DEL PLANIFICADOR */
const uint32_t SIGUIENTE_INSTRUCCION = 1;
const uint32_t DESBLOQUEA_ESI = -3;

/* RESULTADOS DEL COORDINADOR */
const uint32_t PAQUETE_OK = 1;
const uint32_t BLOQUEA_ESI = -1;
const uint32_t ABORTA_ESI = -2;


t_esi_operacion parsearLineaScript(FILE* fp) {
	char * line = NULL;
	size_t len = 0;

	getline(&line, &len, fp);
	log_debug(logger, "%s", line);
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

void finalizar(int cod) {
	if (fp) fclose(fp);
	if (socketCoordinador > 0) finalizarSocket(socketCoordinador);
	if (socketPlanificador > 0) finalizarSocket(socketPlanificador);
	log_destroy(logger);
	finalizarConexionArchivo(config);
	exit(cod);
}

int main(int argc, char* argv[]) { // Recibe por parametro el path que se guarda en arv[1]
	logger = log_create("esi.log", "ESI", true, LOG_LEVEL_DEBUG);

	if (cargarConfiguracion() == CONFIGURACION_ERROR) {
		log_error(logger, "No se pudo cargar la configuracion");
		finalizar(EXIT_FAILURE); // Si hubo error, se corta la ejecucion.
	}

	// Abro el fichero del script
	fp = fopen(argv[1], "r");
	if (!fp) {
		log_error(logger, "Error al abrir el archivo");
		finalizar(EXIT_FAILURE);
	}

	log_info(logger, "Me conecto como cliente al Coordinador y al Planificador");
	socketCoordinador = conectarComoCliente(logger, ip_coordinador, port_coordinador);
	if (socketCoordinador < 0) {
		log_error(logger, "Error de Comunicacion: no me pude conectar con el Coordinador, me aborto");
		finalizar(EXIT_FAILURE);
	}

	uint32_t handshake = ESI;
	send(socketCoordinador, &handshake, sizeof(uint32_t), 0);

	socketPlanificador = conectarComoCliente(logger, ip_planificador, port_planificador);
	if (socketPlanificador < 0) {
		log_error(logger, "Error de Comunicacion: no me pude conectar con el Planificador, me aborto");
		finalizar(EXIT_FAILURE);
	}

	// El planificador me asigna mi ID
	recv(socketPlanificador, &miID, sizeof(uint32_t), 0);
	log_info(logger, "El Planificador me asigno mi ID: %d", miID);

	log_info(logger, "Le aviso al Coordinador que soy el ESI %d", miID);
	send(socketCoordinador, &miID, sizeof(uint32_t), 0);

	recv(socketCoordinador, &respuesta, sizeof(uint32_t), 0);
	if (respuesta == PAQUETE_OK) log_info(logger, "El Coordinador informa que me detecto correctamente");

	uint32_t orden;

	while (!feof(fp)) {
		log_info(logger, "Espero a que el Planificador me ordene parsear una instruccion");
		if (recv(socketPlanificador, &orden, sizeof(uint32_t), 0) < 1) {
			log_error(logger, "Error de Comunicacion: se ha roto la conexion con el Planificador, me aborto");
			finalizar(EXIT_FAILURE);
		}

		/*
		 * ATIENDO PEDIDO DEL PLANIFICADOR
		 */
		if (orden == SIGUIENTE_INSTRUCCION) {
			/*
			 * ENVIO PROXIMA INSTRUCCION
			 */
			log_info(logger, "El planificador me pide que parsee la siguiente instruccion:");
			// Se parsea la instruccion que se le enviara al coordinador
			t_esi_operacion instruccion = parsearLineaScript(fp);
			if (!instruccion.valido) {
				log_error(logger, "No se pudo parsear una instruccion, me aborto");
				send(socketPlanificador, &TERMINA_ESI, sizeof(uint32_t), 0);
				send(socketCoordinador, &TERMINA_ESI, sizeof(uint32_t), 0);
				finalizar(EXIT_FAILURE);
			}
			log_info(logger, "La instruccion fue parseada, le aviso al Planificador");
			send(socketPlanificador, &PAQUETE_OK, sizeof(uint32_t), 0);
			// Se empaqueta la instruccion
			char* paquete = empaquetarInstruccion(instruccion, logger);

			log_info(logger, "Envio la instruccion al Cooordinador");
			uint32_t tam_paquete = strlen(paquete) + 1;
			send(socketCoordinador, &tam_paquete, sizeof(uint32_t), 0); // Envio el header
			send(socketCoordinador, paquete, tam_paquete, MSG_NOSIGNAL); // Envio el paquete

			if (recv(socketCoordinador, &respuesta, sizeof(uint32_t), 0) < 1) {
				log_error(logger, "Error de Comunicacion: se ha roto la conexion con el Coordinador, me aborto");
				finalizar(EXIT_FAILURE);
			}
			if (respuesta == PAQUETE_OK) log_info(logger, "El Coordinador informa que el paquete llego correctamente");

			if (recv(socketCoordinador, &respuesta, sizeof(uint32_t), 0) < 0) {
				log_error(logger, "Error de Comunicacion: se ha roto la conexion con el Coordinador, me aborto");
				finalizar(EXIT_FAILURE);
			}

			if (respuesta == PAQUETE_OK) {
				log_info(logger, "El Coordinador informa que la instruccion se proceso satisfactoriamente");

				if (feof(fp)) {
					log_warning(logger, "Le aviso al Planificador y al Coordinador que no tengo mas instrucciones para ejecutar");
					send(socketPlanificador, &TERMINA_ESI, sizeof(uint32_t), 0);
					send(socketCoordinador, &TERMINA_ESI, sizeof(uint32_t), 0);
					break;
				}

				log_info(logger, "Le aviso al Planificador que la instruccion pudo ser procesada");
				send(socketPlanificador, &PAQUETE_OK, sizeof(uint32_t), 0);
			} else if (respuesta == BLOQUEA_ESI) {
				/*
				 * ESTOY BLOQUEADO
				 */
				log_warning(logger, "El Coordinador informa que la instruccion no se pudo procesar");
				log_warning(logger, "Se bloquea el ESI");

				if (recv(socketPlanificador, &orden, sizeof(uint32_t), 0) < 0) {
					log_error(logger, "Error de Comunicacion: se ha roto la conexion con el Planificador, me aborto");
					finalizar(EXIT_FAILURE);
				}
				if (orden == SIGUIENTE_INSTRUCCION) {
					/*
					 * ESTOY DESBLOQUEADO => ENVIO NUEVAMENTE LA INSTRUCCION
					 */
					log_info(logger, "El Planificador me informa que vuelva a enviar la instruccion");
					send(socketCoordinador, &DESBLOQUEA_ESI, sizeof(uint32_t), 0);
					send(socketCoordinador, &tam_paquete, sizeof(uint32_t), 0); // Envio el header
					send(socketCoordinador, paquete, tam_paquete, 0); // Envio el paquete
					recv(socketCoordinador, &respuesta, sizeof(uint32_t), 0);
					if (respuesta == PAQUETE_OK) log_info(logger, "El Coordinador informa que el paquete llego correctamente");

					if (recv(socketCoordinador, &respuesta, sizeof(uint32_t), 0) < 0) {
						log_error(logger, "Error de Comunicacion: se ha roto la conexion con el Coordinador, me aborto");
						finalizar(EXIT_FAILURE);
					}
					if (respuesta == PAQUETE_OK) {
						log_info(logger, "El Coordinador informa que la instruccion se proceso satisfactoriamente");

						if (feof(fp)) {
							log_warning(logger, "Le aviso al Planificador y al Coordinador que no tengo mas instrucciones para ejecutar");
							send(socketPlanificador, &TERMINA_ESI, sizeof(uint32_t), 0);
							send(socketCoordinador, &TERMINA_ESI, sizeof(uint32_t), 0);
							break;
						}

						log_info(logger, "Le aviso al Planificador que la instruccion pudo ser procesada");
						send(socketPlanificador, &PAQUETE_OK, sizeof(uint32_t), 0);
					} else {
						log_error(logger, "El Coordinador informa que la instruccion no se pudo procesar");
						log_error(logger, "Se aborta el ESI");
						send(socketPlanificador, &TERMINA_ESI, sizeof(uint32_t), MSG_DONTWAIT);
						finalizar(EXIT_FAILURE);
					}

				} else {
					log_error(logger, "El Planificador me informa que debo abortar");
					break;
				}
			} else {
				log_error(logger, "El Coordinador informa que la instruccion no se pudo procesar");
				log_error(logger, "Se aborta el ESI");
				send(socketPlanificador, &TERMINA_ESI, sizeof(uint32_t), MSG_DONTWAIT);
				finalizar(EXIT_FAILURE);
			}
		} else {
			log_error(logger, "El Planificador me informa que debo abortar");
			break;
		}
	}
	finalizar(EXIT_SUCCESS);
}
