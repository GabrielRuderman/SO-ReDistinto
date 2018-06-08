#include "planificador.h"

#include "SJF.h"
#include "HRRN.h"

// \n
// todo free de los char*
/* todo ahora hay que adaptar todas las funciones al sistema nuevo de recursos.
 * todo meter los sockets.
 *
 *
 *
 */

int main(void) {

	logPlanificador = log_create("planificador.log", "Planificador" , true, LOG_LEVEL_INFO);
	listaBloqueados = list_create();
	colaListos = queue_create();
	listaListos = list_create();
	listaFinalizados = list_create();
	deadlockeados = list_create();
	listaRecursos = list_create();


	log_info(logPlanificador,"Arranca el proceso planificador");
	configurar();

	if(string_equals_ignore_case(algoritmoDePlanificacion, SJF) == true)
		{
			log_info(logPlanificador, "la planificacion elegida es SJF");
			planificacionSJF();

		} else {

			log_info(logPlanificador, " la planificacion elegida es HRRN");
			//planficacionHRRN();
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
	ip = string_new();
	puerto = string_new();

	log_info(logPlanificador, "leyendo archivo configuracion ");

	puertoEscucha = config_get_int_value(archivoConfiguracion, KEY_PUERTO_CLIENTE);

	log_info(logPlanificador, "puerto del cliente leido = %d", puertoEscucha);

	string_append( &algoritmoDePlanificacion, config_get_string_value(archivoConfiguracion, KEY_ALGORITMO_PLANIFICACION));

	log_info(logPlanificador, "algoritmo a usar leido = %s", algoritmoDePlanificacion);

	alfa = config_get_int_value(archivoConfiguracion,KEY_CONSTANTE_ESTIMACION);

	log_info(logPlanificador, "constante estimacion leida = %d", alfa);

	estimacionInicial = config_get_int_value(archivoConfiguracion, KEY_ESTIMACION_INICIAL);

	log_info(logPlanificador, "estimacion inicial leida = %d", estimacionInicial);

	string_append(&ipCoordinador,config_get_string_value(archivoConfiguracion, KEY_IP_COORDINADOR));

	log_info(logPlanificador, "ip del coordinador leida = %s", ipCoordinador);

	puertoCoordinador = config_get_int_value(archivoConfiguracion, KEY_PUERTO_COORDINADOR);

	log_info(logPlanificador, "puerto coordinador leido = %d", puertoCoordinador);

	string_append(&ip,config_get_string_value(archivoConfiguracion, KEY_IP));

	log_info(logPlanificador, "mi ip leida = %s", ip);

	string_append(&puerto,config_get_string_value(archivoConfiguracion, KEY_PUERTO));

	log_info(logPlanificador, "mi puerto leido = %s", puerto);

	clavesBloqueadas = config_get_array_value(archivoConfiguracion, KEY_CLAVES_BLOQUEADAS);

	log_info(logPlanificador, "array de claves bloqueadas obtenido");

	log_info(logPlanificador, "llenando lista de bloqueados iniciales");

	int i = 0;

	/*while (clavesBloqueadas[i] != NULL)
	{

		char ** claves = string_n_split(clavesBloqueadas[i],2,":");
		list_add(listaRecursos,crearRecurso(claves[0]));
		log_info(logPlanificador, " entra a bloqueados el recurso %s", claves[0]);
		crearSubrecurso(claves[0],claves[1]);
		log_info(logPlanificador, "con su subrecurso %s", claves[1]);
		i++;
		free (claves);
	}*/
	log_info(logPlanificador, "se llenó la cola de bloqueados");

	config_destroy(archivoConfiguracion);
}


ESI * crearESI(char * clave){ // Y EL RECURSO DE DONDE SALE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!?

	ESI * nuevoESI = malloc(sizeof(ESI));
	nuevoESI->id = string_new();
	string_append(&nuevoESI->id, clave);
	nuevoESI->estimacionAnterior= estimacionInicial;
	nuevoESI->bloqueadoPorRecurso = true;
	nuevoESI-> bloqueadoPorUsuario = false;
	nuevoESI-> rafagaAnterior = 0;
	nuevoESI-> estimacionSiguiente = 0;
	nuevoESI->rafagasRealizadas =0;
	nuevoESI-> tiempoEspera = 0;
	nuevoESI->recursoAsignado = NULL;
	nuevoESI->recursoPedido = NULL;

	return nuevoESI;

}

t_recurso * crearRecurso (char * id){

	t_recurso * nuevo = malloc(sizeof(t_recurso));
	nuevo->clave = id;
	nuevo->subrecursos = list_create();
	return nuevo;

}

void crearSubrecurso (char* claveRecurso, char * claveSubrecurso)
{

	t_subrecurso * nuevoSubrecurso = malloc (sizeof(t_subrecurso));
	nuevoSubrecurso->clave = claveSubrecurso;
	nuevoSubrecurso->recursosFinales = list_create();

	int i = 0;
	bool encontrado = false;
	while(list_size(listaRecursos) >= i)
	{
		if(string_equals_ignore_case(list_get(listaRecursos,i),claveRecurso))
		{

			t_recurso * auxiliar = list_get(listaRecursos,i);
			list_add(auxiliar->subrecursos,nuevoSubrecurso);
			list_replace_and_destroy_element(listaBloqueados, i, auxiliar, (void *) recursoDestroy);
			encontrado = true;
		}
	}

	if(encontrado == false){

		t_recurso * nuevoRecurso = crearRecurso(claveRecurso);
		list_add( nuevoRecurso->subrecursos, nuevoSubrecurso);
	}

}


void recursoDestroy(t_recurso * recurso){

	free(recurso->clave);
	list_destroy_and_destroy_elements(recurso->subrecursos, (void *) subrecursoDestroy);

}

void subrecursoDestroy (t_subrecurso * subrecurso){

	free (subrecurso-> clave);
	list_destroy_and_destroy_elements(subrecurso->recursosFinales, (void *)recursoFinalDestroy);

}

void recursoFinalDestroy(t_recursoFinal * recuFinal){

	free(recuFinal->clave);
	free(recuFinal->valor);

}

void ESI_destroy(ESI * estructura)
{
	free(estructura->id);
	free(estructura->recursoAsignado);
	free(estructura->recursoPedido);
	free(estructura);

}

void DEADLOCK_destroy(t_deadlockeados * ESI){

	free(ESI->clave);
	list_destroy_and_destroy_elements(ESI->ESIasociados, (void *) free);


}

void liberarGlobales (){

	log_info(logPlanificador, "liberando espacio");
	free(algoritmoDePlanificacion);
	free(ipCoordinador);

	int i = 0;
	while(clavesBloqueadas[i]!=NULL)
	{
		free(clavesBloqueadas[i]);
		i++;
	}
	free (clavesBloqueadas);

	log_destroy(logPlanificador);

	list_destroy_and_destroy_elements(listaListos, (void *) ESI_destroy);
	list_destroy_and_destroy_elements(listaBloqueados,(void *)ESI_destroy);
	list_destroy_and_destroy_elements(listaFinalizados, (void*) ESI_destroy);
	list_destroy_and_destroy_elements(deadlockeados, (void *) DEADLOCK_destroy);
	queue_destroy_and_destroy_elements(colaListos,(void *)ESI_destroy);
	list_destroy_and_destroy_elements(listaRecursos, (void *) recursoDestroy);
}



