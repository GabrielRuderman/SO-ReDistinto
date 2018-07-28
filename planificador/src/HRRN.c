#include "planificador.h"

bool planificacionHRRNTerminada = false;

void
planificacionHRRN (bool desalojo)
{

  pthread_create (&hiloEscuchaConsola, NULL, (void *) lanzarConsola, NULL);

  pthread_create(&hiloEscuchaESI, NULL, (void *) escucharNuevosESIS, NULL);

  while (1)
    {

  while (queue_size(colaListos) == 0){

	if (queue_size(colaListos) > 0){
		break;
	}
  }

  while(pausearPlanificacion){}

  log_info (logPlanificador, "Arraca HRRN");

	bool finalizar = false;

	bool bloquear = false;

	bool permiso = true;

	bool desalojar = false;

	uint32_t operacion;
	uint32_t tamanioRecurso;
	char * recursoPedido;

	ESI* nuevoESI = queue_pop (colaListos);
	log_info (logPlanificador, "clave actual ahora es : %d", nuevoESI->id);

	claveActual = nuevoESI -> id;

  while (!finalizar && !bloquear && permiso && !desalojar && !matarESI)
{

  while(pausearPlanificacion){}

  pthread_mutex_lock(&mutexComunicacion);

  if(nuevoESI->bloqueadoPorClave && !nuevoESI->bloqueadoPorConsola){

	  log_info(logPlanificador, " entra un esi recien desbloqueado de la clave ");
	  permiso = true;
  } else {

		log_info(logPlanificador, "empieza comunicacion");

	  nuevoESI->bloqueadoPorConsola = false;
	  send(nuevoESI->id,&CONTINUAR,sizeof(uint32_t),0);
	  int respuesta1 = recv(socketCoordinador, &operacion, sizeof(operacion), 0);
	  int respuesta2 = recv(socketCoordinador, &tamanioRecurso, sizeof(uint32_t), 0);
	  recursoPedido = malloc(sizeof(char)*tamanioRecurso);
	  int respuesta3 = recv(socketCoordinador, recursoPedido, sizeof(char)*tamanioRecurso,0);

		log_info(logPlanificador, "recibo datos suficientes para corroborar permiso");


	  if(respuesta1 <= 0 || respuesta2 <= 0 || respuesta3 <= 0){
		  log_info(logPlanificador, "conexion con el coordinador rota");
		  liberarGlobales();
		  exit(-1);
	  } else {
		  free(nuevoESI->recursoPedido);
		  log_info(logPlanificador, "se carga recurso pedido : %s", recursoPedido);
		  nuevoESI->recursoPedido = string_new();
		  string_append(&nuevoESI->recursoPedido, recursoPedido);
		  nuevoESI->proximaOperacion = operacion;
	  }

	  permiso = validarPedido(nuevoESI->recursoPedido,nuevoESI);
  }

  if(permiso){


	  nuevoESI->tiempoEspera = 0;

	  log_info(logPlanificador, "ESI tiene permiso de ejecucion");

	  if(operacion == 2){

		  char * valorRecurso;
		  uint32_t tamValor;

		  log_info(logPlanificador, "recibo mas datos para ejecucion");

		  int resp = recv (socketCoordinador, &tamValor,sizeof(uint32_t),0);
		  valorRecurso=malloc(sizeof(char)*tamValor);
		  int resp2 =recv (socketCoordinador,valorRecurso,sizeof(char)*tamValor,0);

		  if(resp <= 0 || resp2 <= 0){

			  log_info(logPlanificador, "conexion con el coordinador rota");
			  liberarGlobales();
			  exit(-1);

		  } else {
			  log_info(logPlanificador, "cargo valor obtenidos");
			  cargarValor(recursoPedido,valorRecurso);
		  }


	  }
	  pthread_mutex_unlock(&mutexComunicacion);

	  log_info(logPlanificador, "terminada comunicacion");

	  if(nuevoESI->proximaOperacion == 1 && !recursoEnLista(nuevoESI)){

		  log_info(logPlanificador, "el recurso se añade a asignados");
		  list_add(nuevoESI->recursosAsignado, nuevoESI->recursoPedido);

	  }

	  log_info(logPlanificador, "se bloquea recurso a usar");

	  bloquearRecurso(recursoPedido);

	  nuevoESI->recienLlegado = false;
	  nuevoESI->recienDesbloqueadoPorRecurso = false;

	  log_info(logPlanificador, " ESI de clave %d entra al planificador", nuevoESI->id );

	  pthread_mutex_lock(&mutexComunicacion);

		if( !nuevoESI->bloqueadoPorClave ){
			log_info(logPlanificador, " le aviso al coordinador de que el ESI tiene permiso");
			send(socketCoordinador, &CONTINUAR, sizeof(uint32_t),0);
		} else {
			log_info(logPlanificador, " el ESI sabe que estaba bloqueado, le aviso a el");
			send(nuevoESI->id, &CONTINUAR, sizeof(uint32_t),0);
			nuevoESI->bloqueadoPorClave = false;
			log_info(logPlanificador, " El esi se entero de que puede continuar");
		}

	  log_info(logPlanificador, " ejecuta una sentencia ");

	  nuevoESI -> rafagaAnterior = nuevoESI-> rafagaAnterior +1;
	  nuevoESI -> rafagasRealizadas = nuevoESI -> rafagasRealizadas +1;

	  pthread_mutex_lock(&mutexColaListos);

	  log_info(logPlanificador, "se procede a aumentar tiempos de espera");

	  aumentarEspera();

	  pthread_mutex_unlock(&mutexColaListos);

	  log_info(logPlanificador, "rafagas realizadas del esi %d son %d", nuevoESI-> id, nuevoESI->rafagasRealizadas);

	  uint32_t respuesta ;
	  int conexion = recv(nuevoESI->id, &respuesta, sizeof(uint32_t),0);

	  pthread_mutex_unlock(&mutexComunicacion);

	  if(conexion <= 0){
		  log_info(logPlanificador, "se rompio la conexion");
		  liberarGlobales();
		  exit(-1);
	  }
	  if (respuesta != CONTINUAR)
	  {
		  log_info(logPlanificador, "el ESI quiere finalizar");
		  finalizar = true;

	  } else if(desalojo && queue_size(colaListos) > 0) // si hay desalojo activo
	  {
		  log_info(logPlanificador, "Chequeo si hay que desalojar");

		  pthread_mutex_lock(&mutexColaListos);
		  ESI* auxiliar = queue_peek(colaListos);

		  if(auxiliar->recienLlegado) //chequeo si el proximo en cola es un recien llegado
		  {
			  if(auxiliar->tiempoEspera < (nuevoESI->tiempoEspera - nuevoESI->rafagasRealizadas)) // y si su estimacion siguiente es menor que la del que esta en ejecucion menos lo que ya hizo
			  {
				  log_info(logPlanificador, "se genera desalojo");

				  desalojar = true; //se va a desalojar
			  } else
			  {
				  log_info(logPlanificador, "no hay necesidad de desalojar, se actualiza cola");

				  limpiarRecienLlegados(); // Si no es menor la estimacion, los recien llegados ya no tendrian validez, los actualizo
			  }
		  }
		  pthread_mutex_unlock(&mutexColaListos);

	  }

	  if (nuevoESI->id == claveParaBloquearESI)
	  {
		  log_info(logPlanificador, "se pide bloqueo del esi");

		  bloquear = true;
	  }



  } else {

	  if(nuevoESI-> proximaOperacion == 1){

		  log_info(logPlanificador, "El ESI no tiene permiso de ejecucion, se bloquea");

		  uint32_t aviso = 0;
		  send(socketCoordinador, &aviso,  sizeof(aviso), 0);

		  bloquearESI(nuevoESI->recursoPedido, nuevoESI);
		  pthread_mutex_unlock(&mutexComunicacion);
	  } else if (nuevoESI-> proximaOperacion > 1){

		  log_info(logPlanificador, " El esi no tiene permiso de ejecucion y se aborta (quiso hacer SET o STORE de recurso no tomado ");
		  log_info(logPlanificador, "se procede a liberar sus recursos");
		  liberarRecursos(nuevoESI);
		  list_add(listaFinalizados, nuevoESI);
		  log_info(logPlanificador, "ESI de clave % d en finalizados", nuevoESI->id);
		  uint32_t aviso = -2;
		  send(socketCoordinador, &aviso,  sizeof(aviso), 0);
		  pthread_mutex_unlock(&mutexComunicacion);

	  }


  }

}

      log_info (logPlanificador, "finalizada su rafaga");

      if(matarESI){

    	  log_info(logPlanificador, "ESI de clave %d fue matado por consola", nuevoESI->id);
		  log_info(logPlanificador, "le comunico al coordinador");
    	  pthread_mutex_lock(&mutexComunicacion);
    	  uint32_t abortar = -2;
    	  send(socketCoordinador,&abortar,sizeof(uint32_t),0);
    	  send(socketCoordinador,&claveActual,sizeof(claveActual),0);
    	  pthread_mutex_unlock(&mutexComunicacion);
		  log_info(logPlanificador, "terminada comunicacion");
		  log_info(logPlanificador, " se liberan sus recursos y se lo destruye ");

    	  liberarRecursos(nuevoESI);
    	  ESI_destroy(nuevoESI);

    	  pthread_mutex_lock(&mutexAsesino);

    	  matarESI = false;

    	  pthread_mutex_unlock(&mutexAsesino);
      } else if (finalizar)
	{			//aca con el mensaje del ESI, determino si se bloquea o se finaliza

		  list_add (listaFinalizados, nuevoESI);
		  log_info (logPlanificador, " ESI de clave %s en finalizados!",
				nuevoESI->id);
		  liberarRecursos(nuevoESI);
		  if(bloquearESIActual) bloquearESIActual = false;


	}
      else if (bloquear)
	{			// acá bloqueo usuario

    	  nuevoESI->bloqueadoPorConsola = true;
    	  bloquearRecurso(claveParaBloquearRecurso);
    	  bloquearESI(claveParaBloquearRecurso,nuevoESI);
    	  bloquearESIActual = false;
    	  log_info(logPlanificador, " ESI de clave %s en bloqueados para recurso %s", nuevoESI->id, claveParaBloquearESI);


	}
      else if (desalojar)
      {
    	  armarCola(nuevoESI);
    	  log_info(logPlanificador," ESI de clave %d desalojado", nuevoESI->id);

      }


    }
  planificacionHRRNTerminada = true;

}


void
estimarYCalcularTiempos (ESI * nuevo)
{

  log_info (logPlanificador,
	    "Empieza la estimación de ráfagas y tiempos de respuesta");

  estimarProximaRafaga (nuevo);
  nuevo->tiempoRespuesta =
	calcularTiempoEspera (nuevo->tiempoEspera,
			      nuevo->estimacionSiguiente);

  log_info (logPlanificador, "un tiempo calculado: %.6f", nuevo->tiempoRespuesta);


}


void armarCola(ESI * esi){

	log_info(logPlanificador, " ESI de ID %d quiere entrar en cola de listos", esi->id);

	estimarProximaRafaga(esi);

	log_info(logPlanificador, "Tiene estimación proxima rafaga: %d", esi->estimacionSiguiente);

	log_info(logPlanificador,"Se procede a meterlo en cola listos ");


	if(queue_size(colaListos) == 0){

		log_info(logPlanificador, "la cola estaba vacía! Entra directo");
		queue_push(colaListos, esi);
		log_info(logPlanificador, "adentro!");

	} else if(queue_size(colaListos) > 0){


		log_info(logPlanificador, "La cola tiene tamaño de %d", queue_size(colaListos));
		queue_push(colaListos, esi);

		t_list * auxiliar = list_create();

		log_info(logPlanificador,"armando lista auxiliar");

		while(!queue_is_empty(colaListos)){
			ESI * esi1 = queue_pop(colaListos);
			list_add(auxiliar, esi1);
			log_info(logPlanificador, " id : %d a lista", esi1->id);

		}
		log_info(logPlanificador, "la lista quedo de : %d", list_size(auxiliar));


		list_sort(auxiliar, ordenarESISHRRN);

		log_info(logPlanificador, "la lista quedo de : %d", list_size(auxiliar));

		int i = 0;
		while(!list_is_empty(auxiliar)){

			ESI * hola = list_remove(auxiliar,i);
			log_info(logPlanificador, "meto en cola ESI id : %d", hola->id);
			queue_push(colaListos,hola);

		}


	}

	log_info(logPlanificador,"cola armada ");


}

bool ordenarESISHRRN(void* nodo1, void* nodo2){
	ESI* e1 = (ESI*) nodo1;
	ESI* e2 = (ESI*) nodo2;


	log_info (logPlanificador, "ESI  : %d contra ESI a comparar : %d", e1->estimacionSiguiente, e2->estimacionSiguiente);

	if (e1->tiempoRespuesta > e2->tiempoRespuesta){

		log_info(logPlanificador, " el esi a comparar fue se especula que será mas rapido");

		return false;

	} else if (e1->tiempoRespuesta == e2->tiempoRespuesta) //Ante empate de estimaciones

	{
		log_info(logPlanificador, "hay empate de estimaciones");

		if( !e2->recienDesalojado && e1->recienDesalojado){ //si no es recien llegado, tiene prioridad porque ya estaba en disco

			log_info(logPlanificador, "gana esi nuevo por ser recien llegado");

			return false;

		} else if( !e2->recienDesalojado && !e1->recienDesalojado && e2->recienDesbloqueadoPorRecurso && !e1->recienDesbloqueadoPorRecurso ){ // si se da que ninguno de los dos recien fue creado, me fijo si alguno se desbloqueo recien de un recurso

			log_info(logPlanificador, "gana esi nuevo por ser recien desbloqueado por recurso");

			return false;

		} else if ( e2->recienDesalojado && e1->recienDesalojado && e2->recienDesbloqueadoPorRecurso && !e1->recienDesbloqueadoPorRecurso ){ //si los dos recien llegan, me fijo si el auxiliar recien llego de desbloquearse

			log_info(logPlanificador, "gana esi nuevo por ser recien desbloqueado por recurso");

			return false;

		} else return true;

		} else return true;

}

	/*

	log_info(logPlanificador, "ESI de ID %d quiere entrar en cola de listos", esi->id);

	estimarYCalcularTiempos(esi);

	log_info(logPlanificador, "Tiene estimación proxima rafaga: %d", esi->estimacionSiguiente);

	log_info(logPlanificador,"Se procede a meterlo en cola listos ");

	if(queue_size(colaListos) == 0){

		log_info(logPlanificador, "la cola estaba vacía! Entra directo");
		queue_push(colaListos, esi);
		log_info(logPlanificador, "adentro!");

	} else if(queue_size(colaListos) > 0){

		log_info(logPlanificador, " cola llena, procedo a meter el esi de forma ordenada ");
		t_queue * colaAux; // Apunto a cola listos original para ir sacando de a uno de ahi
		t_queue * final = queue_create(); // cola final ordenada
		t_list * llenada = list_create();

		while (0 < queue_size(colaListos)){

			log_info(logPlanificador, " entro a comparar los tiempos");
			ESI * ESIMasRapido = queue_peek(colaListos); // Chusmeo de la cola listos

			colaAux = colaListos;
			int i = 0;
			bool enLista = idEnLista(llenada,ESIMasRapido);

			while (i < queue_size(colaAux) && !enLista){ //voy sacando de a uno

				ESI * ESIAuxiliar = queue_pop(colaAux);

				if(!idEnLista(llenada,ESIAuxiliar)){

					log_info (logPlanificador, "ESI original : %d contra ESI a comparar : %d", ESIMasRapido->estimacionSiguiente, ESIAuxiliar->estimacionSiguiente);

					if (ESIMasRapido->tiempoRespuesta > ESIAuxiliar->tiempoRespuesta){

						log_info(logPlanificador, " el esi a comparar fue se especula que será mas rapido");

						ESIMasRapido = ESIAuxiliar;

					} else if (ESIMasRapido->tiempoRespuesta == ESIAuxiliar->tiempoRespuesta) //Ante empate de estimaciones

					{
						log_info(logPlanificador, "hay empate de estimaciones");

						if( !ESIAuxiliar->recienLlegado && ESIMasRapido->recienLlegado){ //si no es recien llegado, tiene prioridad porque ya estaba en disco

							log_info(logPlanificador, "gana esi nuevo por ser recien llegado");

							ESIMasRapido = ESIAuxiliar;

						} else if( !ESIAuxiliar->recienLlegado && !ESIMasRapido->recienLlegado && ESIAuxiliar->recienDesbloqueadoPorRecurso && !ESIMasRapido->recienDesbloqueadoPorRecurso ){ // si se da que ninguno de los dos recien fue creado, me fijo si alguno se desbloqueo recien de un recurso

							log_info(logPlanificador, "gana esi nuevo por ser recien desbloqueado por recurso");

							ESIMasRapido = ESIAuxiliar;

						} else if ( ESIAuxiliar->recienLlegado && ESIMasRapido->recienLlegado && ESIAuxiliar->recienDesbloqueadoPorRecurso && !ESIMasRapido->recienDesbloqueadoPorRecurso ){ //si los dos recien llegan, me fijo si el auxiliar recien llego de desbloquearse

							log_info(logPlanificador, "gana esi nuevo por ser recien desbloqueado por recurso");

							ESIMasRapido = ESIAuxiliar;

						}

					}
				} log_info(logPlanificador, " el esi para comparar esta en lista, lo salteo ");

				i++;
			}

			if(enLista){

				log_info(logPlanificador, " el ESI que chusmeamos de la cola original estaba en la lista, lo sacamos");
				ESIMasRapido = queue_pop(colaListos);

			}else {

				ESIMasRapido->bloqueadoPorUsuario = false;
				log_info(logPlanificador,"ESI a entrar en la cola es de estimacion: %d",  ESIMasRapido->estimacionSiguiente);
				list_add(llenada, ESIMasRapido);
				queue_push(final, ESIMasRapido); // Meto el ESI mas rapido de los comparados a la cola final

			}

		}


		log_info(logPlanificador,"cola armada ");
		queue_destroy(colaListos);
		list_destroy(llenada);
		colaListos = final;

	}




}

void
armarCola ()
{

	  estimarYCalcularTiempos();

	  log_info(logPlanificador, "Tiempos de los ESI listos estimados");

	  log_info (logPlanificador, "armando cola de listos ");

	  ESI *ESIPrioridad;

	  while (0 < list_size (listaListos))
		{
		  int i = 1;
		  ESIPrioridad = list_get (listaListos, 0);

		  while (i < list_size (listaListos) && list_size(listaListos) > 0)
		{

		  ESI *ESIAuxiliar = list_get (listaListos, i);
		  if (ESIPrioridad->tiempoRespuesta < ESIAuxiliar->tiempoRespuesta)
			{

			  ESIPrioridad = ESIAuxiliar;

			} else if ( ESIPrioridad-> tiempoRespuesta == ESIAuxiliar -> tiempoRespuesta){

				log_info(logPlanificador, "igualdad en tiempos de respuesta");

				if( !ESIAuxiliar->recienLlegado && ESIPrioridad->recienLlegado){ //si no es recien llegado, tiene prioridad porque ya estaba en disco

					log_info(logPlanificador, "gana esi nuevo por recien llegado");

					ESIPrioridad = ESIAuxiliar;

				} else if( !ESIAuxiliar->recienLlegado && !ESIPrioridad->recienLlegado && ESIAuxiliar->recienDesbloqueadoPorRecurso && !ESIPrioridad->recienDesbloqueadoPorRecurso ){ // si se da que ninguno de los dos recien fue creado, me fijo si alguno se desbloqueo recien de un recurso

					log_info(logPlanificador, "gana esi nuevo por recien desbloqueado de recurso");

					ESIPrioridad = ESIAuxiliar;

				} else if ( ESIAuxiliar->recienLlegado && ESIPrioridad->recienLlegado && ESIAuxiliar->recienDesbloqueadoPorRecurso && !ESIPrioridad->recienDesbloqueadoPorRecurso ){ //si los dos recien llegan, me fijo si el auxiliar recien llego de desbloquearse

					log_info(logPlanificador, "gana esi nuevo por ser recien desbloqueado por recurso");

					ESIPrioridad = ESIAuxiliar;

				}
			}

		  i++;
		}

		  log_info (logPlanificador,
			"ESI entra en cola y es de tiempo de respuesta: %.6f, y su proxima rafaga se estima en %d ",
			ESIPrioridad->tiempoRespuesta,
			ESIPrioridad->estimacionSiguiente);

		  claveActual = ESIPrioridad->id;
		  ESIPrioridad->bloqueadoPorUsuario = false;
		  queue_push (colaListos, ESIPrioridad);
		  log_info(logPlanificador, "esi en cola. Eliminandolo de la lista de listos");

		  list_remove_by_condition (listaListos, (void *) compararClaves);

		}

	  log_info (logPlanificador, "cola armada \n");

*/


float
calcularTiempoEspera (float espera, int estimacionSiguiente)
{

  log_info (logPlanificador, "calculando tiempo respuesta..");
  return (espera + estimacionSiguiente) / estimacionSiguiente;

}

void aumentarEspera(){

	t_queue * aux = queue_create();

	while(!queue_is_empty(colaListos)){

		ESI * nuevo= queue_pop(colaListos);
		log_info(logPlanificador, "aumentando tiempo espera para : %d", nuevo->id);
		nuevo->tiempoEspera = nuevo->tiempoEspera +1;
		log_info(logPlanificador, "queda : %d", nuevo->tiempoEspera);

		queue_push(aux, nuevo);
	}

	queue_destroy(colaListos);
	colaListos = aux;

}


