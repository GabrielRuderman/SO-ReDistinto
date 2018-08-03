#include "planificador.h"

bool planificacionHRRNTerminada = false;

void
planificacionHRRN (bool desalojo)
{

  pthread_create (&hiloEscuchaConsola, NULL, (void *) lanzarConsola, NULL);

  pthread_create(&hiloEscuchaESI, NULL, (void *) escucharNuevosESIS, NULL);

  log_info (logPlanificador, "Arraca HRRN");

  while (1)
    {

	bool finalizar = false;

	bool bloquear = false;

	bool permiso = true;

	bool desalojar = false;

	uint32_t operacion;
	uint32_t tamanioRecurso;
	char * recursoPedido;

	ESI* nuevoESI;

	pthread_mutex_lock(&mutexPauseo);

	if(pausearPlanificacion){

		sem_wait(&semPausarPlanificacion);
	}

	pthread_mutex_unlock(&mutexPauseo);

	sem_wait(&semComodinColaListos);
	sem_wait(&semContadorColaListos);

	pthread_mutex_lock(&mutexColaListos);

	nuevoESI = queue_pop (colaListos);
	claveActual = nuevoESI -> id;

	pthread_mutex_unlock(&mutexColaListos);
	sem_post(&semComodinColaListos);


	log_trace (logPlanificador, "ID actual en planificacion es : %d", nuevoESI->id);

	nuevoESI->tiempoEspera = 0; // se indico que si el ESI entra, aunque no tenga permiso, reiniciara su tiempo de espera


  while (!finalizar && !bloquear && permiso && !desalojar && !matarESI)
{

	pthread_mutex_lock(&mutexPauseo);

	if(pausearPlanificacion){

		sem_wait(&semPausarPlanificacion);

	}
	pthread_mutex_unlock(&mutexPauseo);

  pthread_mutex_lock(&mutexComunicacion);

  if(nuevoESI->bloqueadoPorClave && !nuevoESI->bloqueadoPorConsola){

	  log_info(logPlanificador, " El ESI fue recien desbloqueado de la clave ");
	  permiso = true;
	  pthread_mutex_unlock(&mutexComunicacion);


  } else {

	  log_info(logPlanificador, "Empieza comunicacion con ESI y Coordinador");
	  uint32_t continuacion;
	  send(nuevoESI->id, &CONTINUAR, sizeof(uint32_t), 0);
	  int respuesta0 =recv(nuevoESI->id, &continuacion,sizeof(uint32_t),0);

	  if(respuesta0 < 0){

		  log_error(logPlanificador, "Conexion con ESI rota");
		  finalizar = true;
		  pthread_mutex_unlock(&mutexComunicacion);
		  sem_post(&semPausarPlanificacion);
		  break;
	  }
	  if(continuacion == 0){
		  log_error(logPlanificador, " El ESI de ID : %d se aborto por un motivo desconocido", nuevoESI->id);
		  liberarRecursos(nuevoESI);
		  list_add(listaFinalizados, nuevoESI);
		  pthread_mutex_unlock(&mutexComunicacion);
		  sem_post(&semPausarPlanificacion);

		  break;
	  }

	  int respuesta1 = recv(socketCoordinador, &operacion, sizeof(operacion), 0);
	  int respuesta2 = recv(socketCoordinador, &tamanioRecurso, sizeof(uint32_t), 0);
	  recursoPedido = malloc(sizeof(char)*tamanioRecurso);
	  int respuesta3 = recv(socketCoordinador, recursoPedido, sizeof(char)*tamanioRecurso,0);

	  if(respuesta1 <= 0 || respuesta2 <= 0 || respuesta3 <= 0){
		  log_info(logPlanificador, "Conexion con el coordinador rota. Me cierro");
		  liberarGlobales();
		  exit(-1);
	  } else {
		  log_info(logPlanificador, "Se carga recurso pedido : %s", recursoPedido);
		  nuevoESI->recursoPedido = string_new();
		  string_append(&(nuevoESI->recursoPedido), recursoPedido);
		  nuevoESI->proximaOperacion = operacion;
	  }

	  permiso = validarPedido(nuevoESI->recursoPedido,nuevoESI);
  }

  if(permiso){

	  nuevoESI->rafagaAnterior = 0;

	  log_info(logPlanificador, "ESI tiene permiso de ejecucion");

	  if(nuevoESI->proximaOperacion == 2){

		  char * valorRecurso;
		  uint32_t tamValor;

		  int resp = recv (socketCoordinador, &tamValor,sizeof(uint32_t),0);
		  valorRecurso=malloc(sizeof(char)*tamValor);
		  int resp2 =recv (socketCoordinador,valorRecurso,sizeof(char)*tamValor,0);

		  if(resp <= 0 || resp2 <= 0){

			  log_info(logPlanificador, "Conexion con el coordinador rota");
			  liberarGlobales();
			  exit(-1);

		  } else {
			  cargarValor(nuevoESI->recursoPedido,valorRecurso);
		  }


	  }
	  pthread_mutex_unlock(&mutexComunicacion);

	  if(nuevoESI->proximaOperacion == 1 && !recursoEnLista(nuevoESI)){

		  log_debug(logPlanificador, "El recurso %s se agrega a los asignados del ESI", nuevoESI->recursoPedido);
		  list_add(nuevoESI->recursosAsignado, nuevoESI->recursoPedido);

	  }

	  bloquearRecurso(recursoPedido);

	  nuevoESI->recienLlegado = false;
	  nuevoESI->recienDesbloqueadoPorRecurso = false;

	  pthread_mutex_lock(&mutexComunicacion);

		if (nuevoESI->bloqueadoPorClave && !nuevoESI->bloqueadoPorConsola) {
			send(nuevoESI->id, &CONTINUAR, sizeof(uint32_t), 0);
			nuevoESI->bloqueadoPorClave = false;
			log_info(logPlanificador,
					"ESI estaba bloqueado y se entera que puede continuar");

		} else {
			nuevoESI->bloqueadoPorClave = false;
			nuevoESI->bloqueadoPorConsola = false;
			log_info(logPlanificador,
					" Coordinador se entera que el ESI tiene permiso");
			send(socketCoordinador, &CONTINUAR, sizeof(uint32_t), 0);
		}


	  nuevoESI -> rafagasRealizadas = nuevoESI -> rafagasRealizadas +1;

	  pthread_mutex_lock(&mutexColaListos);

	  aumentarEspera();

	  pthread_mutex_unlock(&mutexColaListos);

	  log_trace(logPlanificador, "Instrucciones realizadas del ESI %d son %d en este ciclo", nuevoESI-> id, nuevoESI->rafagasRealizadas);

	  uint32_t respuesta ;
	  int conexion = recv(nuevoESI->id, &respuesta, sizeof(uint32_t),0);

	  pthread_mutex_unlock(&mutexComunicacion);

	  if(conexion <= 0){
		  log_info(logPlanificador, "Se rompio la conexion con el ESI. Se lo finaliza");
		  finalizar = true;
		  sem_post(&semPausarPlanificacion);
		  break;
	  }

	  if(nuevoESI->proximaOperacion == 3) { //si realizo un STORE, se libera el recurso que estaba usando

		  liberarUnRecurso(nuevoESI);

	  }
	  pthread_mutex_lock(&mutexColaListos); // por las dudas que entre otro ESI justo en este momento (no lo tengo en cuenta)

	  if (respuesta != CONTINUAR)
	  {
		  log_info(logPlanificador, "El ESI me informa que va a finalizar");
		  finalizar = true;

	  } else if(desalojo && queue_size(colaListos) > 0) // si hay desalojo activo
	  {

		  ESI* auxiliar = queue_peek(colaListos);

		  if(auxiliar->recienLlegado || auxiliar->recienDesbloqueadoPorRecurso)
		  {
			  if(auxiliar->tiempoRespuesta < (nuevoESI->tiempoRespuesta - nuevoESI->rafagasRealizadas))
			  {
				  log_trace(logPlanificador, "Se genera desalojo del ESI %d de tiempo de respuesta: %.6f con ESI ID %d de tiempo de respuesta: %.6f", nuevoESI->id, nuevoESI->tiempoRespuesta, auxiliar->id, auxiliar->tiempoRespuesta);

				  desalojar = true; //se va a desalojar
			  } else
			  {
				  log_debug(logPlanificador, "No hay necesidad de desalojar, se actualiza cola");

				  limpiarRecienLlegados(); // Si no es menor la estimacion, los recien llegados ya no tendrian validez, los actualizo
			  }
		  }
	  }
	  pthread_mutex_unlock(&mutexColaListos);


	  if (nuevoESI->id == claveParaBloquearESI)
	  {
		  log_trace(logPlanificador, "Se pidio bloqueo del ESI mediante la consola");

		  bloquear = true;
	  }



  } else {

	  if(nuevoESI-> proximaOperacion == 1){

		  log_error(logPlanificador, "El ESI no tiene permiso de ejecucion, se lo bloquea");

		  claveActual = -1;
		  int rafagas = nuevoESI->rafagasRealizadas;
		  int estimacion = nuevoESI->estimacionSiguiente;
		  nuevoESI->estimacionAnterior = estimacion;
		  nuevoESI->rafagaAnterior = rafagas;
		  nuevoESI->rafagasRealizadas = 0;
		  bloquearESI(nuevoESI->recursoPedido, nuevoESI);
		  uint32_t aviso = 0;
		  send(socketCoordinador, &aviso,  sizeof(aviso), 0);
		  pthread_mutex_unlock(&mutexComunicacion);

	  } else if (nuevoESI-> proximaOperacion > 1){

		  log_info(logPlanificador, " El ESI %d no tiene permiso de ejecucion y se aborta (quiso hacer SET o STORE de recurso no tomado) ", nuevoESI->id);
		  claveActual = -1;
		  liberarRecursos(nuevoESI);
		  list_add(listaFinalizados, nuevoESI);
		  log_debug(logPlanificador, "ESI de ID %d en finalizados", nuevoESI->id);
		  uint32_t aviso = -2;
		  send(socketCoordinador, &aviso,  sizeof(aviso), 0);
		  pthread_mutex_unlock(&mutexComunicacion);

	  }


  }

  sem_post(&semPausarPlanificacion);


}

  	  log_trace(logPlanificador, " ESI ID : %d termino su quantum", nuevoESI->id);


      if(matarESI){

    	  log_trace(logPlanificador, "ESI de ID %d fue matado por consola", nuevoESI->id);
		  claveActual = -1;
    	  pthread_mutex_lock(&mutexComunicacion);
    	  uint32_t abortar = -2;
    	  send(nuevoESI->id,&abortar,sizeof(uint32_t),0);
    	  pthread_mutex_unlock(&mutexComunicacion);
		  log_debug(logPlanificador, "Se le informa al ESI que no puede seguir ejecutando y se agrega a finalizados");
    	  liberarRecursos(nuevoESI);
    	  list_add(listaFinalizados,nuevoESI);

    	  pthread_mutex_lock(&mutexAsesino);

    	  matarESI = false;

    	  pthread_mutex_unlock(&mutexAsesino);

    	  printf("ESI de id %d finalizado por consola", nuevoESI->id);

      } else if (finalizar)
	{			//aca con el mensaje del ESI, determino si se bloquea o se finaliza

    	  claveActual = -1;
		  list_add (listaFinalizados, nuevoESI);
		  log_debug (logPlanificador, " ESI de ID %d en finalizados",
				nuevoESI->id);
		  liberarRecursos(nuevoESI);
		  if(bloquearESIActual) bloquearESIActual = false;


	}
      else if (bloquear)
	{			// acá bloqueo usuario

    	  claveActual = -1;
    	  nuevoESI->bloqueadoPorConsola = true;
    	  int rafagas = nuevoESI->rafagasRealizadas;
    	  int estimacion = nuevoESI->estimacionSiguiente;
    	  nuevoESI->estimacionAnterior = estimacion;
    	  nuevoESI->rafagaAnterior = rafagas;
    	  nuevoESI->rafagasRealizadas = 0;
    	  bloquearESI(claveParaBloquearRecurso,nuevoESI);
    	  bloquearRecurso(claveParaBloquearRecurso);
    	  bloquearESIActual = false;

    	  printf("ESI de ID %d bloqueado", nuevoESI->id);
	}
      else if (desalojar)
      {
    	  claveActual = -1;
    	  int rafagas = nuevoESI->rafagasRealizadas;
    	  int estimacion = nuevoESI->estimacionSiguiente - rafagas;
    	  nuevoESI->estimacionSiguiente = estimacion;
    	  nuevoESI->rafagaAnterior = rafagas;
    	  nuevoESI->rafagasRealizadas = 0;
    	  nuevoESI->recienDesalojado = true;
    	  pthread_mutex_lock(&mutexColaListos);
    	  armarCola(nuevoESI);
    	  sem_post(&semContadorColaListos);
    	  pthread_mutex_unlock(&mutexColaListos);
    	  log_trace(logPlanificador," ESI de ID %d desalojado", nuevoESI->id);

      }


    }

}


void
estimarYCalcularTiempos (ESI * nuevo)
{

  estimarProximaRafaga (nuevo);
  float espera = (float) nuevo->tiempoEspera;

  nuevo->tiempoRespuesta =
	calcularTiempoEspera (espera,
			      nuevo->estimacionSiguiente);

  log_info (logPlanificador, "un tiempo calculado: %.6f", nuevo->tiempoRespuesta);


}


void armarCola(ESI * esi){


	log_debug(logPlanificador, "ESI de ID %d quiere entrar en cola de listos. Tiene estimada su proxima rafaga en: %.6f UT y un tiempo de respuesta de %.6f", esi->id, esi->estimacionSiguiente, esi->tiempoRespuesta);

	if(queue_size(colaListos) == 0){

		estimarYCalcularTiempos(esi);
		queue_push(colaListos, esi);
		log_trace(logPlanificador, "ESI ID %d en cola, con tiempo de respuesta %.6f", esi->id, esi->tiempoRespuesta);

	} else if(queue_size(colaListos) > 0){

		queue_push(colaListos, esi);

		t_list * auxiliar = list_create();

		while(!queue_is_empty(colaListos)){

			ESI * esi1 = queue_pop(colaListos);
			float tiempoEspera = (float) esi1->tiempoEspera;
			esi1 -> tiempoRespuesta = calcularTiempoEspera(tiempoEspera,esi1->estimacionSiguiente);
			list_add(auxiliar, esi1);
		}

		list_sort(auxiliar, ordenarESISHRRN);

		int i = 0;
		while(!list_is_empty(auxiliar)){

			ESI * hola = list_remove(auxiliar,i);
			log_trace(logPlanificador, "En cola ESI id : %d, con tiempo de respuesta: %.6f", hola->id, hola->tiempoRespuesta);
			queue_push(colaListos,hola);

		}


	}


}

bool ordenarESISHRRN(void* nodo1, void* nodo2){
	ESI* e1 = (ESI*) nodo1;
	ESI* e2 = (ESI*) nodo2;


	log_info (logPlanificador, "ESI id : %d de tiempo de respuesta %.6f contra ESI a comparar de id: %d y tiempo de respuesta %.6f ", e1->id, e1->tiempoRespuesta, e2->id, e2->tiempoRespuesta);

	if (e1->tiempoRespuesta > e2->tiempoRespuesta){

		return false;

	} else if (e1->tiempoRespuesta == e2->tiempoRespuesta) //Ante empate de estimaciones

	{
		if( !e2->recienDesalojado && e1->recienDesalojado){ //si no es recien llegado, tiene prioridad porque ya estaba en disco

			return false;

		} else if( !e2->recienDesalojado && !e1->recienDesalojado && e2->recienDesbloqueadoPorRecurso && !e1->recienDesbloqueadoPorRecurso ){ // si se da que ninguno de los dos recien fue creado, me fijo si alguno se desbloqueo recien de un recurso

			return false;

		} else if ( e2->recienDesalojado && e1->recienDesalojado && e2->recienDesbloqueadoPorRecurso && !e1->recienDesbloqueadoPorRecurso ){ //si los dos recien llegan, me fijo si el auxiliar recien llego de desbloquearse

			return false;

		} else return true;

		} else return true;

}


float
calcularTiempoEspera (float espera, float estimacionSiguiente)
{
  float respuesta = ((espera + estimacionSiguiente) / estimacionSiguiente);
  return respuesta;

}

void aumentarEspera(){

	t_queue * aux = queue_create();

	while(!queue_is_empty(colaListos)){

		ESI * nuevo= queue_pop(colaListos);
		nuevo->tiempoEspera = nuevo->tiempoEspera +1;
		log_debug(logPlanificador, "Tiempo de espera de ESI ID %d ahora es : %d",nuevo->id, nuevo->tiempoEspera);

		queue_push(aux, nuevo);
	}

	queue_destroy(colaListos);
	colaListos = aux;

}


