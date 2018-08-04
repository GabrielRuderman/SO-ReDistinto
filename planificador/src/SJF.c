#include "planificador.h"

bool planificacionSJFTerminada;

void planificacionSJF(bool desalojo) {

	pthread_create(&hiloEscuchaConsola, NULL, (void *) lanzarConsola, NULL);

	pthread_create(&hiloEscuchaESI, NULL, (void *) escucharNuevosESIS, NULL);

	log_info(logPlanificador, "Comienza algoritmo SJF");


	while (!salir) {

		bool finalizar = false;

		bool bloquear = false;

		bool permiso = true;

		bool desalojar = false;

		ESI * nuevo;

		pthread_mutex_lock(&mutexPauseo);
		if(pausearPlanificacion){

			sem_wait(&semPausarPlanificacion);
		}

		pthread_mutex_unlock(&mutexPauseo);

		sem_wait(&semComodinColaListos);
		sem_wait(&semContadorColaListos);
		pthread_mutex_lock(&mutexColaListos);

		nuevo = queue_pop(colaListos);
		claveActual = nuevo->id;

		pthread_mutex_unlock(&mutexColaListos);
		sem_post(&semComodinColaListos);


		log_trace(logPlanificador, "ID actual en planificacion es : %d", nuevo->id);

		uint32_t operacion;
		uint32_t tamanioRecurso;
		char * recursoPedido;


		if(claveActual == idSalir){

			ESI_destroy(nuevo);
			log_trace(logPlanificador, "terminando planificacion HRRN");
			break;

		}



		while (!finalizar && !bloquear && permiso && !desalojar && !matarESI && !salir) {

			pthread_mutex_lock(&mutexPauseo);
			if(pausearPlanificacion){

				sem_wait(&semPausarPlanificacion);

			}
			pthread_mutex_unlock(&mutexPauseo);
			pthread_mutex_lock(&mutexComunicacion);

			if (nuevo->bloqueadoPorClave && !nuevo->bloqueadoPorConsola ) {
				log_debug(logPlanificador,
						"Entra un ESI %d recien desbloqueado de clave", nuevo->id);
				permiso = true;
			} else {
				uint32_t continuacion;
				log_info(logPlanificador, "Comienza comunicacion con ESI y Coordinador para obtencion de datos");
				send(nuevo->id, &CONTINUAR, sizeof(uint32_t), 0);
				int respuesta0 =recv(nuevo->id, &continuacion,sizeof(uint32_t),0);

				if(respuesta0 < 0){

					log_error(logPlanificador, "Conexion con ESI rota");
					finalizar=true;
					pthread_mutex_unlock(&mutexComunicacion);
					sem_post(&semPausarPlanificacion);

					break;
				}
				if(continuacion == 0){
					log_error(logPlanificador, " El ESI de ID : %d se aborta por un motivo desconocido", nuevo->id);
					finalizar = true;
					pthread_mutex_unlock(&mutexComunicacion);
					sem_post(&semPausarPlanificacion);

					break;
				}
				int respuesta1 = recv(socketCoordinador, &operacion,
						sizeof(operacion), 0);
				int respuesta2 = recv(socketCoordinador, &tamanioRecurso,
						sizeof(uint32_t), 0);
				recursoPedido = malloc(sizeof(char) * tamanioRecurso);
				int respuesta3 = recv(socketCoordinador, recursoPedido,
						tamanioRecurso, 0);

				if (respuesta1 <= 0 || respuesta2 <= 0 || respuesta3 <= 0) {
					log_error(logPlanificador,
							"Conexion con el coordinador rota. Me cierro");
					pthread_cancel(hiloEscuchaESI);
					salir = true;
					break;
				} else {

					nuevo->recursoPedido = string_new();
					string_append(&(nuevo->recursoPedido), recursoPedido);
					log_debug(logPlanificador,
							"Se carga recurso pedido: %s",nuevo->recursoPedido);
					nuevo->proximaOperacion = operacion;

				}

				permiso = validarPedido(nuevo->recursoPedido, nuevo);
			}

			if (permiso) {

				nuevo->rafagaAnterior = 0;

				log_info(logPlanificador, "El esi tiene permiso de ejecucion");

				if (nuevo->proximaOperacion == 2) {

					char * valorRecurso;
					uint32_t tamValor;

					int resp = recv(socketCoordinador, &tamValor,
							sizeof(uint32_t), 0);
					valorRecurso = malloc(sizeof(char) * tamValor);
					int resp2 = recv(socketCoordinador, valorRecurso, tamValor,
							0);

					if (resp <= 0 || resp2 <= 0) {

						log_error(logPlanificador,
								"Conexion con el coordinador rota. Me cierro");
						pthread_cancel(hiloEscuchaESI);
						salir = true;
						break;

					} else {

						cargarValor(nuevo->recursoPedido, valorRecurso);
					}

				}
				pthread_mutex_unlock(&mutexComunicacion);

				/* ESTO TIENE QUE ESTAR -> Explicacion:
				 *
				 * Si un ESI se desbloquea de un bloqueo por consola, el recurso que se le asigna cuando se desbloquea
				 * no va a ser el que el realmente necesita. Por ende, se lo añade ahora.
				 * Es una comprobacion de mas para los demas esis desbloqueados, pero no deberia traer problemas
				 * de performance
				 *
				 */

				if (!recursoEnLista(nuevo)) {

					log_debug(logPlanificador, "Clave : %s se agrega a los asignados del ESI",
							nuevo->recursoPedido);

					list_add(nuevo->recursosAsignado, nuevo->recursoPedido);

				}

				bloquearRecurso(nuevo->recursoPedido);

				nuevo->recienLlegado = false;
				nuevo->recienDesbloqueadoPorRecurso = false;

				pthread_mutex_lock(&mutexComunicacion);

				if (nuevo->bloqueadoPorClave && !nuevo->bloqueadoPorConsola) {

					send(nuevo->id, &CONTINUAR, sizeof(uint32_t), 0);
					nuevo->bloqueadoPorClave = false;
					log_info(logPlanificador,
							" ESI estaba bloqueado por clave. Se le informa que puede continuar ");

				} else {
					nuevo->bloqueadoPorClave = false;
					nuevo->bloqueadoPorConsola = false;
					log_info(logPlanificador,
							" Se le informa al coordinador que el ESI tiene permiso de ejecucion");
					send(socketCoordinador, &CONTINUAR, sizeof(uint32_t), 0);
				}

				nuevo->rafagasRealizadas = nuevo->rafagasRealizadas + 1;

				log_trace(logPlanificador,
						"Instrucciones realizadas del esi %d son %d en este ciclo", nuevo->id,
						nuevo->rafagasRealizadas);

				uint32_t respuesta;
				int resp = recv(nuevo->id, &respuesta, sizeof(uint32_t), 0);

				pthread_mutex_unlock(&mutexComunicacion);

				if (resp <= 0) {

					log_error(logPlanificador, " Conexion rota con el ESI. Se lo finaliza ");
					finalizar = true;
					sem_post(&semPausarPlanificacion);
					break;
				}

				if(nuevo->proximaOperacion == 3) { //si realizo un STORE, se libera el recurso que estaba usando

					liberarUnRecurso(nuevo);

				}
				pthread_mutex_lock(&mutexColaListos); // por las dudas que entre otro ESI justo en este momento (no lo tengo en cuenta)

				if (respuesta != CONTINUAR) {
					log_debug(logPlanificador, " El ESI me informa que va a finalizar ");

					finalizar = true;

				} else if (desalojo && queue_size(colaListos) > 0) // si hay desalojo activo
						{


					ESI* auxiliar = queue_peek(colaListos);

					if (auxiliar->recienLlegado
							|| auxiliar->recienDesbloqueadoPorRecurso) //chequeo si el proximo en cola es un recien llegado
							{
						if (auxiliar->estimacionSiguiente
								< (nuevo->estimacionSiguiente
										- nuevo->rafagasRealizadas)) // y si su estimacion siguiente es menor que la del que esta en ejecucion menos lo que ya hizo
								{
							  log_trace(logPlanificador, "Se genera desalojo del ESI %d de tiempo de respuesta: %.6f con ESI ID %d de tiempo de respuesta: %.6f", nuevo->id, nuevo->tiempoRespuesta, auxiliar->id, auxiliar->tiempoRespuesta);

							desalojar = true; //se va a desalojar
						} else {
							log_info(logPlanificador,
									"No ocurre desalojo. Se actualiza cola de listos");

							limpiarRecienLlegados(); // Si no es menor la estimacion, los recien llegados ya no tendrian validez, los actualizo
						}
					}
				}
				pthread_mutex_unlock(&mutexColaListos);


				if (nuevo->id == claveParaBloquearESI) {
					log_trace(logPlanificador, "Se pidio bloqueo del ESI mediante la consola");

					bloquear = true;
				}

			} else {

				if (nuevo->proximaOperacion == 1) { //Caso GET

					log_error(logPlanificador,
							" El ESI no tiene permiso de ejecucion y se bloquea ");
					claveActual = -1;
					int rafagas = nuevo->rafagasRealizadas;
					int estimacion = nuevo->estimacionSiguiente;
					nuevo->estimacionAnterior = estimacion;
					nuevo->rafagaAnterior = rafagas;
					nuevo->rafagasRealizadas = 0;
					bloquearESI(nuevo->recursoPedido, nuevo);
					uint32_t aviso = 0;
					send(socketCoordinador, &aviso, sizeof(aviso), 0);
					pthread_mutex_unlock(&mutexComunicacion);

				} else if (nuevo->proximaOperacion > 1) {

					log_error(logPlanificador,
							" El ESI %d no tiene permiso de ejecucion y se aborta (quiso hacer SET o STORE de recurso no tomado) ", nuevo->id);
					claveActual = -1;
					liberarRecursos(nuevo);
					list_add(listaFinalizados, nuevo);
					log_debug(logPlanificador, " ESI ID %d en finalizados", nuevo->id);
					uint32_t aviso = -2;
					send(socketCoordinador, &aviso, sizeof(aviso), 0);
					pthread_mutex_unlock(&mutexComunicacion);
				}

			}

			sem_post(&semPausarPlanificacion);

		}

		log_trace(logPlanificador, " ESI ID : %d termino su quantum", nuevo->id);

		if (matarESI) {

			log_info(logPlanificador,
					"ESI de ID %d fue mandado a matar por consola",
					nuevo->id);
			claveActual = -1;

			pthread_mutex_lock(&mutexComunicacion);

			uint32_t abortar = -2;
			send(nuevo->id, &abortar, sizeof(uint32_t), 0);

			pthread_mutex_unlock(&mutexComunicacion);

			log_debug(logPlanificador, "Se le informa al ESI que no puede seguir ejecutando y se agrega a finalizados");
			liberarRecursos(nuevo);
			list_add(listaFinalizados, nuevo);

			pthread_mutex_lock(&mutexAsesino);

			matarESI = false;

			pthread_mutex_unlock(&mutexAsesino);

			printf("ESI de ID %d finalizado por consola", nuevo->id);

		} else if (finalizar) { //aca con el mensaje del ESI, determino si se bloquea o se finaliza

			claveActual = -1;
			list_add(listaFinalizados, nuevo);
			log_debug(logPlanificador, " ESI de ID %d en finalizados",
					nuevo->id);
			liberarRecursos(nuevo);
			if (bloquearESIActual)
				bloquearESIActual = false;

		} else if (bloquear) { // este caso sería para bloqueados por usuario. No se libera clave acá
			claveActual = -1;
			nuevo->bloqueadoPorConsola = true;
			int rafagas = nuevo->rafagasRealizadas;
			int estimacion = nuevo->estimacionSiguiente;
			nuevo->estimacionAnterior = estimacion;
			nuevo->rafagaAnterior = rafagas;
			nuevo->rafagasRealizadas = 0;
			bloquearESI(claveParaBloquearRecurso, nuevo);
			bloquearRecurso(claveParaBloquearRecurso);
			bloquearESIActual = false;
			printf("ESI de ID %d bloqueado", nuevo->id);


		} else if (desalojar) {

			claveActual = -1;
			int rafagas = nuevo->rafagasRealizadas;
			int estimacion = nuevo->estimacionSiguiente - rafagas;
			nuevo->estimacionSiguiente = estimacion;
			nuevo->rafagaAnterior = rafagas;
			nuevo->rafagasRealizadas = 0;
			nuevo->recienDesalojado = true;
			pthread_mutex_lock(&mutexColaListos);
			armarColaListos(nuevo);
			sem_post(&semContadorColaListos);
			pthread_mutex_unlock(&mutexColaListos);
			log_trace(logPlanificador, " ESI de ID %d desalojado", nuevo->id);

		}

	}

	  sem_post(&semSalir);

}

void armarColaListos(ESI * esi) {

	estimarProximaRafaga(esi);

	log_debug(logPlanificador, " ESI de ID %d quiere entrar en cola de listos. Tiene estimación proxima rafaga: %.6f", esi->id,
			esi->estimacionSiguiente);

	if (queue_size(colaListos) == 0) {
		queue_push(colaListos, esi);
		log_trace(logPlanificador, "ESI ID: %d en cola, con estimacion: %.6f", esi->id, esi->estimacionSiguiente);

	} else if (queue_size(colaListos) > 0) {

		queue_push(colaListos, esi);

		t_list * auxiliar = list_create();

		while (!queue_is_empty(colaListos)) {
			ESI * esi1 = queue_pop(colaListos);
			list_add(auxiliar, esi1);

		}

		list_sort(auxiliar, ordenarESIS);

		while (!list_is_empty(auxiliar)) {

			ESI * hola = list_remove(auxiliar, 0);
			log_trace(logPlanificador, "En cola ESI id : %d con estimacion: %.6f", hola->id, hola->estimacionSiguiente);
			queue_push(colaListos, hola);

		}

	}

}

bool ordenarESIS(void* nodo1, void* nodo2) {
	ESI* e1 = (ESI*) nodo1;
	ESI* e2 = (ESI*) nodo2;

	log_info(logPlanificador,
			"ESI ID :%d y estimacion : %.6f contra ESI a comparar ID : %d y estimacion : %.6f",
			e1->id, e1->estimacionSiguiente, e2->id, e2->estimacionSiguiente);

	if (e1->estimacionSiguiente <= e2->estimacionSiguiente) {

		return true;

	} else return false;



}
