#include "planificador.h"

int socketDeEscucha;
bool planificacionSJFTerminada;

void planificacionSJF(bool desalojo){


	pthread_create(&hiloEscuchaConsola, NULL, (void *) lanzarConsola, NULL);

	pthread_create(&hiloEscuchaESI, NULL, (void *) escucharNuevosESIS, NULL);

	while (queue_size(colaListos) == 0){

		if (queue_size(colaListos) > 0){
			break;
		}

	}

	log_info(logPlanificador, "Comienza planificacion SJF");

	pthread_mutex_lock(&mutexColaListos);
	armarColaListos();
	pthread_mutex_unlock(&mutexColaListos);

	log_info(logPlanificador, "Cola de ESI armada");


	while ( !queue_is_empty(colaListos)){

		bool finalizar = false;

		bool bloquear = false;

		bool permiso = true;

		bool desalojar = false;

		ESI * nuevo = queue_pop(colaListos);

		uint32_t operacion;
		uint32_t tamanioRecurso;
		char * recursoPedido;


		while(!finalizar && !bloquear && permiso && !desalojar){


				send(nuevo->id,&CONTINUAR,sizeof(uint32_t),0);
				int respuesta1 = recv(socketCoordinador, &operacion, sizeof(operacion), 0);
				int respuesta2 = recv(socketCoordinador, &tamanioRecurso, sizeof(uint32_t), 0);
				recursoPedido = malloc(sizeof(tamanioRecurso));
				int respuesta3 = recv(socketCoordinador, recursoPedido, sizeof(char)*tamanioRecurso,0);


				if(respuesta1 < 0 || respuesta2 < 0 || respuesta3 < 0){
					log_info(logPlanificador, "conexion con el coordinador rota");
					exit(-1);
				} else {
					nuevo->recursoPedido = recursoPedido;
					nuevo->proximaOperacion = operacion;

				}

				permiso = validarPedido(nuevo->recursoPedido,nuevo);

				if(permiso){

					char * recursoAUsar = nuevo->recursoPedido;

					if(!recursoEnLista(nuevo->recursoPedido, nuevo->recursosAsignado)){

						list_add(listaRecursos, nuevo->recursosAsignado);

					}

					bloquearRecurso(recursoAUsar);

					nuevo->recienLlegado = false;

					log_info(logPlanificador, " ESI de clave %s entra al planificador", nuevo->id );

					send(socketCoordinador, &CONTINUAR, sizeof(uint32_t),0);

					log_info(logPlanificador, " ejecuta una sentencia ");

					nuevo -> rafagaAnterior = nuevo-> rafagaAnterior +1;
					nuevo -> rafagasRealizadas = nuevo -> rafagasRealizadas +1;

					log_info(logPlanificador, "rafagas realizadas del esi %s son %d", nuevo-> id, nuevo->rafagasRealizadas);

					int respuesta ;
					recv(nuevo->id, &respuesta, sizeof(int),0);

					if (respuesta != CONTINUAR)
					{
						finalizar = true;

					} else if(desalojo) // si hay desalojo activo
					{
						pthread_mutex_lock(&mutexColaListos); // por las dudas que entre otro ESI justo en este momento (no lo tengo en cuenta)

						ESI* auxiliar = queue_peek(colaListos);

						if(auxiliar->recienLlegado) //chequeo si el proximo en cola es un recien llegado
						{
							if(auxiliar->estimacionSiguiente < (nuevo->estimacionSiguiente - nuevo->rafagasRealizadas)) // y si su estimacion siguiente es menor que la del que esta en ejecucion menos lo que ya hizo
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

					if (nuevo->id == claveParaBloquearESI)
					{
						bloquear = true;
					}



				} else {

					bloquearESI(nuevo->recursoPedido, nuevo);

				}

			}

		log_info(logPlanificador,"fin de ciclo");

		if( finalizar ){ //aca con el mensaje del ESI, determino si se bloquea o se finaliza

			list_add ( listaFinalizados, nuevo);
			log_info(logPlanificador, " ESI de clave %d en finalizados!", nuevo->id);
			liberarRecursos(nuevo);

		} else if( bloquear ){ // este caso sería para bloqueados por usuario. No se libera clave acá

			nuevo->bloqueadoPorUsuario = true;
			bloquearRecurso(claveParaBloquearRecurso);
			bloquearESI(claveParaBloquearRecurso,nuevo);
			log_info(logPlanificador, " ESI de clave %d en bloqueados para recurso %s", nuevo->id, claveParaBloquearESI);

		} else if (desalojar){

			list_add(listaListos,nuevo); //aca no meto mutex porque si llega otro ESI que esté primero que el que genero el desalojo, lo desalojaria igual.
			log_info(logPlanificador," ESI de clave %d desalojado", nuevo->id);
			armarColaListos();

		}

		ESI_destroy(nuevo);

	}

	planificacionSJFTerminada = true;


}

void estimarTiempos(){

	log_info(logPlanificador,"Estimacion de tiempos empezada \n");

	listaListos = list_map(listaListos, (void *)estimarProximaRafaga);

	log_info(logPlanificador,"terminada la estimacion");

}



void armarColaListos(){

	estimarTiempos();

	log_info(logPlanificador, "Tiempos de los ESI listos estimados");

	log_info(logPlanificador,"armando cola de listos \n");

	ESI * ESIMasRapido;

	while (0 < list_size(listaListos)){

		int i = 1;
		ESIMasRapido = list_get(listaListos, 0);

		while (i < list_size(listaListos) && list_size(listaListos) > 0){

			ESI * ESIAuxiliar = list_get(listaListos, i);
			if (ESIMasRapido->estimacionSiguiente > ESIAuxiliar->estimacionSiguiente){

				ESIMasRapido = ESIAuxiliar;

			} else if (ESIMasRapido->estimacionSiguiente == ESIAuxiliar->estimacionSiguiente) //Ante empate de estimaciones

			{
				if(list_size(ESIAuxiliar->recursosAsignado)> 0 && list_size(ESIMasRapido->recursosAsignado) == 0){ //si tiene recursos asignados el nuevo, quiere decir que estaba ejecutando y tiene prioridad

					ESIMasRapido = ESIAuxiliar;

				} // en cualquier otro caso que no pase eso, seguiria el ESIMasRapido como el seleccionado

			}
			i++;
		}

		log_info(logPlanificador,"ESI numero %d es de estimacion: %d \n", i-1, ESIMasRapido->estimacionSiguiente);

		claveActual = ESIMasRapido->id;
		ESIMasRapido->bloqueadoPorUsuario = false;
		queue_push(colaListos,ESIMasRapido);
		list_remove_and_destroy_by_condition(listaListos, (void *) compararClaves,(void *)ESI_destroy);
	}

	log_info(logPlanificador,"cola armada \n");


}
