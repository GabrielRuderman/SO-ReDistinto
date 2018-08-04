#include "planificador.h"

// ---------------------------- VARIABLES GLOBALES ----------------------------------//


// -------------------------- CONFIGURACION --------------------------- //

char * KEY_PUERTO_CLIENTE = "PUERTO_CLIENTE";
char * KEY_ALGORITMO_PLANIFICACION = "ALGORITMO_PLANIFICACION";
char * KEY_ESTIMACION_INICIAL = "ESTIMACION_INICIAL";
char * KEY_IP_COORDINADOR = "IP_COORDINADOR";
char * KEY_PUERTO_COORDINADOR = "PUERTO_COORDINADOR";
char * KEY_IP = "IP";
char * KEY_PUERTO = "PUERTO";
char * KEY_CLAVES_BLOQUEADAS = "CLAVES_BLOQUEADAS";
char * KEY_CONSTANTE_ESTIMACION = "CONSTANTE_ESTIMACION";
char * KEY_IP_PROPIA = "IP";
char * KEY_PUERTO_PROPIO = "PUERTO";
char * RUTA_CONFIGURACION = "/home/utnso/workspace/tp-2018-1c-El-Rejunte/planificador/config_planificador.cfg";
char * SJF = "SJF";
char * HRRN = "HRRN";
char * SJFConDesalojo = "SJFConDesalojo";
char * HRRNConDesalojo = "HRRNConDesalojo";



// ----------------------------------- SOCKETS ------------------------------------ //


int CONTINUAR = 1;
int FINALIZAR = 2;
int socketDeEscucha;
uint32_t idESI = 0;
uint32_t GET = 0;



// ------------------------------ CONSOLA ---------------------------------- //



bool pausearPlanificacion = false;
bool matarESI=false;
bool bloquearESIActual = false;
bool salir = false;
int claveMatar = -1;
char * PAUSEAR_PLANIFICACION= "pausear_planificacion";
char* REANUDAR_PLANIFICACION = "reanudar_planificacion";
char* BLOQUEAR_ESI = "bloquear_esi";
char* DESBLOQUEAR_CLAVE = "desbloquear_clave";
char* LISTAR_POR_RECURSO = "listar_por_recurso";
char* KILL_ESI = "kill_esi";
char* STATUS_CLAVE = "status_clave";
char* COMPROBAR_DEADLOCK = "comprobar_deadlock";
char* LISTAR_FINALIZADOS = "listar_finalizados";
int idSalir = -50;


// --------------------------- SEMAFOROS --------------------------------- //


pthread_mutex_t mutexColaListos = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexAsesino = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexComunicacion = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexPauseo  = PTHREAD_MUTEX_INITIALIZER;




// --------------------------- FUNCIONES --------------------------------- //



// --------------------------- DEADLOCK ---------------------------------- //



bool compararClaves (ESI * esi){

		if(esi->id ==claveActual){

			return true;

		} else{

			return false;

		}

}


void comprobarDeadlock(){

	int contador = 0;
	t_list * dl = list_create();

	log_info (logPlanificador, "Chequeando existencia de deadlock");
	printf("Chequeando existencia de deadlock. De no listarse nada, no existe el DL \n");

	while ( contador < list_size(listaRecursos) ){ // por cada recurso

		t_recurso * recursoAnalizar = list_get(listaRecursos, contador);

		log_debug(logPlanificador,"Analizando desde clave %s", recursoAnalizar->clave);

		int contador2 = 0;

		t_list * listaColaRecurso = list_create();

		while(!queue_is_empty(recursoAnalizar->ESIEncolados)){

			list_add(listaColaRecurso, queue_pop(recursoAnalizar->ESIEncolados));

		}

		while (contador2 < list_size(listaColaRecurso)){ // tomo su primer ESI bloqueado

			ESI * Aux = list_get(listaColaRecurso, contador2);

			int contador3 = 0;

			while(list_size(Aux -> recursosAsignado) > contador3){ // y chequeo si para cada recurso asignado del mismo, hay un ESI en la cola de bloqueados del mismo que tenga asignada la clave que lo está bloqueando

				char * recursoAsignado = list_get(Aux->recursosAsignado,contador3);

				chequearDependenciaDeClave(recursoAnalizar->clave, recursoAsignado, Aux->id, dl );

				if(list_size(dl) > 0){ // en este caso se encontro el DL

					int cont = 0;

					printf("DEADLOCK formado por los siguientes ESI: \n");
					log_trace(logPlanificador, "DEADLOCK formado por los siguientes ESI:");

					while(list_size(dl)> cont){

						int * ESI = list_get(dl,cont);

						log_trace(logPlanificador, "ID: %d", *ESI);


						printf("ESI ID: %d \n", *ESI);

						cont ++;

					}

					list_clean(dl); // limpio para encontrar otros DL

				}
				contador3++;

			}
			contador2++;

		}

		int cont4 = 0;

		while(list_size(listaColaRecurso)> cont4){

			queue_push(recursoAnalizar->ESIEncolados, list_get(listaColaRecurso, cont4));
			cont4 ++;

		}

		list_destroy(listaColaRecurso);

		contador++;

	}



}

void chequearDependenciaDeClave(char * recursoOriginal, char * recursoESI, int idESI, t_list * listaDL){ //  hay que hacer una busqueda circular entre claves

	t_recurso * recurso = traerRecurso(recursoESI); // busco el recurso que iguale la clave del ESI

	if(recurso == NULL){ // Si no lo encuentra, no mando nada, no deberia pasar, logueo para estar al tanto

		log_error(logPlanificador,"CASO RARO: no se encontro el recurso que tiene asignado un ESI, de clave %s", recursoESI);

	} else { // al encontrar el recurso, tengo que chequear por cada ESI bloqueado de la clave, si su asignado coincide con la clave del recursoOriginal

		int i = 0;

		t_list * listaColaESI = list_create();

		while(!queue_is_empty(recurso->ESIEncolados)){
			list_add(listaColaESI, queue_pop(recurso->ESIEncolados));

		}

		bool DLEncontrado = false;

		if( list_size(listaDL) > 0){ //encontro un DL en una ejecucion anterior de la funcion

			list_add(listaDL, &idESI);

		} else {

			while ( list_size(listaColaESI)> i && !DLEncontrado ){ // por cada ESI encolado

				ESI * esi = list_get(listaColaESI, i); // tomo de a uno

				int t = 0;

				while (list_size(esi->recursosAsignado) > t && !DLEncontrado){

					char * recurso = list_get (esi->recursosAsignado,t); // saco un recurso asignado

					if(string_equals_ignore_case(recurso,recursoOriginal)){ // es igual al recurso original?

						list_add(listaDL,&idESI);
						list_add(listaDL, &esi->id);
						DLEncontrado = true;

					} else { // si no son iguales, va a buscar coincidencia en los recursos que ese esi tiene asignados

						chequearDependenciaDeClave(recursoOriginal, recurso, esi->id, listaDL); //recursivo, el recu original y el nuevo

						if(list_size(listaDL)> 0){ // si metio algo en lista, quiere decir que encontro espera circular, que dada la unicidad de ESI por recurso, va a generar un ciclo de un solo esi por recurso
							list_add(listaDL, &idESI); // agrego el primer esi, que por recursividad, seria el tercero del dl
							DLEncontrado = true; // por unicidad de recursos, no puede haber mas de un ciclo asociado a un recurso.
						}

					}

					t++;


				}

				i++;

			}
		}

		int cont = 0;

		while(list_size(listaColaESI)> cont){

			queue_push(recurso->ESIEncolados, list_get(listaColaESI, cont));
			cont ++;

		}

		list_destroy(listaColaESI);

	}

}


// ----------------------------------- UTILIDADES Y PLANIFICACION --------------------------------- //

void estimarProximaRafaga(ESI * proceso ){

	if(!proceso->recienDesalojado){
		if(proceso->rafagaAnterior > 0){
			float constante = ((float) alfa) /100;
			float anterior = (float)  proceso->rafagaAnterior;
			float estimacionAnterior = (float)  proceso->estimacionAnterior;

			proceso->estimacionSiguiente = ((constante * anterior)+(1-constante)* estimacionAnterior);

			log_debug(logPlanificador,"un tiempo estimado: %.6f para ESI : %d", proceso->estimacionSiguiente, proceso->id);

		}else{

			proceso->estimacionSiguiente = estimacionInicial;
			log_debug(logPlanificador,"Tiempo estimado: %.6f (primera estimacion) para ESI : %d", proceso->estimacionSiguiente, proceso->id);

		}
	}

}


void liberarUnRecurso ( ESI * esi ){

	int i = 0;
	log_info(logPlanificador, " Se libera clave %s debido a un STORE del ESI de id %d", esi->recursoPedido, esi->id);

	while (i<list_size(esi->recursosAsignado)){

		if(string_equals_ignore_case(esi->recursoPedido, (char *) list_get(esi->recursosAsignado,i))){

			list_remove(esi->recursosAsignado, i);

		}

		i++;

	}

	desbloquearRecurso(esi->recursoPedido);
}


t_recurso * traerRecurso (char * clave){


	t_recurso * recurso;
	bool encontrado = false;
	int i = 0;

	while(list_size(listaRecursos) > i && !encontrado){

		recurso = list_get(listaRecursos, i);

		if(string_equals_ignore_case(clave,recurso->clave)){

			encontrado = true;
		}
		i++;
	}

	if(encontrado == false){

		recurso = NULL;
	}

	return recurso;
}

bool recursoEnLista(ESI * esi){

	bool retorno = false;
	int i = 0;
	while( i< list_size(esi->recursosAsignado) && !retorno){
		char * r = list_get(esi->recursosAsignado, i);

		if(string_equals_ignore_case(r, esi->recursoPedido)){
			retorno = true;
		}
		i++;
	}

	return retorno;
}


void buscarYBloquearESI(int clave, char * recurso){

	log_info(logPlanificador, " Se procede a buscar ESI en cola listos");

	ESI * esiEncontrado;
	int t = 0;
	bool encontrado = false;
	t_list * colaAuxiliar = list_create();

	while(!queue_is_empty(colaListos)){

		list_add(colaAuxiliar, queue_pop(colaListos));

	}

	while(list_size(colaAuxiliar) > t && !encontrado){

		esiEncontrado = list_get(colaAuxiliar,t);

		if(esiEncontrado -> id == clave){

			int x = 0;
			encontrado = true;
			bool claveAsignada = false;

			while(list_size(esiEncontrado->recursosAsignado) > x)
			{
				if(string_equals_ignore_case(recurso, list_get(esiEncontrado->recursosAsignado,x))){

					claveAsignada = true;
					printf("El ESI de ID %d ya tiene tomada la clave %s. No puede realizarse el pedido", esiEncontrado->id, recurso);
					log_error(logPlanificador,"El ESI de ID %d ya tiene tomada la clave %s. No puede realizarse el pedido de la consola", esiEncontrado->id, recurso);

				}
				x++;

			}
			if(!claveAsignada){

				list_remove(colaAuxiliar,t);
				log_trace(logPlanificador, " El ESI de ID %d fue sacado de la cola de listos por la consola y se procede a bloquearlo", esiEncontrado->id);
				esiEncontrado->bloqueadoPorConsola = true;
				sem_wait(&semContadorColaListos); // Le saco un ESI a la cola, le bajo al contador de la misma. EN EL CASO PARTICULAR QUE LA CLAVE NO ESTE BLOQUEADA, BloquearRecurso() se encarga del sem_post()
				bloquearESI(recurso,esiEncontrado);

			}

		}

		t++;

	}

	while(!list_is_empty(colaAuxiliar)){

		queue_push(colaListos, list_remove(colaAuxiliar, 0));

	}

	if(!encontrado){
		printf("La ID del ESI ingresada no pudo ser hallada. Es invalida o bien ya esta bloqueada en cola de alguna clave");
		log_error(logPlanificador,"La ID ingresada por consola no fue encontrada en cola listos");

	}


}




extern void cargarValor(char* clave, char* valor){

	int i = 0;
	bool encontrado = false;
	log_info(logPlanificador, "Cargando valor %s a clave %s", valor, clave);
	while(i< list_size(listaRecursos) && !encontrado){

		t_recurso * auxiliar = list_get(listaRecursos, i);

		if(string_equals_ignore_case(auxiliar->clave, clave)){
			free(auxiliar->valor);
			auxiliar->valor = string_new();
			string_append(&(auxiliar->valor),valor);
			encontrado = true;
		}

		i++;


	}

	if(!encontrado){

		log_error(logPlanificador,"No se encontro la clave especificada");
	}
}

bool buscarEnBloqueados (int clave){

	int i = 0;
	bool encontrado = false;

	while (list_size( listaRecursos) > i && !encontrado){

		t_recurso * recu = list_get(listaRecursos, i);

		int r = 0;

		t_list * listaAux = list_create();
		while(!queue_is_empty(recu->ESIEncolados)){

			list_add (listaAux, queue_pop(recu->ESIEncolados));

		}


		while(list_size(listaAux) > r && !encontrado){

			ESI * aux = list_get(listaAux, r); // avanza entre bloqueados

			if(aux->id == clave){ // si encuentra, libera recursos y destruye al ESI

				pthread_mutex_lock(&mutexComunicacion);
				log_debug(logPlanificador, "Le informo al ESI de ID: %d que sera abortado", aux->id);

				uint32_t abortar = -2;
				send(aux->id, &abortar, sizeof(uint32_t), 0);
				pthread_mutex_unlock(&mutexComunicacion);

				encontrado = true;
				liberarRecursos(aux);
				list_add(listaFinalizados,aux);
				list_remove(listaAux, r); // saco el ESI porque va a finalizados
				claveMatar=-1;
			}

			r++;

		}
		while(!list_is_empty(listaAux)){
			ESI * auxiliar = list_remove(listaAux,0);
			queue_push(recu->ESIEncolados,auxiliar);
		}

		i++;
	}



	return encontrado;
}



// ----------------------------------- CONSOLA ----------------------------------- //


void lanzarConsola(){

	char* linea;

	while(!salir){

		printf("¿Que operacion desea hacer?\n");
		printf("1. Pausear_planificacion \n");
		printf("2. Reanudar_planificacion \n");
		printf("3. Bloquear_ESI \n");
		printf("4. Desbloquear_clave \n");
		printf("5. Listar_por_recurso \n");
		printf("6. Kill_ESI \n");
		printf("7. Status_clave \n");
		printf("8. Comprobar_Deadlock \n");
		printf("9. Listar_finalizados \n");
		printf("10. Salir \n");
		printf("Ingrese el numero o el nombre de la opcion, incluyendo el guion bajo \n");
		linea = readline(">");

		if (string_equals_ignore_case(linea, PAUSEAR_PLANIFICACION) || string_equals_ignore_case(linea, "1"))  //Hermosa cadena de if que se viene
		{
			log_info(logPlanificador, "Comando ingresado por consola : Pausar planificacion", linea);
			if(!pausearPlanificacion){
				pthread_mutex_lock(&mutexPauseo);
				pausearPlanificacion = true;
				sem_wait(&semPausarPlanificacion);
			} else printf("La planificacion ya estaba pausada!");
			free(linea);
		}
		else if (string_equals_ignore_case(linea,REANUDAR_PLANIFICACION)  || string_equals_ignore_case(linea, "2"))
		{
			log_info(logPlanificador, "Comando ingresado por consola : Reanudar planificacion", linea);
			if(pausearPlanificacion){
				pausearPlanificacion = false;
				printf("Planificacion reanudada");
				pthread_mutex_unlock(&mutexPauseo);
				sem_post(&semPausarPlanificacion);
			} else printf("La planificacion no estaba pausada!");
			free(linea);
		}
		else if (string_equals_ignore_case(linea, BLOQUEAR_ESI)  || string_equals_ignore_case(linea, "3"))
		{
			log_info(logPlanificador, "Comando ingresado por consola : bloquear ESI ", linea);
			linea = readline("ID ESI:");
			log_info(logPlanificador, "ID ESI ingresada por consola : %s", linea);

			int clave = atoi(linea);

			free(linea);

			linea = readline("CLAVE RECURSO:");
			log_info(logPlanificador, "Clave recurso ingresada por consola : %s", linea);

			sem_wait(&semComodinColaListos);
			pthread_mutex_lock(&mutexColaListos);

			if( claveActual==clave){

				log_info(logPlanificador, "La clave coincide con la del ESI acualmente ejecutandose");
				claveParaBloquearESI = clave;
				string_append(&claveParaBloquearRecurso, linea);

				printf("Bloqueando ESI... (debe terminar su instruccion antes de ser bloqueado) \n");
				bloquearESIActual = false;

			} else {

				buscarYBloquearESI(clave, linea);

			}
			pthread_mutex_unlock(&mutexColaListos);
			sem_post(&semComodinColaListos);

			free(linea);

		}
		else if (string_equals_ignore_case(linea,DESBLOQUEAR_CLAVE)  || string_equals_ignore_case(linea, "4"))
		{
			log_info(logPlanificador, "Comando ingresado por consola : DESBLOQUEAR_CLAVE");

			linea = readline("CLAVE RECURSO:");
			log_info(logPlanificador, "Clave recurso ingresada por consola : %s", linea);

			desbloquearRecurso(linea);

			free(linea);


		}
		else if (string_equals_ignore_case(linea, LISTAR_POR_RECURSO)  || string_equals_ignore_case(linea, "5")){

			log_info(logPlanificador, "Comando ingresado por consola : LISTAR_POR_RECURSO", linea);
			linea = readline("CLAVE RECURSO:");
			log_info(logPlanificador, "Clave recurso ingresada por consola : %s", linea);

			listarBloqueados(linea);
			free(linea);
		}
		else if (string_equals_ignore_case(linea, KILL_ESI)  || string_equals_ignore_case(linea, "6"))
		{
			log_info(logPlanificador, "Comando ingresado por consola : KILL_ESI", linea);
			linea = readline("ID ESI:");
			log_info(logPlanificador, "Clave esi ingresada por consola : %s", linea);

			int clave = atoi(linea);

			if(clave == claveActual){

				log_info(logPlanificador, "La clave del esi ejecutandose es igual a la que se quiere matar");

				pthread_mutex_lock(&mutexAsesino);

				matarESI = true;

				pthread_mutex_unlock(&mutexAsesino);

				printf("esperando a que el ESI pueda ser matado (debe terminar su instruccion antes) \n");


			}else {

				log_info(logPlanificador, "La clave a matar no es igual a la que se esta ejecutando. Buscando... ");
				claveMatar = clave;
				seekAndDestroyESI(clave);
			}
			free(linea);

		}else if (string_equals_ignore_case(linea, STATUS_CLAVE)  || string_equals_ignore_case(linea, "7"))
		{
			log_info(logPlanificador, "Comando ingresado por consola : STATUS_CLAVE", linea);
			linea = readline("CLAVE:");
			log_info(logPlanificador, "Clave ingresada por consola : %s", linea);
			statusClave(linea);
			free(linea);

		}	else if (string_equals_ignore_case(linea, COMPROBAR_DEADLOCK)  || string_equals_ignore_case(linea, "8"))
		{
			log_info(logPlanificador, "Comando ingresado por consola  : COMPROBAR_DEADLOCK");
			comprobarDeadlock(); // hace printf en la funcion
			free(linea);

		} else if ( string_equals_ignore_case(linea, LISTAR_FINALIZADOS)  || string_equals_ignore_case(linea, "9")){

			log_info(logPlanificador, "Comando ingresado por consola  : LISTAR_FINALIZADOS");
			int i = 0;
			while(i< list_size(listaFinalizados)){

				ESI * esi = list_get (listaFinalizados, i);

				printf("El ESI numero %d es de clave %d \n", i, esi->id);
				i++;
			}

			free(linea);

		}
		else if (string_equals_ignore_case(linea, "salir")  || string_equals_ignore_case(linea, "10"))
		{
			printf("Cerrando planificador");
			log_info(logPlanificador, "Comando ingresado por consola  : salir");
			pthread_mutex_lock(&mutexColaListos);

			salir = true;
			ESI * ESISalir = crearESI(idSalir);

			queue_destroy_and_destroy_elements(colaListos, (void *) ESI_destroy);
			if(string_equals_ignore_case(algoritmoDePlanificacion, SJF) || string_equals_ignore_case(algoritmoDePlanificacion, SJFConDesalojo)){

				colaListos=queue_create();
				armarColaListos(ESISalir);
				sem_post(&semContadorColaListos);
			} else if(string_equals_ignore_case(algoritmoDePlanificacion, HRRN) || string_equals_ignore_case(algoritmoDePlanificacion, HRRNConDesalojo)){

				colaListos=queue_create();
				armarColaListos(ESISalir);
				sem_post(&semContadorColaListos);

			}
			pthread_mutex_unlock(&mutexColaListos);
			pthread_cancel(hiloEscuchaESI);
			if(pausearPlanificacion){

				pausearPlanificacion = false;
				printf("Planificacion reanudada");
				pthread_mutex_unlock(&mutexPauseo);
				sem_post(&semPausarPlanificacion);
			}
			sem_wait(&semSalir);
			free(linea);

			break;
		}
		else
		{
			log_info(logPlanificador, "Comando ingresado por consola: %s no reconocido", linea);
			printf("Comando no reconocido");
			free(linea);
		}

	}

	log_error(logPlanificador, " Cerrando consola ");

}


// ------------------------------ CREADORES Y DESTRUCTORES ------------------------------- //



ESI * crearESI(uint32_t clave){

	log_info(logPlanificador, "Inicializando ESI");

	ESI * nuevoESI = malloc(sizeof(ESI));
	nuevoESI->id = clave;
	nuevoESI->estimacionAnterior= estimacionInicial;
	nuevoESI-> bloqueadoPorClave = false;
	nuevoESI->bloqueadoPorConsola = false;
	nuevoESI-> rafagaAnterior = 0;
	nuevoESI-> estimacionSiguiente = 0;
	nuevoESI->rafagasRealizadas =0;
	nuevoESI-> tiempoEspera = 0;
	nuevoESI->recursosAsignado= list_create();
	nuevoESI->recursoPedido = string_new();
	nuevoESI->proximaOperacion = -1;
	nuevoESI->recienLlegado = true;
	nuevoESI->recienDesalojado = false;
	nuevoESI->recienDesbloqueadoPorRecurso = false;

	return nuevoESI;

}


t_recurso * crearRecurso (char * id){

	log_info(logPlanificador, "Inicializando recurso");
	t_recurso * nuevo = malloc(sizeof(t_recurso));
	nuevo->estado = 0;
	nuevo->clave = string_new();
	string_append(&(nuevo->clave), id);
	nuevo->operacion = 0;
	nuevo->valor = string_new();
	nuevo->ESIEncolados = queue_create();
	return nuevo;
}

void ESI_destroy(ESI * estructura)
{
		log_info(logPlanificador, "destruyendo ESI clave : %d", estructura->id);
		list_destroy_and_destroy_elements(estructura->recursosAsignado, (void * ) free);
		free(estructura->recursoPedido);
		free(estructura);
}


void recursoDestroy(t_recurso * recurso)
{
	log_info(logPlanificador, "destruyendo recurso clave : %s", recurso->clave);

	free(recurso->clave);
	free(recurso->valor);
	queue_destroy_and_destroy_elements(recurso->ESIEncolados, (void *) ESI_destroy);
	free(recurso);

}


// ----------------------------------- COMUNICACIONES --------------------------------- //



void escucharNuevosESIS(){

	log_info(logPlanificador, "Inicio hilo de escucha de ESIS");

	while(!salir){

		uint32_t socketESINuevo = escucharCliente(logPlanificador,socketDeEscucha);

		if(salir){
			break;
		}
		log_info(logPlanificador, "se escucho un nuevo ESI");

		send(socketESINuevo,&socketESINuevo,sizeof(uint32_t),0);
		ESI * nuevoESI = crearESI(socketESINuevo);

		pthread_mutex_lock(&mutexColaListos); //pongo mutex para que no se actualice la cola mientras agrego un ESI.
		if(string_equals_ignore_case(algoritmoDePlanificacion,SJF) || string_equals_ignore_case(algoritmoDePlanificacion,SJFConDesalojo)){
			armarColaListos(nuevoESI);
			sem_post(&semContadorColaListos);
		} else if(string_equals_ignore_case(algoritmoDePlanificacion, HRRN) || string_equals_ignore_case(algoritmoDePlanificacion,HRRNConDesalojo)){
			armarCola(nuevoESI);
			sem_post(&semContadorColaListos);
		}
		pthread_mutex_unlock(&mutexColaListos);

	}
}



bool validarPedido (char * recurso, ESI * ESIValidar){

	int i = 0;
	bool encontrado = false;
	bool retorno = false;

	while(list_size(listaRecursos) > i){

			t_recurso * nuevo = list_get(listaRecursos, i);

			if(string_equals_ignore_case(recurso,nuevo->clave)){

				encontrado = true;

				if(nuevo->estado == 1 && ESIValidar->proximaOperacion == 1){ // caso recurso tomado para get

					log_trace(logPlanificador,"Se quiere hacer un GET de un recurso tomado. Se le niega permiso al ESI");
					nuevo = NULL;
					retorno = false;

				} else if (nuevo-> estado == 1 && ESIValidar-> proximaOperacion > 1){ //caso recurso tomado para set y store

					log_trace(logPlanificador,"Se quiere hacer un SET o STORE. Se chequea si la clave esta asignada");
					bool RecursoEncontrado = false;
					int t = 0;
					while(list_size(ESIValidar->recursosAsignado) > t){

						char * recursoAuxiliar = list_get (ESIValidar->recursosAsignado, t);

						if( string_equals_ignore_case(recursoAuxiliar,nuevo->clave)){ // caso ESI tiene asignado recurso

							log_trace(logPlanificador,"Clave asignada. Tiene permiso");
							RecursoEncontrado = true;
							nuevo->operacion = ESIValidar->proximaOperacion; //mete si hace set o get
							retorno = true;
						}

						recursoAuxiliar= NULL;

						t++;

					} if(RecursoEncontrado == false){ // Caso ESI no tiene asignado recurso

						log_error(logPlanificador," No se puede hacer un SET o STORE de un recurso no asignado ");
						nuevo = NULL;
						retorno = false;

					}


				} else if(nuevo->estado==0 && ESIValidar->proximaOperacion > 1){ // caso el recurso no esta tomado y quiere hacer SET o STORE

					log_error(logPlanificador,"SET o STORE de recurso no tomado, no se permite");
					retorno = false;
				} else {

					log_trace(logPlanificador,"GET de un recurso no tomado. Se permite");
					retorno = true;

				}

			}

			i++;

		}

		if ( encontrado == false){ // caso recurso nuevo

			if (ESIValidar -> proximaOperacion == 1){

				log_trace(logPlanificador,"Se crea recurso nuevo porque no fue encontrada la clave pedida. Tiene permiso");
				t_recurso * nuevoRecurso = crearRecurso(recurso);
				nuevoRecurso -> operacion =  0;
				list_add(listaRecursos,nuevoRecurso);
				retorno = true;
			} else if (ESIValidar-> proximaOperacion > 1){
				log_error (logPlanificador, " Se pide set o store de una clave no existente, abortando esi ");
				retorno = false;

			}

		}

		return retorno;
}


// ------------------------------- FUNCIONES VARIAS DE CONSOLA ---------------------------- //

/**
 *
 * Bloquea un recurso cuando se pone en uso.
 * De no existir la clave que llega, crea el recurso y lo mete en lista.
 *
 */

void bloquearRecurso (char* claveRecurso) {


	int i = 0;
	bool encontrado = false;
	while(list_size(listaRecursos) > i && !encontrado)
	{
		t_recurso * nuevoRecurso = list_get(listaRecursos,i);
		if(string_equals_ignore_case(nuevoRecurso->clave,claveRecurso))
		{
			if(nuevoRecurso->estado == 0){

				nuevoRecurso->estado = 1;
				log_info(logPlanificador, "Recurso de clave %s bloqueado", nuevoRecurso->clave);
				encontrado = true;

			} else {
				log_info(logPlanificador, " Se intento bloquear un recurso ya bloqueado. Se ignora");
			}

		}
		i++;
	}

}



void desbloquearRecurso (char* claveRecurso) {

	log_info(logPlanificador, "Se intenta desbloquear un recurso clave %s", claveRecurso);
	int i = 0;
	bool encontrado = false;
	while(list_size(listaRecursos) > i && !encontrado)
	{
		t_recurso * nuevoRecurso = list_get(listaRecursos,i);
		if(string_equals_ignore_case(nuevoRecurso->clave,claveRecurso))
		{
			if(nuevoRecurso->estado == 1){ // libera el recurso y saca de la cola a SOLO UN ESI que lo estaba esperando
				encontrado = true;
				ESI * nuevo = queue_pop(nuevoRecurso->ESIEncolados);
				if(nuevo != NULL){
					nuevo->recienDesbloqueadoPorRecurso = true;
					list_add(nuevo->recursosAsignado, nuevoRecurso->clave);
					pthread_mutex_lock(&mutexColaListos);
					if(string_equals_ignore_case(algoritmoDePlanificacion, SJF) || string_equals_ignore_case(algoritmoDePlanificacion, SJFConDesalojo)){
						armarColaListos(nuevo);
						sem_post(&semContadorColaListos);

					}else if (string_equals_ignore_case(algoritmoDePlanificacion, HRRNConDesalojo) || string_equals_ignore_case(algoritmoDePlanificacion, HRRN)){
						armarCola(nuevo);
						sem_post(&semContadorColaListos);
					}
					pthread_mutex_unlock(&mutexColaListos);

				} else {
					log_info(logPlanificador, " La clave no tenia ESIS encolados");
					log_error(logPlanificador, " OJO si la liberacion fue hecha por consola, puede estarse liberando una clave tomada por algun ESI");
					nuevoRecurso->estado = 0;
				}

				log_debug(logPlanificador, "Recurso de clave %s desbloqueado", nuevoRecurso->clave);


			} else {
				log_info(logPlanificador, " Se intento desbloquear un recurso no bloqueado. Se ignora");
				encontrado = true;
			}

		}
		i++;
	}

	if(!encontrado){
		log_error(logPlanificador, "Clave no reconocida");
		printf("Clave no existente");
	}

}


void bloquearESI(char * claveRecurso, ESI * esi){

	int i = 0;
	bool encontrado = false;
	while(i< list_size(listaRecursos) && !encontrado){

		t_recurso * recursoAuxiliar = list_get(listaRecursos, i);
		if(string_equals_ignore_case(recursoAuxiliar->clave,claveRecurso)){

			if(recursoAuxiliar->estado == 1){
			esi->bloqueadoPorClave = true;
			queue_push(recursoAuxiliar->ESIEncolados,esi);
			encontrado = true;
			log_debug(logPlanificador, "ESI de ID %d en cola de recurso de clave : %s", esi->id, recursoAuxiliar->clave);

			} else {

				log_debug(logPlanificador, "Se pidio bloquear el ESI %d a una clave: %s no bloqueada. Se asigna la clave (BLOQUEO POR CONSOLA)", esi->id, recursoAuxiliar->clave);
				recursoAuxiliar -> estado = 1;
				list_add(esi->recursosAsignado, recursoAuxiliar->clave);


				if(string_equals_ignore_case(algoritmoDePlanificacion, SJF ) || string_equals_ignore_case(algoritmoDePlanificacion, SJFConDesalojo)){

					armarColaListos(esi);
					sem_post(&semContadorColaListos);
				} else if (string_equals_ignore_case(algoritmoDePlanificacion, HRRN ) || string_equals_ignore_case(algoritmoDePlanificacion, HRRNConDesalojo)){

					armarCola(esi);
					sem_post(&semContadorColaListos);
				}

			}
		}

		i++;

	}
	if(!encontrado){
		log_info(logPlanificador, "No se pudo bloquear el ESI de clave %d porque la clave del recurso ingresada no es valida", esi->id);
		printf("Reingrese la clave del recurso, porque no existe  \n");
	}

}



void listarBloqueados(char * clave){

	int i = 0;
	t_recurso * encontrado;
	bool e = false;

	while (list_size(listaRecursos) > i && !e){

		encontrado = list_get (listaRecursos, i);
		if(string_equals_ignore_case(encontrado->clave,clave)){
			e = true;
		}
		i++;

	}

	if (e == false){

		log_error(logPlanificador, "No se encontro el recurso de clave %s", clave);

		printf("No se encontro el recurso de clave %s  \n", clave);

	} else {

		int x = queue_size(encontrado->ESIEncolados);

		if(x == 0){

			log_debug(logPlanificador, "La clave no tiene recursos encolados.");

			printf("La clave no tiene ESI en cola  \n");

		}else{

			int i=0;
			t_list * listaAuxiliar = list_create();
			while ( !queue_is_empty(encontrado->ESIEncolados)){

				list_add(listaAuxiliar, queue_pop(encontrado->ESIEncolados));

			}

			while(i<list_size(listaAuxiliar)){

				ESI * aux = list_get(listaAuxiliar, i);
				log_debug(logPlanificador,"ESI en cola: %d ", aux->id);
				printf("ESI en cola: %d ", aux->id);
				queue_push(encontrado->ESIEncolados, aux);
				i++;
			}

			list_destroy(listaAuxiliar);
		}
	}


}

void seekAndDestroyESI(int clave){

	bool matar;


	pthread_mutex_lock(&mutexColaListos);

	matar = buscarYMatarEnCola(clave);

	pthread_mutex_unlock(&mutexColaListos);


	if(!matar){

		matar = buscarEnBloqueados(clave);

		if(!matar){

			log_error(logPlanificador, " No existe tal ESI ");
			printf("No existe tal ESI \n");

		} else {

			log_debug(logPlanificador, " ESI muerto desde cola de bloqueados ");

			printf("ESI muerto \n");
		}

	} else
		log_info(logPlanificador,"ESI encontrado en cola de Listos y terminado.");
		printf("ESI muerto \n");
		claveMatar = -1;

}


bool buscarYMatarEnCola(int clave){

	int t = 0;
	bool encontrado = false;
	t_list * colaAuxiliar = list_create();

	while(!queue_is_empty(colaListos)){

		list_add(colaAuxiliar,queue_pop(colaListos));

	}

	ESI * hola;

	while(list_size(colaAuxiliar)> t && !encontrado){

		hola = list_get(colaAuxiliar, t);

		if(hola -> id == clave){

			pthread_mutex_lock(&mutexComunicacion);
			log_info(logPlanificador, "Le aviso al ESI %d que va a ser abortado ", hola->id);

			uint32_t abortar = -2;
			send(hola->id, &abortar, sizeof(uint32_t), 0);
			pthread_mutex_unlock(&mutexComunicacion);
			liberarRecursos(hola);
			list_add(listaFinalizados,hola);
			sem_wait(&semContadorColaListos);
			list_remove(colaAuxiliar, t);
			log_debug (logPlanificador, "ESI ID : %d en finalizados", hola->id);
			encontrado = true;
		}

		t++;

	}


	while(!list_is_empty(colaAuxiliar)){

		queue_push(colaListos, list_remove(colaAuxiliar, 0));

	}

	list_destroy(colaAuxiliar);
	return encontrado;


}

bool encontrarVictima (ESI * esi){

		if(esi->id ==claveMatar){

			return true;

		} else{

			return false;

		}

}

void statusClave(char * clave){

	int i = 0;
	bool encontrado = false;

	log_info(logPlanificador, "Obteniendo status de una clave");

	while (i < list_size(listaRecursos) && !encontrado){

		t_recurso * recurso= list_get (listaRecursos, i);

		if (string_equals_ignore_case(recurso->clave, clave)){

			if(string_is_empty(recurso->valor)){
				log_error(logPlanificador, "La clave no tiene valor");
				printf("valor : NO TIENE \n");

			} else {
				log_debug(logPlanificador, "Encontrado valor : %s en la clave pedida ", recurso->valor);
				printf ("valor : %s \n", recurso->valor);
			}


			log_info(logPlanificador, "Pidiendo instancia de clave");

			uint32_t tam_clave = strlen(clave) + 1;
			send(socketClienteCoordinador, &tam_clave, sizeof(uint32_t), 0);
			send(socketClienteCoordinador, clave, tam_clave,0);

			uint32_t instanciaPosta_ID; // Si no esta asignada a ninguna devuelve <=0
			uint32_t instanciaSimulada_ID; // Siempre es un valor >0

			int resp = recv(socketClienteCoordinador,&instanciaPosta_ID, sizeof(uint32_t), 0);
			int resp2= recv(socketClienteCoordinador, &instanciaSimulada_ID, sizeof(uint32_t),0);


			if (resp <= 0 || resp2 <= 0){

				log_info(logPlanificador, " Fallo de conexion con coordinador");
				printf("El coordinador no pudo definir la instancia a ocupar \n");

			} else {
				if (instanciaPosta_ID != -1) {
					log_debug(logPlanificador, "La clave no esta asignada a ninguna Instancia");
					printf("La clave ingresada no se encuentra en ninguna instancia \n");
					if(instanciaSimulada_ID != -1){
						log_debug(logPlanificador, "Si la clave se asignara nuevamente se haria en la Instancia %d ", instanciaSimulada_ID);
						printf("Si la clave se asignara nuevamente se haria en la Instancia %d ", instanciaSimulada_ID);
					} else {
						printf("No se pudo obtener una instancia simulada");
						log_debug(logPlanificador, " No se pudo obtener una instancia simulada (-1)");
					}

				} else {
					log_debug(logPlanificador, "La clave esta asignada a la Instancia %d", instanciaPosta_ID);
					printf("La clave esta asignada a la Instancia %d \n", instanciaPosta_ID);
					if(instanciaSimulada_ID != -1){
						log_debug(logPlanificador, "Si la clave se asignara nuevamente se haria en la Instancia %d ", instanciaSimulada_ID);
						printf("Si la clave se asignara nuevamente se haria en la Instancia %d ", instanciaSimulada_ID);
					} else {
						printf("No se pudo obtener una instancia simulada");
						log_debug(logPlanificador, " No se pudo obtener una instancia simulada (-1)");
					}
				}

			}

			listarBloqueados(clave);
			encontrado = true;

		} else i++;

	}
	if(!encontrado){
		log_error(logPlanificador, "clave no hallada");
		printf("La clave no fue encontrada \n");
	}

}



// ----------------------------------- ACTUALIZACIONES ---------------------------------- //


void liberarRecursos(ESI * esi){

	int i=0;

	log_info(logPlanificador,"liberando recursos del ESI");

	while(i < list_size(esi->recursosAsignado)){

		char * recurso = list_get(esi->recursosAsignado, i);
		desbloquearRecurso( recurso );
		i++;
	}

	log_info(logPlanificador,"Recursos liberados");

	list_clean(esi->recursosAsignado);

}

void limpiarRecienLlegados(){

	log_info(logPlanificador, "Actualizando indices de la cola de ESI listos");
	t_queue * colaAuxiliar = queue_create();

	while(!queue_is_empty(colaListos)){

		ESI*nuevo = queue_pop(colaListos);
		nuevo -> recienLlegado = false;
		nuevo->recienDesbloqueadoPorRecurso = false;
		nuevo->recienDesalojado = false;

		queue_push(colaAuxiliar, nuevo);

	}

	log_info(logPlanificador,"Cola de listos al dia");

	queue_destroy(colaListos);
	colaListos = colaAuxiliar;
}



void liberarGlobales (){
	finalizarSocket(socketDeEscucha);
	finalizarSocket(socketCoordinador);
	finalizarSocket(socketClienteCoordinador);


	log_info(logPlanificador, "Liberando espacio");
	free(algoritmoDePlanificacion);
	free(puertoPropio);
	free(ipPropia);
	free(ipCoordinador);
	free(puertoCoordinador);


	int i = 0;
	while(clavesBloqueadas[i]!=NULL)
	{
		free(clavesBloqueadas[i]);
		i++;
	}
	free(clavesBloqueadas);

	if(list_size(listaFinalizados)>0){
		list_destroy_and_destroy_elements(listaFinalizados, (void*) ESI_destroy);
	} else {
		list_destroy(listaFinalizados);
	}

	if(queue_size(colaListos)>0){
		queue_destroy_and_destroy_elements(colaListos,(void *)ESI_destroy);
	} else {
		queue_destroy(colaListos);
	}


	if(list_size(listaRecursos)>0){

		list_destroy_and_destroy_elements(listaRecursos, (void *) recursoDestroy);
	} else {

		list_destroy(listaRecursos);
	}

	log_trace(logPlanificador," Memoria liberada. Cerrando log ");
	log_destroy(logPlanificador);
}

void signalHandler(int senal) {

	log_error(logPlanificador, "cerrando planificador");
	liberarGlobales();
	salir=true;
	sem_destroy(&semComodinColaListos);
	sem_destroy(&semContadorColaListos);
	sem_destroy(&semPausarPlanificacion);
	sem_destroy(&semSalir);
	pthread_join(hiloEscuchaConsola, NULL);
	pthread_join(hiloEscuchaESI, NULL);
	exit(-1);

}

