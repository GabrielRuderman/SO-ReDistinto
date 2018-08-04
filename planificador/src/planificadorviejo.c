/*
 ============================================================================
 Name        : planificador.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

/*
 * Modelo ejemplo de un Cliente que envia mensajes a un Server.
 *
 * 	No se contemplan el manejo de errores en el sistema por una cuestion didactica. Tener en cuenta esto al desarrollar.
 */
/*
#include "planificadorviejo.h"

t_log* logger;
bool error_config;
char* ip; char* port;
int packagesize, backlog;
char* algoritmo;
t_queue* listos;
t_queue* bloqueados;
t_queue* terminados;
sem_t sem_bin_menu;
sem_t sem_bin_esi;
int pid_asignacion;
const uint32_t PAQUETE_OK = 1;

void estimar() {
	// Hay que implementarlo
}

void* procesarESI(void* socketESI) {
	log_info(logger, "ESI conectado");

	// Creo una estructura donde se aloja el pcb
	t_pcb* pcb = malloc(sizeof(t_pcb));
	pcb->pid = pid_asignacion;
	pcb->socket = *(int*) socketESI;
	pid_asignacion++; // Preparo el pid_asignacion para la proxima asignacion

	// Guardo al pcb del ESI en mi cola de listos
	queue_push(listos, pcb);
	return NULL;
}

void* administrarHilosESI(void* socketDeEscucha) {
	while (1) { // Infinitamente escucha a la espera de que se conecte un ESI
		int socketESI = escucharCliente(logger, *(int*) socketDeEscucha,
				backlog);
		pthread_t unHilo; // Cada conexion la delega en un hilo
		pthread_create(&unHilo, NULL, procesarESI, (void*) &socketESI);
	}
	return NULL;
}

int imprimirMenu() {
	int seleccion, clave, id, recurso;

	printf("\nSeleccione una operacion (1-7):\n");
	printf("\n   1. Pausar/Continuar");
	printf("\n   2. Bloquear <clave> <id>");
	printf("\n   3. Desbloquear <clave>");
	printf("\n   4. Listar <recurso>");
	printf("\n   5. Kill <ID>");
	printf("\n   6. Status <clave>");
	printf("\n   7. Deadlock");

	printf("\n\nSeleccion: ");
	scanf("%d", &seleccion);
	printf("\n");

	switch (seleccion) {
	case 1:
		printf("Pausar/Continuar ACTIVADO");
		printf("\nGracias, se esta procesando su solicitud...\n");
		break;

	case 2:
		printf("Bloquear ACTIVADO");
		printf("\nIngrese clave: ");
		scanf("%d", &clave);
		printf("Ingrese ID: ");
		scanf("%d", &id);
		printf("Gracias, se esta procesando su solicitud...\n");
		break;

	case 3:
		printf("Desbloquear ACTIVADO");
		printf("Ingrese clave: ");
		scanf("%d", &clave);
		printf("Gracias, se esta procesando su solicitud...\n");
		break;

	case 4:
		printf("Listar ACTIVADO");
		printf("Ingrese recurso: ");
		scanf("%d", &recurso);
		printf("Gracias, se esta procesando su solicitud...\n");
		break;

	case 5:
		printf("Kill ACTIVADO");
		printf("Ingrese ID: ");
		scanf("%d", &id);
		printf("Gracias, se esta procesando su solicitud...\n");
		break;

	case 6:
		printf("Status ACTIVADO");
		printf("\nIngrese clave: ");
		scanf("%d", &clave);
		printf("Gracias, se esta procesando su solicitud...\n");
		break;

	case 7:
		printf("Deadlock ACTIVADO");
		printf("\nGracias, se esta procesando su solicitud...\n");
		break;

	default:
		log_error(logger, "No es una opcion valida, intente nuevamente.");
		break;
	}
	return seleccion;
}

int cargarConfiguracion() {
	error_config = false;

	// Importo los datos del archivo de configuracion
	t_config* config =
			conectarAlArchivo(logger, "/home/utnso/workspace/tp-2018-1c-El-Rejunte/planificador/config_planificador.cfg", &error_config);

	ip = obtenerCampoString(logger, config, "IP", &error_config);
	port = obtenerCampoString(logger, config, "PORT", &error_config);
	algoritmo = obtenerCampoString(logger, config, "ALGORITMO_PLANIFICACION", &error_config);

	// Valido posibles errores
	if (error_config) {
		log_error(logger, "No se pudieron obtener todos los datos correspondientes");
		return -1;
	}
	return 1;
}

int main() {
	pid_asignacion = 0;
	//sem_init(sem_bin_menu, 0, 0);
	//sem_init(sem_bin_esi, 0, 1);

	// Colas para los procesos (son listas porque se actualiza el orden de ejecucion)
	listos = queue_create();
	bloqueados = queue_create();
	terminados = queue_create();

	logger = log_create("coordinador_planificador.log", "Planificador",
	true, LOG_LEVEL_INFO);

	if (cargarConfiguracion() < 0)
		return EXIT_FAILURE; // Si hubo error, se corta la ejecucion.

	// Se conecta como Servidor y espera a que el ESI se conecte
	int socketDeEscucha = conectarComoServidor(logger, ip, port, backlog);

	// Se crea un hilo que administre a los demas hilos (los que charlen con los ESI)
	pthread_t hiloAdministrador;
	pthread_create(&hiloAdministrador, NULL, administrarHilosESI, (void*) &socketDeEscucha);

	while (1) { // Va leyendo la seleccion del menu y la envia a ESI (por ahora solo entiende "1")
		uint32_t seleccion = imprimirMenu();
		t_pcb* pcb = (t_pcb*) queue_pop(listos); // Agarra el primero que haya en la cola de listos
		send(pcb->socket, &seleccion, sizeof(uint32_t), 0); // Envio al ESI lo que se eligio en consola
		uint32_t respuesta_esi;
		recv(pcb->socket, &respuesta_esi, sizeof(uint32_t), 0);
		if (respuesta_esi == PAQUETE_OK) {
			log_info(logger, "El ESI ejecuto una sentencia");
		} else {
			log_error(logger, "No se pudo ejecutar la sentencia");
		}
		queue_push(listos, pcb); // Vuelvo a encolar
	}

	finalizarSocket(socketDeEscucha);

	return EXIT_SUCCESS;
}
*/
