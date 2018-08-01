#include "planificador.h"

bool planificacionSJFTerminada;

void planificacionSJF(bool desalojo) {

	pthread_create(&hiloEscuchaConsola, NULL, (void *) lanzarConsola, NULL);

	pthread_create(&hiloEscuchaESI, NULL, (void *) escucharNuevosESIS, NULL);

	while (1) {

		while (queue_size(colaListos) == 0) {

			if (queue_size(colaListos) > 0) {
				break;
			}
		}

		while (pausearPlanificacion) {
		};

		log_info(logPlanificador, "Comienza planificacion SJF");

		bool finalizar = false;

		bool bloquear = false;

		bool permiso = true;

		bool desalojar = false;

		while (queue_size(colaListos) == 0) {

			if (queue_size(colaListos) > 0) {
				break;
			}
		}
		ESI * nuevo = queue_pop(colaListos);

		claveActual = nuevo->id;

		log_info(logPlanificador, "clave actual ahora es : %d", nuevo->id);

		uint32_t operacion;
		uint32_t tamanioRecurso;
		char * recursoPedido;

		while (!finalizar && !bloquear && permiso && !desalojar && !matarESI) {

			while (pausearPlanificacion) {
			} // ciclo hermoso que pausea la planificacion

			pthread_mutex_lock(&mutexComunicacion);

			if (nuevo->bloqueadoPorClave && !nuevo->bloqueadoPorConsola ) {
				log_info(logPlanificador,
						"entra un ESI recien desbloqueado de la clave");
				log_debug(logPlanificador, "Recurso pedido: %s",
						nuevo->recursoPedido);
				permiso = true;
			} else {

				uint32_t continuacion;
				log_info(logPlanificador, "empieza comunicacion");
				send(nuevo->id, &CONTINUAR, sizeof(uint32_t), 0);
				int respuesta0 =recv(nuevo->id, &continuacion,sizeof(uint32_t),0);

				if(respuesta0 < 0){

					log_error(logPlanificador, "Conexion con ESI rota");
					liberarRecursos(nuevo);
					list_add(listaFinalizados, nuevo);

				}
				if(continuacion == 0){
					log_error(logPlanificador, " El ESI de ID : %d se aborto por un motivo desconocido", nuevo->id);
					liberarRecursos(nuevo);
					list_add(listaFinalizados, nuevo);
					break;
				}
				int respuesta1 = recv(socketCoordinador, &operacion,
						sizeof(operacion), 0);
				int respuesta2 = recv(socketCoordinador, &tamanioRecurso,
						sizeof(uint32_t), 0);
				recursoPedido = malloc(sizeof(char) * tamanioRecurso);
				int respuesta3 = recv(socketCoordinador, recursoPedido,
						tamanioRecurso, 0);

				log_info(logPlanificador,
						"recibo los datos suficientes para corroborar ejecucion");

				if (respuesta1 <= 0 || respuesta2 <= 0 || respuesta3 <= 0) {
					log_info(logPlanificador,
							"conexion con el coordinador rota");
					liberarGlobales();
					exit(-1);
				} else {
					log_info(logPlanificador,
							"las respuestas llegaron satisfactoriamente");
					nuevo->recursoPedido = string_new();
					string_append(&(nuevo->recursoPedido), recursoPedido);
					nuevo->proximaOperacion = operacion;

				}

				log_debug(logPlanificador, "Recurso pedido: %s",
						nuevo->recursoPedido);

				permiso = validarPedido(nuevo->recursoPedido, nuevo);
			}

			if (permiso) {

				nuevo->rafagaAnterior = 0;

				log_info(logPlanificador, "el esi tiene permiso de ejecucion");

				if (nuevo->proximaOperacion == 2) {

					char * valorRecurso;
					uint32_t tamValor;

					log_debug(logPlanificador, "Recurso pedido: %s",
							nuevo->recursoPedido);

					log_info(logPlanificador,
							"empieza comunicacion con coordinador para recibir valor (Op: SET)");

					int resp = recv(socketCoordinador, &tamValor,
							sizeof(uint32_t), 0);
					valorRecurso = malloc(sizeof(char) * tamValor);
					int resp2 = recv(socketCoordinador, valorRecurso, tamValor,
							0);

					if (resp <= 0 || resp2 <= 0) {

						log_info(logPlanificador,
								"conexion con el coordinador rota");
						liberarGlobales();
						exit(-1);

					} else {

						log_info(logPlanificador, "llega el valor ");
						cargarValor(recursoPedido, valorRecurso);
						log_info(logPlanificador, "valor cargado");

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

					log_debug(logPlanificador, "Recurso pedido: %s",
							nuevo->recursoPedido);

					list_add(nuevo->recursosAsignado, nuevo->recursoPedido);

					log_info(logPlanificador,
							"El recurso se añadio a los recursos asignados del esi");

				}

				for (int i = 0; i < list_size(nuevo->recursosAsignado); i++) {
					log_debug(logPlanificador, "%s",
							list_get(nuevo->recursosAsignado, i));
				}

				bloquearRecurso(nuevo->recursoPedido);

				log_info(logPlanificador, "clave bloqueada");

				nuevo->recienLlegado = false;
				nuevo->recienDesbloqueadoPorRecurso = false;

				log_info(logPlanificador,
						" ESI de clave %d entra al planificador", nuevo->id);

				pthread_mutex_lock(&mutexComunicacion);

				if (nuevo->bloqueadoPorClave && !nuevo->bloqueadoPorConsola) {
					log_info(logPlanificador,
							" el ESI sabe que estaba bloqueado, le aviso que puede seguir ");
					send(nuevo->id, &CONTINUAR, sizeof(uint32_t), 0);
					nuevo->bloqueadoPorClave = false;
					log_info(logPlanificador,
							" El esi se entero de que puede continuar");

				} else {
					nuevo->bloqueadoPorClave = false;
					nuevo->bloqueadoPorConsola = false;
					log_info(logPlanificador,
							" le aviso al coordinador de que el ESI tiene permiso");
					send(socketCoordinador, &CONTINUAR, sizeof(uint32_t), 0);
				}

				log_info(logPlanificador, " ejecuta una sentencia ");

				nuevo->rafagasRealizadas = nuevo->rafagasRealizadas + 1;

				log_info(logPlanificador,
						"rafagas realizadas del esi %d son %d", nuevo->id,
						nuevo->rafagasRealizadas);

				uint32_t respuesta;
				int resp = recv(nuevo->id, &respuesta, sizeof(uint32_t), 0);

				log_info(logPlanificador,
						" recibo respuesta del ESI %d, resp: %d", nuevo->id,
						respuesta);
				pthread_mutex_unlock(&mutexComunicacion);

				if (resp <= 0) {

					log_info(logPlanificador, " conexion rota con el ESI. Se lo finaliza ");
					liberarRecursos(nuevo);
					list_add(listaFinalizados, nuevo);
					break;
				}

				if(nuevo->proximaOperacion == 3) { //si realizo un STORE, se libera el recurso que estaba usando

					log_debug(logPlanificador, " el ESI termino un STORE");
					liberarUnRecurso(nuevo);

				}
				if (respuesta != CONTINUAR) {
					log_info(logPlanificador, " el ESI quiere finalizar ");

					finalizar = true;

				} else if (desalojo && queue_size(colaListos) > 0) // si hay desalojo activo
						{
					log_info(logPlanificador, " hay desalojo ");

					pthread_mutex_lock(&mutexColaListos); // por las dudas que entre otro ESI justo en este momento (no lo tengo en cuenta)

					ESI* auxiliar = queue_peek(colaListos);

					if (auxiliar->recienLlegado
							|| auxiliar->recienDesbloqueadoPorRecurso) //chequeo si el proximo en cola es un recien llegado
							{
						if (auxiliar->estimacionSiguiente
								< (nuevo->estimacionSiguiente
										- nuevo->rafagasRealizadas)) // y si su estimacion siguiente es menor que la del que esta en ejecucion menos lo que ya hizo
								{
							log_info(logPlanificador, "se desaloja el ESI ");

							desalojar = true; //se va a desalojar
						} else {
							log_info(logPlanificador,
									"el esi sigue ejecutando");

							limpiarRecienLlegados(); // Si no es menor la estimacion, los recien llegados ya no tendrian validez, los actualizo
						}
					}
					pthread_mutex_unlock(&mutexColaListos);

				}

				if (nuevo->id == claveParaBloquearESI) {
					log_info(logPlanificador, "se pide bloqueo del esi");

					bloquear = true;
				}

			} else {

				if (nuevo->proximaOperacion == 1) { //Caso GET

					log_info(logPlanificador,
							" El esi no tiene permiso de ejecucion y se bloquea ");
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

					log_info(logPlanificador,
							" El esi no tiene permiso de ejecucion y se aborta (quiso hacer SET o STORE de recurso no tomado) ");
					claveActual = -1;
					liberarRecursos(nuevo);
					list_add(listaFinalizados, nuevo);
					uint32_t aviso = -2;
					send(socketCoordinador, &aviso, sizeof(aviso), 0);
					pthread_mutex_unlock(&mutexComunicacion);
				}

			}

		}

		log_info(logPlanificador, "fin de ciclo");

		if (matarESI) {

			log_info(logPlanificador,
					"ESI de clave %d fue mandado a matar por consola",
					nuevo->id);
			claveActual = -1;
			pthread_mutex_lock(&mutexComunicacion);
			log_info(logPlanificador, "le aviso al esi");

			uint32_t abortar = -2;
			send(nuevo->id, &abortar, sizeof(uint32_t), 0);
			pthread_mutex_unlock(&mutexComunicacion);
			log_info(logPlanificador, "comunicacion terminada");

			log_info(logPlanificador,
					"se procede a liberar sus recursos y se añade a finalizados");
			liberarRecursos(nuevo);
			list_add(listaFinalizados, nuevo);
			log_info(logPlanificador, "ESI terminado");

			pthread_mutex_lock(&mutexAsesino);

			matarESI = false;

			pthread_mutex_unlock(&mutexAsesino);

		} else if (finalizar) { //aca con el mensaje del ESI, determino si se bloquea o se finaliza

			claveActual = -1;
			list_add(listaFinalizados, nuevo);
			log_info(logPlanificador, " ESI de clave %d en finalizados!",
					nuevo->id);
			liberarRecursos(nuevo);
			if (bloquearESIActual)
				bloquearESIActual = false;

		} else if (bloquear) { // este caso sería para bloqueados por usuario. No se libera clave acá
			claveActual = -1;
			log_info(logPlanificador, "bloqueando esi..");
			nuevo->bloqueadoPorConsola = true;
			int rafagas = nuevo->rafagasRealizadas;
			int estimacion = nuevo->estimacionSiguiente;
			nuevo->estimacionAnterior = estimacion;
			nuevo->rafagaAnterior = rafagas;
			nuevo->rafagasRealizadas = 0;
			bloquearRecurso(claveParaBloquearRecurso);
			bloquearESI(claveParaBloquearRecurso, nuevo);
			bloquearESIActual = false;
			log_info(logPlanificador,
					" ESI de clave %d en bloqueados para recurso %s", nuevo->id,
					claveParaBloquearESI);

		} else if (desalojar) {

			claveActual = -1;
			int rafagas = nuevo->rafagasRealizadas;
			int estimacion = nuevo->estimacionSiguiente - rafagas;
			nuevo->estimacionSiguiente = estimacion;
			nuevo->rafagaAnterior = rafagas;
			nuevo->rafagasRealizadas = 0;
			nuevo->recienDesalojado = true;
			armarColaListos(nuevo); //aca no meto mutex porque si llega otro ESI que esté primero que el que genero el desalojo, lo desalojaria igual.
			log_info(logPlanificador, " ESI de clave %d desalojado", nuevo->id);

		}

	}

}

void armarColaListos(ESI * esi) {

	log_info(logPlanificador, " ESI de ID %d quiere entrar en cola de listos",
			esi->id);

	estimarProximaRafaga(esi);

	log_info(logPlanificador, "Tiene estimación proxima rafaga: %.6f",
			esi->estimacionSiguiente);

	if (queue_size(colaListos) == 0) {
		log_info(logPlanificador, "la cola estaba vacía! Entra directo");
		queue_push(colaListos, esi);

	} else if (queue_size(colaListos) > 0) {

		log_info(logPlanificador, "La cola tiene tamaño de %d",
				queue_size(colaListos));
		queue_push(colaListos, esi);

		t_list * auxiliar = list_create();

		while (!queue_is_empty(colaListos)) {
			ESI * esi1 = queue_pop(colaListos);
			list_add(auxiliar, esi1);

		}

		list_sort(auxiliar, ordenarESIS);

		int i = 0;
		while (!list_is_empty(auxiliar)) {

			ESI * hola = list_remove(auxiliar, i);
			log_info(logPlanificador, "meto en cola ESI id : %d", hola->id);
			queue_push(colaListos, hola);

		}

	}

	log_info(logPlanificador, "cola armada ");

}

bool ordenarESIS(void* nodo1, void* nodo2) {
	ESI* e1 = (ESI*) nodo1;
	ESI* e2 = (ESI*) nodo2;

	log_info(logPlanificador,
			"ESI ID :%d y estimacion : %.6f contra ESI a comparar id : %d y estimacion : %.6f",
			e1->id, e1->estimacionSiguiente, e2->id, e2->estimacionSiguiente);

	if (e1->estimacionSiguiente > e2->estimacionSiguiente) {

		return false;

	} else if (e1->estimacionSiguiente == e2->estimacionSiguiente) //Ante empate de estimaciones

			{
		log_info(logPlanificador, "hay empate de estimaciones");

		if (!e2->recienDesalojado && e1->recienDesalojado) { //si no es recien llegado, tiene prioridad porque ya estaba en disco

			return false;

		} else if (!e2->recienDesalojado && !e1->recienDesalojado
				&& e2->recienDesbloqueadoPorRecurso
				&& !e1->recienDesbloqueadoPorRecurso) { // si se da que ninguno de los dos recien fue creado, me fijo si alguno se desbloqueo recien de un recurso

			return false;

		} else if (e2->recienDesalojado && e1->recienDesalojado
				&& e2->recienDesbloqueadoPorRecurso
				&& !e1->recienDesbloqueadoPorRecurso) { //si los dos recien llegan, me fijo si el auxiliar recien llego de desbloquearse

			return false;

		} else {

			return true;
		}

	} else {

		return true;
	}

}
