#include "planificador.h"


int main(void) {

	logPlanificador = log_create("planificador.log", "Planificador" , true, LOG_LEVEL_TRACE);
	colaListos = queue_create();
	listaFinalizados = list_create();
	listaRecursos = list_create();

	log_info(logPlanificador,"Arranca el proceso planificador");
	configurar();

	socketCoordinador = conectarComoCliente(logPlanificador, ipCoordinador, puertoCoordinador);

	if(socketClienteCoordinador == -1)
	{
		log_error(logPlanificador, "Falla conexion al coordinador como servidor. Se aborta proceso");
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
		log_error(logPlanificador,"Falla conexion con el coordinador como cliente. Se aborta proceso");
		liberarGlobales();
		exit(-1);
	}

	socketDeEscucha = conectarComoServidor(logPlanificador, ipPropia, puertoPropio);

	socketClienteCoordinador = escucharCliente(logPlanificador, socketDeEscucha);

	if(socketCoordinador == -1)
	{
		log_error(logPlanificador,"Falla conexion con el coordinador como cliente. Se aborta proceso");
		liberarGlobales();
		exit(-1);
	}


	if(string_equals_ignore_case(algoritmoDePlanificacion, SJF) == true)
		{
			log_info(logPlanificador, "La planificacion elegida es SJF sin desalojo");
			planificacionSJF(false);

		} else if (string_equals_ignore_case(algoritmoDePlanificacion,SJFConDesalojo))
		{
			log_info(logPlanificador, " La planificacion elegida es SJF con desalojo");
			planificacionSJF(true);
		} else if(string_equals_ignore_case(algoritmoDePlanificacion,HRRN)){

			log_info(logPlanificador, " La planficacion elegida es HRRN sin desalojo");
			planificacionHRRN(false);

		} else if (string_equals_ignore_case(algoritmoDePlanificacion,HRRNConDesalojo)){

			log_info(logPlanificador, " La planficacion elegida es HRRN con desalojo");
			planificacionHRRN(true);

		}

	liberarGlobales();

	return EXIT_SUCCESS;

}


void configurar(){

	archivoConfiguracion = config_create(RUTA_CONFIGURACION);
	algoritmoDePlanificacion = string_new();
	ipCoordinador = string_new();
	puertoCoordinador = string_new();
	ipPropia = string_new();
	puertoPropio = string_new();
	//sem_init(&contadorColaListos, NULL, 0); //inicio semaforo de cola listos en 0 todo

	log_info(logPlanificador, "Leyendo archivo configuracion ");

	string_append( &algoritmoDePlanificacion, config_get_string_value(archivoConfiguracion, KEY_ALGORITMO_PLANIFICACION));

	log_debug(logPlanificador, "Algoritmo a usar leido = %s", algoritmoDePlanificacion);

	alfa = config_get_int_value(archivoConfiguracion,KEY_CONSTANTE_ESTIMACION);

	log_debug(logPlanificador, "Constante estimacion leida = %d", alfa);

	estimacionInicial = config_get_int_value(archivoConfiguracion, KEY_ESTIMACION_INICIAL) ;

	log_debug(logPlanificador, "estimacion inicial leida = %d", estimacionInicial);

	string_append(&ipCoordinador,config_get_string_value(archivoConfiguracion, KEY_IP_COORDINADOR));

	log_debug(logPlanificador, "IP coordinador leido = %s", ipCoordinador);

	string_append(&puertoCoordinador,config_get_string_value(archivoConfiguracion, KEY_PUERTO_COORDINADOR));

	log_debug(logPlanificador, " puerto coordinador leido = %s", puertoCoordinador);

	string_append(&ipPropia,config_get_string_value(archivoConfiguracion, KEY_IP_PROPIA));

	log_debug(logPlanificador, " ip propia leida = %s", ipPropia);

	string_append(&puertoPropio,config_get_string_value(archivoConfiguracion, KEY_PUERTO_PROPIO));

	log_debug(logPlanificador, " puerto propio leido = %s", puertoPropio);

	clavesBloqueadas = config_get_array_value(archivoConfiguracion, KEY_CLAVES_BLOQUEADAS);

	int i = 0;

	while (clavesBloqueadas[i] != NULL)
	{
		t_recurso * recurso = crearRecurso(clavesBloqueadas[i]);
		recurso->estado = 1;
		log_debug(logPlanificador, " Clave para bloquear leida = %s", recurso->clave);
		list_add(listaRecursos, recurso);
		i++;

	}

	config_destroy(archivoConfiguracion);
}
