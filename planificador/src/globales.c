#include "planificador.h"

void recursoDestroy(t_recurso * recurso);
void lanzarConsola();



//char* rutaLog = "/home/utnso/workspace/tp-2018-1c-El-Rejunte/planificador/Debug/logs";
char * KEY_PUERTO_CLIENTE = "PUERTO_CLIENTE";
char * KEY_ALGORITMO_PLANIFICACION = "ALGORITMO_PLANIFICACION";
char * KEY_ESTIMACION_INICIAL = "ESTIMACION_INICIAL";
char * KEY_IP_COORDINADOR = "IP_COORDINADOR";
char * KEY_PUERTO_COORDINADOR = "PUERTO_COORDINADOR";
char * KEY_IP = "IP";
char * KEY_PUERTO = "PUERTO";
char * KEY_CLAVES_BLOQUEADAS = "CLAVES_BLOQUEADAS";
char * KEY_CONSTANTE_ESTIMACION = "CONSTANTE_ESTIMACION";
char * RUTA_CONFIGURACION = "/home/utnso/workspace/tp-2018-1c-El-Rejunte/planificador/config_planificador.cfg";
char * SJF = "SJF";
char * HRRN = "HRRN";
char * SJFConDesalojo = "SJFConDesalojo";
char * HRRNConDesalojo = "HRRNConDesalojo";
char * ipPropia = "127.0.0.2";
char * puertoPropio = "8080";
int CONTINUAR = 1;
int FINALIZAR = 2;
int socketDeEscucha = 1; //conectarComoServidor(logPlanificador, ip, puerto,1);
uint32_t idESI = 0;
uint32_t GET = 0;

// CONSOLA


char * PAUSEAR_PLANIFICACION= "pausear_planificacion";
char* REANUDAR_PLANIFICACION = "reanudar_planificacion";
char* BLOQUEAR_ESI = "bloquear_esi";
char* DESBLOQUEAR_ESI = "desbloquear_esi";
char* LISTAR_POR_RECURSO = "listar_por_recurso";
char* KILL_ESI = "kill_esi";
char* STATUS_ESI = "status_esi";
char* COMPROBAR_DEADLOCK = "comprobar_deadlock";
pthread_mutex_t mutexColaListos = PTHREAD_MUTEX_INITIALIZER;


void estimarProximaRafaga(ESI * proceso ){


	log_info (logPlanificador, "EL ESI DE CLAVE %s TIENE RAFAGA ANTERIOR DE %d", proceso->id, proceso->rafagaAnterior);
	if(proceso -> rafagaAnterior == 0){

		proceso -> estimacionSiguiente = 0;

	} else{

		proceso->estimacionSiguiente = ((alfa*proceso->estimacionAnterior)+(1-alfa)*proceso->rafagaAnterior);

	}
	log_info(logPlanificador,"un tiempo estimado \n");

}


bool compararClaves (ESI * esi){

		log_info(logPlanificador, "entra a la comparacion de claves");

		if(esi->id ==claveActual){

			return true;

		} else{

			return false;

		}

}

void comprobarDeadlock(){ //todo modificar DL para cola de bloqueados

/*	int contador = 0;

	while ( contador <= list_size(listaListos) ){

		ESI * nuevo = list_get(listaListos, contador);
		t_deadlockeados * posibleDeadlock = malloc (sizeof(deadlockeados));

		int contador2;

		while (contador2 <= list_size(listaListos)){

			ESI * Aux = list_get(listaListos, contador2);
			if ( (Aux -> recursoPedido == nuevo -> recursosAsignado) && (Aux -> recursosAsignado == nuevo -> recursoPedido)){

				posibleDeadlock->clave = nuevo->id;
				list_add(posibleDeadlock->ESIasociados, Aux->id);
			}
			ESI_destroy(Aux);
			contador2++;

		}
		ESI_destroy(nuevo);
		DEADLOCK_destroy(posibleDeadlock);
		contador++;


	} //todo estoy metiendo repetidos los deadlock porque no compruebo si la clave ya está dentro de otra estructura deadlock. ARREGLAR

*/

}


void lanzarConsola(){

	char* linea;

	while(1){

		printf("¿Que operacion desea hacer?\n");
		printf("Pausear_planificacion \n");
		printf("Reanudar_planificacion \n");
		printf("Bloquear_ESI \n");
		printf("Desbloquear_ESI \n");
		printf("Listar_por_recurso \n");
		printf("Kill_ESI \n");
		printf("Status_ESI \n");
		printf("Comprobar_Deadlock \n");
		printf("Salir");
		printf("Ingrese el nombre de la opcion, incluyendo el guion bajo");
		linea = readline(">");

		if (string_equals_ignore_case(linea, PAUSEAR_PLANIFICACION))  //Hermosa cadena de if que se viene
		{
			//todo funcion que pausee
			// Conectar con planificador e indicarle que el ESI se pausea pero NO se bloquea, solo para temporalmente la planificacion
			break;
		}
		else if (string_equals_ignore_case(linea,REANUDAR_PLANIFICACION))
		{
			//todo funcion que reanude
			//reanuda lo pauseado -> podrian ser la misma opcion con pausear porque la comprobacion se hace igual
			break;
		}
		else if (string_equals_ignore_case(linea, BLOQUEAR_ESI))
		{
			//todo funcion que bloquee
			linea = readline("CLAVE:");
			//mandar clave al planificador
			// Manda al ESI a la cola de bloqueados del recurso CLAVE si el mismo esta en ejecucion o listo para ejecurar
			break;
		}
		else if (string_equals_ignore_case(linea,DESBLOQUEAR_ESI))
		{
			//todo funcion que desbloquee al primer bloqueado de la cola del recurso
			linea = readline("CLAVE:");
			break;
		}
		else if (string_equals_ignore_case(linea, LISTAR_POR_RECURSO)){

			//todo funcion que liste
			linea = readline("RECURSO:");
			//Aca agarra la clave del recurso a usar y muestra por pantalla los recursos que estan esperando usarlo
			break;
		}
		else if (string_equals_ignore_case(linea, KILL_ESI))
		{
			//Mata al ESI y libera sus recursos
			//todo funcion asesina
			linea = readline("CLAVE:");
			break;
		}
		else if (string_equals_ignore_case(linea, STATUS_ESI))
		{
			//todo funcion explicacion pendiente
			linea = readline("CLAVE:");
			//Explicacion pendiente
			break;
		}
		else if (string_equals_ignore_case(linea, COMPROBAR_DEADLOCK))
		{
			//devuelve las claves que esten generando deadlock (si es que lo hay)
			// aca va a haber una escucha del socket a la espera de la respuesta del plani
			break;
		}
		else if (string_equals_ignore_case(linea, "salir"))
		{
			break;
		}
		else
		{
			printf("escribi bien, gil");
			break;
		}

	}
}


ESI * crearESI(uint32_t clave){

	ESI * nuevoESI = malloc(sizeof(ESI));
	nuevoESI->id = clave;
	nuevoESI->estimacionAnterior= estimacionInicial;
	nuevoESI-> bloqueadoPorUsuario = false;
	nuevoESI-> rafagaAnterior = 0;
	nuevoESI-> estimacionSiguiente = 0;
	nuevoESI->rafagasRealizadas =0;
	nuevoESI-> tiempoEspera = 0;
	nuevoESI->recursosAsignado= list_create();
	nuevoESI->recursoPedido = NULL;
	nuevoESI->proximaOperacion = -1;
	nuevoESI->recienLlegado = true;

	return nuevoESI;

}

void escucharNuevosESIS(){

	while(1){
		uint32_t socketESINuevo = escucharCliente(logPlanificador,socketDeEscucha);
		send(socketESINuevo,&socketESINuevo,sizeof(uint32_t),0);
		ESI * nuevoESI = crearESI(socketESINuevo);
		list_add(listaListos,nuevoESI);

		pthread_mutex_lock(&mutexColaListos); //pongo mutex para que no se actualice la cola mientras agrego un ESI.
		if(string_equals_ignore_case(algoritmoDePlanificacion,SJF) || string_equals_ignore_case(algoritmoDePlanificacion,SJFConDesalojo)){
			armarColaListos();
		} else if(string_equals_ignore_case(algoritmoDePlanificacion, HRRN) || string_equals_ignore_case(algoritmoDePlanificacion,HRRNConDesalojo)){
			armarCola();
		}
		pthread_mutex_unlock(&mutexColaListos);

		ESI_destroy(nuevoESI);
	}
}


t_recurso * crearRecurso (char * id){

	t_recurso * nuevo = malloc(sizeof(t_recurso));
	nuevo->clave = id;
	nuevo->ESIEncolados = queue_create();
	return nuevo;
}

void ESI_destroy(ESI * estructura)
{
		list_destroy_and_destroy_elements(estructura->recursosAsignado, (void * ) recursoDestroy);
		free(estructura->recursoPedido);
		free(estructura);
}


void recursoDestroy(t_recurso * recurso)
{
	free(recurso->clave);
	queue_destroy_and_destroy_elements(recurso->ESIEncolados, (void *) ESI_destroy);
	free(recurso);

}



void DEADLOCK_destroy(t_deadlockeados * ESI){

	list_destroy_and_destroy_elements(ESI->ESIasociados, (void *) free);
	free(ESI);

}

bool validarPedido (char * recurso, ESI * ESIValidar){

	int i = 0;
	bool encontrado = false;
	bool retorno = false;

	while(list_size(listaRecursos) >= i){

			t_recurso * nuevo = list_get(listaRecursos, i);


			if(string_equals_ignore_case(recurso,nuevo->clave)){

				encontrado = true;

				if(nuevo->estado == 1 && ESIValidar->proximaOperacion == 0){ // caso recurso tomado para get
					recursoDestroy(nuevo);
					retorno = false;

				} else if (nuevo-> estado == 1 && ESIValidar-> proximaOperacion > 0){ //caso recurso tomado para set y store

					bool RecursoEncontrado = false;
					int t = 0;
					while(list_size(ESIValidar->recursosAsignado) < t){

						t_recurso * recursoAuxiliar = list_get (ESIValidar->recursosAsignado, t);

						if( string_equals_ignore_case(recursoAuxiliar->clave,nuevo->clave)){ // caso ESI tiene asignado recurso

							RecursoEncontrado = true;
							recursoDestroy(recursoAuxiliar);
							recursoDestroy(nuevo);
							retorno = true;
						}

						recursoDestroy(recursoAuxiliar);

						t++;

					} if(RecursoEncontrado == false){ // Caso ESI no tiene asignado recurso

						free(nuevo);
						retorno = false;

					}


				} else retorno = true; // caso el recurso no esta tomado

			}

			i++;

		}

		if ( encontrado == false){ // caso recurso nuevo

			t_recurso * nuevoRecurso = crearRecurso(recurso);
			list_add(listaRecursos,nuevoRecurso);
			recursoDestroy(nuevoRecurso);
			retorno = true;

		}

		return retorno;
}


/**
 *
 * Bloquea un recurso cuando se pone en uso.
 * De no existir la clave que llega, crea el recurso y lo mete en lista.
 *
 */
void bloquearRecurso (char* claveRecurso) {


	int i = 0;
	bool encontrado = false;
	while(list_size(listaRecursos) >= i || encontrado)
	{
		t_recurso * nuevoRecurso = list_get(listaRecursos,i);
		if(string_equals_ignore_case(nuevoRecurso->clave,claveRecurso))
		{
			if(nuevoRecurso->estado == 0){

				nuevoRecurso->estado = 1;
				list_replace_and_destroy_element(listaRecursos, i, nuevoRecurso, (void *) recursoDestroy);
				log_info(logPlanificador, "Recurso de clave %s bloqueado", nuevoRecurso->clave);
				recursoDestroy(nuevoRecurso);
				encontrado = true;

			} else {
				log_info(logPlanificador, " se intento bloquear un recurso ya bloqueado. Se ignora");
			}

		}
		i++;
	}

}

void desbloquearRecurso (char* claveRecurso) {

	int i = 0;
	bool encontrado = false;
	while(list_size(listaRecursos) >= i || encontrado)
	{
		t_recurso * nuevoRecurso = list_get(listaRecursos,i);
		if(string_equals_ignore_case(nuevoRecurso->clave,claveRecurso))
		{
			if(nuevoRecurso->estado == 1){ // libera el recurso y saca de la cola a SOLO UN ESI que lo estaba esperando

				nuevoRecurso->estado = 0;
				encontrado = true;
				ESI * nuevo = queue_pop(nuevoRecurso->ESIEncolados);
				list_replace_and_destroy_element(listaRecursos, i, nuevoRecurso, (void *) recursoDestroy);
				log_info(logPlanificador, "Recurso de clave %s desbloqueado", nuevoRecurso->clave);
				list_add(listaListos,nuevo);
				recursoDestroy(nuevoRecurso);

			} else {
				log_info(logPlanificador, " se intento desbloquear un recurso no bloqueado. Se ignora");
			}

		}
		i++;
	}

}

void bloquearESI(char * claveRecurso, ESI * esi){

	int i = 0;
	bool encontrado = false;
	while(i<= list_size(listaRecursos) || encontrado){

		t_recurso * recursoAuxiliar = list_get(listaRecursos, i);
		if(recursoAuxiliar->clave == claveRecurso){

			queue_push(recursoAuxiliar->ESIEncolados,esi);
			list_replace_and_destroy_element(listaRecursos,i,recursoAuxiliar, (void *) recursoDestroy);

		}
		recursoDestroy(recursoAuxiliar);

	}

}

bool recursoEnLista(char * r, t_list * lista){

	bool retorno = false;
	int i = 0;
	while( i<= list_size(lista)){
		if(string_equals_ignore_case(list_get(lista,i), r)){
			retorno = true;
		}
		i++;
	}

	return retorno;
}

void liberarRecursos(ESI * esi){

	int i=0;

	while(i < list_size(esi->recursosAsignado)){

		char * recurso = list_get(esi->recursosAsignado, i);
		desbloquearRecurso( recurso );
		free (recurso);
		i++;
	}


}

void limpiarRecienLlegados(){

	log_info(logPlanificador, "actualizando cola listos");
	t_queue * colaAuxiliar = queue_create();

	while(queue_is_empty(colaListos)){

		ESI*nuevo = queue_pop(colaListos);

		nuevo -> recienLlegado = false;

		queue_push(colaAuxiliar, nuevo);

		ESI_destroy(nuevo);

	}

	colaListos = colaAuxiliar;
	queue_destroy(colaAuxiliar);
}
