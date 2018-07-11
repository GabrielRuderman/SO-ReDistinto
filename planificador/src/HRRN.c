#include "planificador.h"

bool planificacionHRRNTerminada = false;

void
planificacionHRRN (bool desalojo)
{


  pthread_create (&hiloEscuchaConsola, NULL, (void *) lanzarConsola, NULL);

  pthread_create(&hiloEscuchaESI, NULL, (void *) escucharNuevosESIS, NULL);

  while (queue_size(colaListos) == 0){

	if (queue_size(colaListos) > 0){
		break;
	}
  }

  log_info (logPlanificador, "Arraca HRRN");


  estimarYCalcularTiempos ();

  log_info (logPlanificador, "Todos los tiempos listos");

  pthread_mutex_lock(&mutexColaListos);
  armarCola ();
  pthread_mutex_unlock(&mutexColaListos);

  log_info (logPlanificador, "Cola armada");


  while (!queue_is_empty (colaListos)) //todo implementar los hilos
    {

		bool finalizar = false;

		bool bloquear = false;

		bool permiso = true;

		bool desalojar = false;

		uint32_t operacion;
		uint32_t tamanioRecurso;
		char * recursoPedido;

		ESI* nuevoESI = queue_pop (colaListos);

      while (!finalizar && !bloquear && permiso && !desalojar)
	{


	  send(nuevoESI->id,&CONTINUAR,sizeof(uint32_t),0);
	  int respuesta1 = recv(socketCoordinador, &operacion, sizeof(operacion), 0);
	  int respuesta2 = recv(socketCoordinador, &tamanioRecurso, sizeof(uint32_t), 0);
	  recursoPedido = malloc(sizeof(tamanioRecurso));
	  int respuesta3 = recv(socketCoordinador, recursoPedido, sizeof(char)*tamanioRecurso,0);

	  if(respuesta1 < 0 || respuesta2 < 0 || respuesta3 < 0){
		  log_info(logPlanificador, "conexion con el coordinador rota");
		  exit(-1);
	  } else {
		  nuevoESI->recursoPedido = recursoPedido;
		  nuevoESI->proximaOperacion = operacion;

	  }

	  permiso = validarPedido(nuevoESI->recursoPedido,nuevoESI);

	  if(permiso){

		  char * recursoAUsar = nuevoESI->recursoPedido;

		  if(!recursoEnLista(nuevoESI->recursoPedido, nuevoESI->recursosAsignado)){

			  list_add(listaRecursos, nuevoESI->recursosAsignado);

		  }

		  bloquearRecurso(recursoAUsar);

		  nuevoESI->recienLlegado = false;

		  log_info(logPlanificador, " ESI de clave %s entra al planificador", nuevoESI->id );

		  send(socketCoordinador, &CONTINUAR, sizeof(uint32_t),0);

		  log_info(logPlanificador, " ejecuta una sentencia ");

		  nuevoESI -> rafagaAnterior = nuevoESI-> rafagaAnterior +1;
		  nuevoESI -> rafagasRealizadas = nuevoESI -> rafagasRealizadas +1;

		  log_info(logPlanificador, "rafagas realizadas del esi %s son %d", nuevoESI-> id, nuevoESI->rafagasRealizadas);

		  int respuesta ;
		  int conexion = recv(nuevoESI->id, &respuesta, sizeof(int),0);

		  if(conexion < 0){
			  log_info(logPlanificador, "se rompio la conexion");
			  exit(-1);
		  }
		  if (respuesta != CONTINUAR)
		  {
			  finalizar = true;

		  } else if(desalojo) // si hay desalojo activo
		  {
			  pthread_mutex_lock(&mutexColaListos);
			  ESI* auxiliar = queue_peek(colaListos);

			  if(auxiliar->recienLlegado) //chequeo si el proximo en cola es un recien llegado
			  {
				  if(auxiliar->tiempoEspera < (nuevoESI->tiempoEspera - nuevoESI->rafagasRealizadas)) // y si su estimacion siguiente es menor que la del que esta en ejecucion menos lo que ya hizo
				  {
					  desalojar = true; //se va a desalojar
				  } else
				  {
					  limpiarRecienLlegados(); // Si no es menor la estimacion, los recien llegados ya no tendrian validez, los actualizo
				  }
			  }
			  ESI_destroy(auxiliar);
			  pthread_mutex_unlock(&mutexColaListos);

		  }

		  if (nuevoESI->id == claveParaBloquearESI)
		  {
			  bloquear = true;
		  }



	  } else {

		  bloquearESI(nuevoESI->recursoPedido, nuevoESI);

	  }

	}

      log_info (logPlanificador, "finalizada su rafaga");

      if (finalizar)
	{			//aca con el mensaje del ESI, determino si se bloquea o se finaliza

		  list_add (listaFinalizados, nuevoESI);
		  log_info (logPlanificador, " ESI de clave %s en finalizados!",
				nuevoESI->id);
		  liberarRecursos(nuevoESI);

	}
      else if (bloquear)
	{			// acá bloqueo usuario

    	  nuevoESI->bloqueadoPorUsuario = true;
    	  bloquearRecurso(claveParaBloquearRecurso);
    	  bloquearESI(claveParaBloquearRecurso,nuevoESI);

    	  log_info(logPlanificador, " ESI de clave %s en bloqueados para recurso %s", nuevoESI->id, claveParaBloquearESI);


	}
      else if (desalojar)
      {
    	  list_add(listaListos,nuevoESI);
    	  log_info(logPlanificador," ESI de clave %d desalojado", nuevoESI->id);
    	  armarCola();

      }

      ESI_destroy(nuevoESI);


    }
  planificacionHRRNTerminada = true;



}



void
estimarYCalcularTiempos ()
{


  log_info (logPlanificador,
	    "Empieza la estimación de ráfagas y tiempos de respuesta");
  int i = 0;

  ESI *nuevo;


  while (i <= list_size (listaListos))
    {

      log_info (logPlanificador, "Entra ESI clave %d", nuevo->id);

      nuevo = list_remove (listaListos, i);

      estimarProximaRafaga (nuevo);

      nuevo->tiempoEspera =
	calcularTiempoEspera (nuevo->tiempoEspera,
			      nuevo->estimacionSiguiente);

      log_info (logPlanificador, "un tiempo calculado");

    }


}


void
armarCola ()
{


	  estimarYCalcularTiempos();

	  log_info(logPlanificador, "Tiempos de los ESI listos estimados");

	  log_info (logPlanificador, "armando cola de listos \n");

	  ESI *ESIPrioridad;

	  while (0 < list_size (listaListos))
		{

		  int i = 1;
		  ESIPrioridad = list_get (listaListos, 0);

		  while (i <= list_size (listaListos) && list_size(listaListos) > 0)
		{

		  ESI *ESIAuxiliar = list_get (listaListos, i);
		  if (ESIPrioridad->tiempoEspera < ESIAuxiliar->tiempoEspera)
			{

			  ESIPrioridad = ESIAuxiliar;

			} else if ( ESIPrioridad-> tiempoEspera == ESIAuxiliar -> tiempoEspera){

				if(list_size(ESIAuxiliar->recursosAsignado)> 0 && list_size(ESIPrioridad->recursosAsignado) == 0){

					ESIPrioridad = ESIAuxiliar;
				}
			}

		  i++;
		}

		  log_info (logPlanificador,
			"ESI numero %d, de clave %s, es de tiempo de respuesta: %.6f, y su proxima rafaga se estima en %d ",
			i - 1, ESIPrioridad->id, ESIPrioridad->tiempoEspera,
			ESIPrioridad->estimacionSiguiente);

		  claveActual = ESIPrioridad->id;
		  ESIPrioridad->bloqueadoPorUsuario = false;
		  queue_push (colaListos, ESIPrioridad);
		  list_remove_and_destroy_by_condition (listaListos, (void *) compararClaves,(void *) ESI_destroy);

		}

	  log_info (logPlanificador, "cola armada \n");

}



float
calcularTiempoEspera (float espera, int estimacionSiguiente)
{

  log_info (logPlanificador, "calculando..");
  return (espera + estimacionSiguiente) / estimacionSiguiente;

}
