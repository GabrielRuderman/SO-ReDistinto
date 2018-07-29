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


// --------------------------- SEMAFOROS --------------------------------- //


pthread_mutex_t mutexColaListos = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexAsesino = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexComunicacion = PTHREAD_MUTEX_INITIALIZER;



// --------------------------- FUNCIONES --------------------------------- //



// --------------------------- DEADLOCK ---------------------------------- //



bool compararClaves (ESI * esi){

		log_info(logPlanificador, "entra a la comparacion de claves");

		if(esi->id ==claveActual){

			log_info(logPlanificador, "clave encontrada");

			return true;

		} else{

			return false;

		}

}


void comprobarDeadlock(){

	int contador = 0;
	t_list * dl = list_create();

	log_info (logPlanificador, "chequeando existencia de deadlock");

	while ( contador < list_size(listaRecursos) ){ // por cada recurso

		t_recurso * recursoAnalizar = list_get(listaRecursos, contador);

		log_info(logPlanificador,"analizando desde clave %s", recursoAnalizar->clave);

		int contador2 = 0;

		t_list * listaColaRecurso = list_create();

		while(!queue_is_empty(recursoAnalizar->ESIEncolados)){

			list_add(listaColaRecurso, queue_pop(recursoAnalizar->ESIEncolados));

		}

		while (contador2 < list_size(listaColaRecurso)){ // tomo su primer ESI bloqueado

			ESI * Aux = list_get(listaColaRecurso, contador2);

			log_info(logPlanificador, "tomo ESI clave: %d ", Aux->id);

			int contador3 = 0;

			while(list_size(Aux -> recursosAsignado) > contador3){ // y chequeo si para cada recurso asignado del mismo, hay un ESI en la cola de bloqueados del mismo que tenga asignada la clave que lo está bloqueando

				char * recursoAsignado = list_get(Aux->recursosAsignado,contador3);

				log_info(logPlanificador, "comparo contra clave: %d", recursoAsignado);

				chequearDependenciaDeClave(recursoAnalizar->clave, recursoAsignado, Aux->id, dl );

				if(list_size(dl) > 0){ // en este caso se encontro el DL

					int cont = 0;

					log_info(logPlanificador, "DL encontrado asociado a la clave original, con ESI:");

					printf("DEADLOCK formado por los siguientes ESI: \n");

					while(list_size(dl)> cont){

						log_info(logPlanificador, "clave: %d", list_get(dl,cont));

						printf("ESI %d \n", (int) list_get(dl,cont));

						cont ++;

						list_clean(dl); // limpio para encontrar otros DL
					}

				}

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

	log_info(logPlanificador, "empiezo a chequear ciclos");

	t_recurso * recurso = traerRecurso(recursoESI); // busco el recurso que iguale la clave del ESI

	if(recurso == NULL){ // Si no lo encuentra, no mando nada, no deberia pasar, logueo para estar al tanto

		log_info(logPlanificador,"CASO RARO: no se encontro el recurso que tiene asignado un ESI");

	} else { // al encontrar el recurso, tengo que chequear por cada ESI bloqueado de la clave, si su asignado coincide con la clave del recursoOriginal

		int i = 0;

		t_list * listaColaESI = list_create();

		while(!queue_is_empty(recurso->ESIEncolados)){
			list_add(listaColaESI, queue_pop(recurso->ESIEncolados));

		}

		bool DLEncontrado = false;

		while ( list_size(listaColaESI)> i && !DLEncontrado){ // por cada ESI encolado

			log_info(logPlanificador, "Cola ESI tiene elementos");

			ESI * esi = list_get(listaColaESI, i); // tomo de a uno

			int t = 0;

			while (list_size(esi->recursosAsignado) > t && !DLEncontrado){

				log_info(logPlanificador, " ESI %d tiene recursos asignados", esi->id);

				char * recurso = list_get (esi->recursosAsignado,t); // saco un recurso asignado

				if(string_equals_ignore_case(recurso,recursoOriginal)){ // es igual al recurso original?

					list_add(listaDL,&idESI);
					list_add(listaDL, &esi->id);
					log_info(logPlanificador, "encontrado DL");
					DLEncontrado = true;

				} else { // si no son iguales, va a buscar coincidencia en los recursos que ese esi tiene asignados

					log_info(logPlanificador, "No genera DL, chequeo contra claves anidadas");

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

		int cont = 0;

		while(list_size(listaColaESI)> cont){

			queue_push(recurso->ESIEncolados, list_get(listaColaESI, cont));
			cont ++;

		}

		list_destroy(listaColaESI);

	}

}


// ----------------------------------- UTILIDADES Y PLANIFICACION --------------------------------- //

ESI* estimarProximaRafaga(ESI * proceso ){


	log_info (logPlanificador, "EL ESI DE CLAVE %d TIENE RAFAGA ANTERIOR DE %d", proceso->id, proceso->rafagaAnterior);
	if(proceso -> rafagaAnterior == 0){

		log_info(logPlanificador, "ESTIMACION = 0");

		proceso -> estimacionSiguiente = 0;

	} else{

		proceso->estimacionSiguiente = ((alfa*proceso->estimacionAnterior)+(1-alfa)*proceso->rafagaAnterior);

	}
	log_info(logPlanificador,"un tiempo estimado");
	return proceso;

}



t_recurso * traerRecurso (char * clave){


	t_recurso * recurso;
	bool encontrado = false;
	int i = 0;
	log_info(logPlanificador, "buscando recurso de clave %s", clave);


	while(list_size(listaRecursos) > i && !encontrado){

		log_info(logPlanificador, "recorriendo lista recursos");

		recurso = list_get(listaRecursos, i);

		if(recurso->clave == clave){

			log_info(logPlanificador, "clave encontrada");

			encontrado = true;
		}
		i++;
	}

	if(encontrado == false){

		log_info(logPlanificador, "recurso no encontrado");

		recurso = NULL;
	}

	return recurso;
}

bool recursoEnLista(ESI * esi){

	log_info(logPlanificador,"chequeo si el recurso está en su lista de asignados");
	bool retorno = false;
	int i = 0;
	while( i< list_size(esi->recursosAsignado) && !retorno){
		char * r = list_get(esi->recursosAsignado, i);

		if(string_equals_ignore_case(r, esi->recursoPedido)){
			retorno = true;
			log_info(logPlanificador,"Está");
		}
		i++;
	}

	return retorno;
}


ESI * buscarESI(int clave){

	log_info(logPlanificador, " buscando ESI en cola listos");

	ESI * esiEncontrado;
	int t = 0;
	bool encontrado = false;
	t_queue * colaAuxiliar = queue_create();

	while(queue_size(colaListos) > t && !encontrado){

		esiEncontrado = queue_pop(colaListos);
		log_info(logPlanificador, "Comparando %d con clave: %d", clave, esiEncontrado->id);

		if(esiEncontrado -> id == clave){
			encontrado = true;
		}

		t++;

	}
	queue_destroy(colaListos);

	colaListos = colaAuxiliar;

	return esiEncontrado;


}




extern void cargarValor(char* clave, char* valor){

	int i = 0;
	bool encontrado = false;
	log_info(logPlanificador, "cargando valor a clave");
	while(i< list_size(listaRecursos) && !encontrado){

		t_recurso * auxiliar = list_get(listaRecursos, i);

		if(string_equals_ignore_case(auxiliar->clave, clave)){
			free(auxiliar->valor);
			auxiliar->valor = string_new();
			log_info(logPlanificador, "clave encontrada");
			string_append(&(auxiliar->valor),valor);
			log_info(logPlanificador, "valor nuevo: %s",auxiliar->valor);
			encontrado = true;
		}

		i++;


	}

	if(!encontrado){

		log_info(logPlanificador,"no encontro la clave");
	}
}

bool buscarEnBloqueados (int clave){

	int i = 0;
	bool encontrado = false;

	log_info(logPlanificador, "arranca busqueda en bloqueados");

	while (list_size( listaRecursos) > i && !encontrado){

		t_recurso * recu = list_get(listaRecursos, i);

		log_info(logPlanificador, "toma recurso clave %s", recu->clave);


		int r = 0;

		t_queue * cola = recu->ESIEncolados; // la cola esta apuntando a los bloqueados

		while(queue_size(recu->ESIEncolados)> r && !encontrado){

			ESI * aux = queue_pop(cola); // avanza entre bloqueados

			log_info(logPlanificador, "saca esi clave %d", aux->id);

			if(aux->id == clave){ // si encuentra, libera recursos y destruye al ESI

				log_info(logPlanificador, "se encontro el esi");
				encontrado = true;
				liberarRecursos(aux);
				ESI_destroy(aux);
				claveMatar=-1;
			}

			r++;

		} // pero al encontrarlo, la cola original queda con un hueco que generara fallas mas adelante

		if(encontrado){ // entonces si fue encontrado

			int t = 0;
			log_info(logPlanificador, "rearmando cola de bloqueados de la clave");

			t_queue * colaNueva = queue_create(); //creo una cola nueva

			while(queue_size(recu->ESIEncolados)>t ){

				ESI * esiComprobar = queue_pop(recu->ESIEncolados); // voy sacando de a uno de la original

				if(esiComprobar != NULL){ //si es diferente de NULL

					queue_push(colaNueva,esiComprobar); // lo meto en la cola nueva
				} // si no, no hago nada

			}

			queue_destroy(recu->ESIEncolados);
			recu->ESIEncolados = colaNueva; // ahora la original apunta a la nueva, que no tiene el hueco.
			log_info(logPlanificador, "rearmada");
		} // cuanto mas facil hubiese sido con una lista..-
		i++;

	}


	return encontrado;
}



// ----------------------------------- CONSOLA ----------------------------------- //


void lanzarConsola(){

	char* linea;

	while(1){

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
			log_info(logPlanificador, "Comando ingresado por consola : Pausear planificacion", linea);
			pausearPlanificacion = true;
			free(linea);
		}
		else if (string_equals_ignore_case(linea,REANUDAR_PLANIFICACION)  || string_equals_ignore_case(linea, "2"))
		{
			log_info(logPlanificador, "Comando ingresado por consola : Reanudar planificacion", linea);
			pausearPlanificacion= false;
			free(linea);
		}
		else if (string_equals_ignore_case(linea, BLOQUEAR_ESI)  || string_equals_ignore_case(linea, "3"))
		{
			log_info(logPlanificador, "Comando ingresado por consola : bloquear ESI ", linea);
			linea = readline("ID ESI:");
			log_info(logPlanificador, "clave ESI ingresada por consola : %s", linea);

			int clave = atoi(linea);

			log_info(logPlanificador, "clave ESI convertida a int: %d ", clave);

			free(linea);

			linea = readline("CLAVE RECURSO:");
			log_info(logPlanificador, "Clave recurso ingresada por consola : %s", linea);


			if( claveActual==clave){

				log_info(logPlanificador, "la clave coincide con la del ESI acualmente ejecutandose");
				claveParaBloquearESI = clave;
				string_append(&claveParaBloquearRecurso, linea);
				log_info(logPlanificador, "esperando que se bloquee el ESI actual (debe terminar una instruccion)");

				while(1){

					if(bloquearESIActual){
						break;
					}

				}

				log_info(logPlanificador, "bloqueado");
				printf("bloqueado \n");
				bloquearESIActual = false;

			} else {

				log_info(logPlanificador, "el esi no es el de ejecucion actual, buscamos en cola");

				ESI * nuevoESI = buscarESI(clave);

				if(nuevoESI == NULL){

					log_info(logPlanificador, "la clave ingresada no existe en los ESI listos o en ejecucion");

					printf("se introdujo una id erronea o la misma ya se encuentra bloqueada : %d \n", clave);

				} else {

					int i = 0;
					bool encontrado = false;
					log_info(logPlanificador, " chequeando que el ESI no tenga asignada esa clave ");
					while(list_size(nuevoESI-> recursosAsignado ) > i && !encontrado){

						char * recurso = list_get (nuevoESI->recursosAsignado , i);
						if(string_equals_ignore_case(linea, recurso)){

							log_info(logPlanificador, " la clave ya esta asignada al ESI, no se puede bloquear");
							printf(" la clave ya esta asignada al ESI, no se puede bloquear \n");
							encontrado = true;

						}

						i++;

					}

					if(!encontrado){

						log_info(logPlanificador, " no tiene asignada la clave ");
						log_info(logPlanificador, "mandando a bloquear esi");
						nuevoESI->bloqueadoPorConsola = true;
						bloquearESI(linea, nuevoESI);
					}

				}
			}
			free(linea);

		}
		else if (string_equals_ignore_case(linea,DESBLOQUEAR_CLAVE)  || string_equals_ignore_case(linea, "4"))
		{
			log_info(logPlanificador, "Comando ingresado por consola : DESBLOQUEAR_ESI");

			linea = readline("CLAVE RECURSO:");
			log_info(logPlanificador, "Clave recurso ingresada por consola : %s", linea);

			desbloquearRecurso(linea);

			free(linea);


		}
		else if (string_equals_ignore_case(linea, LISTAR_POR_RECURSO)  || string_equals_ignore_case(linea, "5")){

			log_info(logPlanificador, "comando ingresado por consola : LISTAR_POR_RECURSO", linea);
			linea = readline("RECURSO:");
			log_info(logPlanificador, "Clave recurso ingresada por consola : %s", linea);

			listarBloqueados(linea);
			free(linea);
		}
		else if (string_equals_ignore_case(linea, KILL_ESI)  || string_equals_ignore_case(linea, "6"))
		{
			log_info(logPlanificador, "comando ingresado por consola : KILL_ESI", linea);
			linea = readline("CLAVE ESI:");
			log_info(logPlanificador, "Clave esi ingresada por consola : %s", linea);

			int clave = atoi(linea);

			log_info(logPlanificador, "Clave esi convertida : %d", clave);

			if(clave == claveActual){

				log_info(logPlanificador, "la clave del esi ejecutandose es igual a la que se quiere matar");
				pthread_mutex_lock(&mutexAsesino);

				matarESI = true;

				pthread_mutex_unlock(&mutexAsesino);

				printf("esperando a que el ESI pueda ser matado \n");

			}else {

				log_info(logPlanificador, "la clave a matar no es igual a la que se esta ejecutando. Buscando... ");
				claveMatar = clave;
				seekAndDestroyESI(clave);
			}
			free(linea);

		}else if (string_equals_ignore_case(linea, STATUS_CLAVE)  || string_equals_ignore_case(linea, "7"))
		{
			log_info(logPlanificador, "Comando ingresado por consola : STATUS_CLAVE", linea);
			linea = readline("CLAVE:");
			log_info(logPlanificador, "Clave esi ingresada por consola : %s", linea);
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

				printf("el ESI numero %d es de clave %d \n", i, esi->id);
				i++;
			}

			free(linea);

		}
		else if (string_equals_ignore_case(linea, "salir")  || string_equals_ignore_case(linea, "10"))
		{
			printf("cerrando planificador");
			log_info(logPlanificador, "Comando ingresado por consola  : salir");
			liberarGlobales();
			free(linea);
			exit(EXIT_FAILURE);
		}
		else
		{
			log_info(logPlanificador, "Clave recurso ingresada por consola : %s no reconocido", linea);
			printf("Comando no reconocido");
			free(linea);
		}

	}
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

	log_info(logPlanificador, "inicio hilo de escucha de ESIS");

	while(1){

		uint32_t socketESINuevo = escucharCliente(logPlanificador,socketDeEscucha);

		log_info(logPlanificador, "se escucho un nuevo ESI");

		send(socketESINuevo,&socketESINuevo,sizeof(uint32_t),0);
		ESI * nuevoESI = crearESI(socketESINuevo);

		pthread_mutex_lock(&mutexColaListos); //pongo mutex para que no se actualice la cola mientras agrego un ESI.
		if(string_equals_ignore_case(algoritmoDePlanificacion,SJF) || string_equals_ignore_case(algoritmoDePlanificacion,SJFConDesalojo)){
			armarColaListos(nuevoESI);
		} else if(string_equals_ignore_case(algoritmoDePlanificacion, HRRN) || string_equals_ignore_case(algoritmoDePlanificacion,HRRNConDesalojo)){
			armarCola(nuevoESI);
		}
		pthread_mutex_unlock(&mutexColaListos);

	}
}



bool validarPedido (char * recurso, ESI * ESIValidar){

	int i = 0;
	bool encontrado = false;
	bool retorno = false;

	log_info(logPlanificador,"verificando si el proceso tiene permiso de ejecutar");

	while(list_size(listaRecursos) > i){

			t_recurso * nuevo = list_get(listaRecursos, i);

			if(string_equals_ignore_case(recurso,nuevo->clave)){

				encontrado = true;

				log_info(logPlanificador,"el recurso pedido existe");

				if(nuevo->estado == 1 && ESIValidar->proximaOperacion == 1){ // caso recurso tomado para get

					log_info(logPlanificador,"Se quiere hacer un GET de un recurso tomado. Se le niega permiso al ESI");
					nuevo = NULL;
					retorno = false;

				} else if (nuevo-> estado == 1 && ESIValidar-> proximaOperacion > 1){ //caso recurso tomado para set y store

					log_info(logPlanificador,"se quiere hacer un SET o STORE. Se chequea si la clave esta asignada");
					bool RecursoEncontrado = false;
					int t = 0;
					while(list_size(ESIValidar->recursosAsignado) > t){

						char * recursoAuxiliar = list_get (ESIValidar->recursosAsignado, t);

						if( string_equals_ignore_case(recursoAuxiliar,nuevo->clave)){ // caso ESI tiene asignado recurso

							log_info(logPlanificador,"clave asignada. Tiene permiso");
							RecursoEncontrado = true;
							nuevo->operacion = ESIValidar->proximaOperacion; //mete si hace set o get
							retorno = true;
						}

						recursoAuxiliar= NULL;

						t++;

					} if(RecursoEncontrado == false){ // Caso ESI no tiene asignado recurso

						log_info(logPlanificador," No se puede hacer un SET o STORE de un recurso no asignado!");
						nuevo = NULL;
						retorno = false;

					}


				} else if(nuevo->estado==0 && ESIValidar->proximaOperacion > 1){ // caso el recurso no esta tomado y quiere hacer SET o STORE

					log_info(logPlanificador,"SET o STORE de recurso no tomado, no se permite");
					retorno = false;
				} else {

					log_info(logPlanificador,"GET de un recurso no tomado. Se permite");
					retorno = true;

				}

			}

			i++;

		}

		if ( encontrado == false){ // caso recurso nuevo

			if (ESIValidar -> proximaOperacion == 1){

				log_info(logPlanificador,"Se crea recurso nuevo porque no fue encontrada la clave pedida. Tiene permiso");
				t_recurso * nuevoRecurso = crearRecurso(recurso);
				nuevoRecurso -> operacion =  0;
				list_add(listaRecursos,nuevoRecurso);
				retorno = true;
			} else if (ESIValidar-> proximaOperacion > 1){
				log_info (logPlanificador, " se pide set o store de una clave no existente, abortando esi ");
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
				log_info(logPlanificador, " se intento bloquear un recurso ya bloqueado. Se ignora");
			}

		}
		i++;
	}

}



void desbloquearRecurso (char* claveRecurso) {

	log_info(logPlanificador, "se intenta desbloquea un recurso clave %s", claveRecurso);
	int i = 0;
	bool encontrado = false;
	while(list_size(listaRecursos) > i && !encontrado)
	{
		t_recurso * nuevoRecurso = list_get(listaRecursos,i);
		if(string_equals_ignore_case(nuevoRecurso->clave,claveRecurso))
		{
			log_info(logPlanificador, "recurso encontrado");
			if(nuevoRecurso->estado == 1){ // libera el recurso y saca de la cola a SOLO UN ESI que lo estaba esperando
				log_info(logPlanificador, "que esta bloqueado");
				log_info(logPlanificador, "clave recurso : %s", nuevoRecurso->clave);
				encontrado = true;
				ESI * nuevo = queue_pop(nuevoRecurso->ESIEncolados);
				if(nuevo != NULL){
					log_info(logPlanificador, "desencolo el primer esi bloqueado");
					nuevo->recienDesbloqueadoPorRecurso = true;
					list_add(nuevo->recursosAsignado, nuevoRecurso->clave);
					pthread_mutex_lock(&mutexColaListos);
					if(string_equals_ignore_case(algoritmoDePlanificacion, SJF) || string_equals_ignore_case(algoritmoDePlanificacion, SJFConDesalojo)){
						log_info(logPlanificador, " a cola SJF ");
						armarColaListos(nuevo);
					}else if (string_equals_ignore_case(algoritmoDePlanificacion, HRRNConDesalojo) || string_equals_ignore_case(algoritmoDePlanificacion, HRRN)){
						log_info(logPlanificador, " a cola HRRN ");
						armarCola(nuevo);
					}
					pthread_mutex_unlock(&mutexColaListos);

				} else {
					log_info(logPlanificador, " la clave no tenia ESIS encolados");
					nuevoRecurso->estado = 0;
				}

				log_info(logPlanificador, "Recurso de clave %s desbloqueado", nuevoRecurso->clave);


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
	while(i< list_size(listaRecursos) && !encontrado){

		t_recurso * recursoAuxiliar = list_get(listaRecursos, i);
		if(string_equals_ignore_case(recursoAuxiliar->clave,claveRecurso)){

			if(recursoAuxiliar->estado == 1){
			log_info(logPlanificador, "el recurso esta tomado y el ESI entra en su cola");
			esi->bloqueadoPorClave = true;
			queue_push(recursoAuxiliar->ESIEncolados,esi);
			encontrado = true;
			log_info(logPlanificador, "esi de clave %d en cola de recurso clave : %s", esi->id, recursoAuxiliar->clave);

			} else {

				log_info(logPlanificador, "se pidio bloquear un ESI a una clave no bloqueada. Se asigna la clave (BLOQUEO POR CONSOLA)");
				recursoAuxiliar -> estado = 1;
				list_add(esi->recursosAsignado, recursoAuxiliar->clave);
				log_info(logPlanificador, " Clave asignada y bloqueada ");

				if(string_equals_ignore_case(algoritmoDePlanificacion, SJF ) || string_equals_ignore_case(algoritmoDePlanificacion, SJFConDesalojo)){

					log_info(logPlanificador, "metiendolo en cola listos");
					armarColaListos(esi);
				} else if (string_equals_ignore_case(algoritmoDePlanificacion, HRRN ) || string_equals_ignore_case(algoritmoDePlanificacion, HRRNConDesalojo)){

					log_info(logPlanificador, "metiendolo en cola listos");
					armarCola(esi);
				}

			}
		}

		i++;

	}
	if(!encontrado){
		log_info(logPlanificador, "no se pudo bloquear el ESI de clave %d porque la clave del recurso ingresada no es valida", esi->id);
		printf("reingrese la clave del recurso, porque no existe  \n");
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

		log_info(logPlanificador, "no se encontro el recurso de clave %s", clave);

		printf("no se encontro el recurso de clave %s  \n", clave);

	} else {

		int x = queue_size(encontrado->ESIEncolados);

		if(x == 0){

			log_info(logPlanificador, "la clave no tiene recursos encolados.");

			printf("la clave no tiene ESI en cola  \n");

		}else{

			int i=0;
			t_list * listaAuxiliar = list_create();
			log_info(logPlanificador, " lleno lista auxiliar ");

			while ( !queue_is_empty(encontrado->ESIEncolados)){

				list_add(listaAuxiliar, queue_pop(encontrado->ESIEncolados));

			}

			log_info(logPlanificador, "mostrando por pantalla los esi encolados y vuelvo a encolar ESIS");

			while(i<list_size(listaAuxiliar)){

				ESI * aux = list_get(listaAuxiliar, i);
				printf("ESI en cola: %d ", aux->id);
				queue_push(encontrado->ESIEncolados, aux);
				i++;
			}

			list_destroy(listaAuxiliar);
		}
	}


}

void seekAndDestroyESI(int clave){

	log_info(logPlanificador,"Buscando a ESI de clave %d", clave);

	log_info(logPlanificador, "Se procede a buscar en cola");

	pthread_mutex_lock(&mutexColaListos);

	bool matar = buscarYMatarEnCola(clave);

	pthread_mutex_unlock(&mutexColaListos);

	if(!matar){

		matar = buscarEnBloqueados(clave);

		if(!matar){

			log_info(logPlanificador, " no existe tal ESI ");
			printf("No existe tal ESI \n");

		} else {

			log_info(logPlanificador, " ESI muerto desde cola de bloqueados ");

			printf("ESI muerto \n");
		}

	} else
		log_info(logPlanificador,"ESI encontrado en cola de Listos y terminado.");
		claveMatar = -1;

}


bool buscarYMatarEnCola(int clave){

	int t = 0;
	bool encontrado = false;
	t_queue * colaAuxiliar = queue_create();
	ESI * hola;

	while(queue_size(colaListos)> t && !encontrado){

		hola = queue_pop(colaListos);
		log_info(logPlanificador, "Comparando %d con clave: %d", clave, hola->id);

		if(hola -> id == clave){
			log_info(logPlanificador, "encontrado");
			log_info(logPlanificador, " liberando recursos ");
			liberarRecursos(hola);
			list_add(listaFinalizados,hola);
			log_info (logPlanificador, "esi ID : %d en finalizados", hola->id);
			encontrado = true;
		} else queue_push(colaAuxiliar,hola);

		t++;

	}
	queue_destroy(colaListos);

	colaListos = colaAuxiliar;

	return encontrado;


}

bool encontrarVictima (ESI * esi){

		log_info(logPlanificador, "entra a la comparacion de claves para matar");

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

			log_info(logPlanificador, "clave encontrada");
			if(string_is_empty(recurso->valor)){
				log_info(logPlanificador, "sin valor");
				printf("valor : NO TIENE \n");

			} else {
				log_info(logPlanificador, "valor : %s ", recurso->valor);
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

				log_info(logPlanificador, " fallo conexion");
				printf("el coordinador no pudo definir la instancia a ocupar \n");

			} else {
				if (instanciaPosta_ID <= 0) {
					log_info(logPlanificador, "La clave no esta asignada a ninguna Instancia");
					printf("la clave ingresada no se encuentra en ninguna instancia \n");
				} else {
					log_info(logPlanificador, "La clave esta asignada a la Instancia %d", instanciaPosta_ID);
					printf("La clave esta asignada a la Instancia %d \n", instanciaPosta_ID);
				}
				log_info(logPlanificador, "Si la clave se asignara nuevamente se haria en la Instancia %d ", instanciaSimulada_ID);
				printf("Si la clave se asignara nuevamente se haria en la Instancia %d ", instanciaSimulada_ID);
			}

			listarBloqueados(clave);
			encontrado = true;

		} else i++;

	}
	if(!encontrado){
		log_info(logPlanificador, "clave no hallada");
		printf("la clave no fue encontrada \n");
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

	log_info(logPlanificador, "actualizando cola listos -recien llegados-");
	t_queue * colaAuxiliar = queue_create();
	log_info(logPlanificador, "creada cola auxiliar");

	while(queue_is_empty(colaListos)){

		ESI*nuevo = queue_pop(colaListos);

		log_info(logPlanificador, "Actualizando un ESI...");

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

	log_info(logPlanificador, "liberando espacio");
	log_info(logPlanificador,"liberando algoPlanif");
	free(algoritmoDePlanificacion);
	log_info(logPlanificador,"liberando ipPropia");
	free(puertoPropio);
	log_info(logPlanificador,"liberando PuertoPropio");
	free(ipPropia);
	log_info(logPlanificador,"liberando ipCoordinador");
	free(ipCoordinador);


	int i = 0;
	while(clavesBloqueadas[i]!=NULL)
	{
		log_info(logPlanificador,"liberando clave: %s", clavesBloqueadas[i]);
		free(clavesBloqueadas[i]);
		i++;
	}

	log_info(logPlanificador,"liberando finalizados");

	if(list_size(listaFinalizados)>0){
		log_info(logPlanificador, "lista con elementos");
		list_destroy_and_destroy_elements(listaFinalizados, (void*) ESI_destroy);
	} else {
		log_info(logPlanificador, "lista vacia");
		list_destroy(listaFinalizados);
	}

	log_info(logPlanificador, "liberando colaListos");
	if(queue_size(colaListos)>0){
		log_info(logPlanificador, "cola con elementos");
		queue_destroy_and_destroy_elements(colaListos,(void *)ESI_destroy);
	} else {
		log_info(logPlanificador, "cola vacia");
		queue_destroy(colaListos);
	}


	log_info(logPlanificador,"liberando lista recursos ");
	if(list_size(listaRecursos)>0){

		log_info(logPlanificador,"lista con elementos");
		list_destroy_and_destroy_elements(listaRecursos, (void *) recursoDestroy);
	} else {

		log_info(logPlanificador, "lista vacia");
		list_destroy(listaRecursos);
	}

	log_info(logPlanificador,"cerrando log ");
	log_destroy(logPlanificador);
}

