#include "planificador.h"


int main(void) {

	logPlanificador = log_create("planificador.log", "Planificador" , true, LOG_LEVEL_DEBUG);
	colaListos = queue_create();
	listaFinalizados = list_create();
	listaRecursos = list_create();

	log_info(logPlanificador,"Arranca el proceso planificador");
	configurar();

	socketCoordinador = conectarComoCliente(logPlanificador, ipCoordinador, puertoCoordinador);

	if(socketClienteCoordinador == -1)
	{
		log_info(logPlanificador, "se rompio todo wacho");
		liberarGlobales();
		exit(-1);
	}

	uint32_t handshake = 3;
	send(socketCoordinador, &handshake, sizeof(handshake),0);

	uint32_t respuesta = 0;
	int comprobar = 0;

	comprobar = recv(socketCoordinador, &respuesta, sizeof(respuesta),0);

	if(respuesta != 1 || comprobar <= 0)
	{
		log_info(logPlanificador,"Conexion rota");
		liberarGlobales();
		exit(-1);
	}

	socketDeEscucha = conectarComoServidor(logPlanificador, ipPropia, puertoPropio);

	socketClienteCoordinador = escucharCliente(logPlanificador, socketDeEscucha);

	if(socketCoordinador == -1)
	{
		log_info(logPlanificador, "se rompio todo wacho");
		liberarGlobales();
		exit(-1);
	}


	if(string_equals_ignore_case(algoritmoDePlanificacion, SJF) == true)
		{
			log_info(logPlanificador, "la planificacion elegida es SJF sin desalojo");
			planificacionSJF(false);

		} else if (string_equals_ignore_case(algoritmoDePlanificacion,SJFConDesalojo))
		{
			log_info(logPlanificador, " la planificacion elegida es SJF con desalojo");
			planificacionSJF(true);
		} else if(string_equals_ignore_case(algoritmoDePlanificacion,HRRN)){

			log_info(logPlanificador, " la planficacion elegida es HRRN sin desalojo");
			planificacionHRRN(false);

		} else if (string_equals_ignore_case(algoritmoDePlanificacion,HRRNConDesalojo)){

			log_info(logPlanificador, " la planficacion elegida es HRRN con desalojo");
			planificacionHRRN(true);

		}

	liberarGlobales();

	return EXIT_SUCCESS;

}


void configurar(){

	log_info(logPlanificador, "preparando archivo configuracion");

	archivoConfiguracion = config_create(RUTA_CONFIGURACION);

	log_info (logPlanificador, "preparado");

	algoritmoDePlanificacion = string_new();
	ipCoordinador = string_new();
	puertoCoordinador = string_new();
	ipPropia = string_new();
	puertoPropio = string_new();

	log_info(logPlanificador, "leyendo archivo configuracion ");

	string_append( &algoritmoDePlanificacion, config_get_string_value(archivoConfiguracion, KEY_ALGORITMO_PLANIFICACION));

	log_info(logPlanificador, "algoritmo a usar leido = %s", algoritmoDePlanificacion);

	alfa = config_get_int_value(archivoConfiguracion,KEY_CONSTANTE_ESTIMACION);

	log_info(logPlanificador, "constante estimacion leida = %d", alfa);

	estimacionInicial = config_get_int_value(archivoConfiguracion, KEY_ESTIMACION_INICIAL);

	log_info(logPlanificador, "estimacion inicial leida = %d", estimacionInicial);

	string_append(&ipCoordinador,config_get_string_value(archivoConfiguracion, KEY_IP_COORDINADOR));

	log_info(logPlanificador, "IP coordinador leido = %s", ipCoordinador);

	string_append(&puertoCoordinador,config_get_string_value(archivoConfiguracion, KEY_PUERTO_COORDINADOR));

	log_info(logPlanificador, " puerto coordinador leido = %s", puertoCoordinador);

	string_append(&ipPropia,config_get_string_value(archivoConfiguracion, KEY_IP_PROPIA));

	log_info(logPlanificador, " ip propia leida = %s", ipPropia);

	string_append(&puertoPropio,config_get_string_value(archivoConfiguracion, KEY_PUERTO_PROPIO));

	log_info(logPlanificador, " puerto propio leido = %s", puertoPropio);


	clavesBloqueadas = config_get_array_value(archivoConfiguracion, KEY_CLAVES_BLOQUEADAS);

	log_info(logPlanificador, "array de claves bloqueadas obtenido");

	log_info(logPlanificador, "llenando lista de bloqueados iniciales");

	int i = 0;

	while (clavesBloqueadas[i] != NULL)
	{
		t_recurso * recurso = crearRecurso(clavesBloqueadas[i]);
		recurso->estado = 1;
		list_add(listaRecursos, recurso);
		i++;

	}
	log_info(logPlanificador, "se llenó la lista de claves bloqueadas");

	config_destroy(archivoConfiguracion);
}
