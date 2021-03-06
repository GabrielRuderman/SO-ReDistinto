/*
 ============================================================================
 Name        : instancia.c
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
#include "instancia.h"
#include "../../biblioteca-El-Rejunte/src/misSockets.h"


typedef enum {
	CIRC,
	LRU,
	BSU
} t_reemplazo;

t_log* logger;
t_config* config;
bool error_config;
char* ip_coordinador;
char* port_coordinador;
char* algoritmo_reemplazo;
char* nombre_instancia;
uint32_t id_instancia;
char* montaje;
t_reemplazo protocolo_reemplazo;
int socketCoordinador, intervalo_dump, fd;
uint32_t cant_entradas, tam_entrada, entradas_libres;
t_list* tabla_entradas;
t_list* tabla_entradas_atomicas;
t_list* lista_claves;
char* bloque_instancia;
int puntero_circular;
int pos_a_pisar;
t_entrada* entrada_a_reemplazar;
int mayorTamano;
int mayorReferencia;
t_list* reemplazos_recientes;
t_instruccion* instruccion; // es la instruccion actual
int referencia_actual = 0;
bool ejecutarDumpAutomatico;
pthread_t hiloTemporizador;
pthread_mutex_t mutexDumpeo = PTHREAD_MUTEX_INITIALIZER;

const uint32_t PAQUETE_OK = 1;
const uint32_t PAQUETE_ERROR = -1;
const uint32_t CHEQUEO_INSTANCIA_ACTIVA = 0;
const uint32_t PEDIDO_COMPACTACION = -5;

void imprimirTablaDeEntradas(t_list* tabla) {
	printf("\n_______TABLA DE ENTRADAS_______\n");
	for (int i = 0; i < list_size(tabla); i++) {
		t_entrada* entrada = list_get(tabla, i);
		printf("%s - %d (%d) - %d\n", entrada->clave, entrada->entrada_asociada, entrada->size_valor_almacenado, entrada->ultima_referencia);
	}
	printf("CANTIDAD DE CLAVES: %d\n", list_size(tabla));
}

int solicitarCompactacion() {
	log_info(logger, "Se solicita al Coordinador permiso para compactar");
	send(socketCoordinador, &PEDIDO_COMPACTACION, sizeof(uint32_t), 0);
	uint32_t respuesta;
	if (recv(socketCoordinador, &respuesta, sizeof(uint32_t), 0) < 1) {
		log_error(logger, "Error de Comunicacion: se ha roto la conexion con el Coordinador, me aborto");
		finalizar(EXIT_FAILURE);
	}
	if (respuesta == PAQUETE_OK) {
		compactarAlmacenamiento();
		return 1;
	} else {
		log_error(logger, "Permiso denegado");
		return -1;
	}
}

int obtenerEntradasAOcupar(char* valor) {
	div_t division = div(strlen(valor), tam_entrada);
	int entradas_a_ocupar = division.quot;
	if (division.rem > 0) entradas_a_ocupar++;
	return entradas_a_ocupar;
}

int operacion_STORE(char* clave) {
	// Funcion magica para comparar si esta la clave que quiero en la tabla de entradas
	bool comparadorClaveActual(void* nodo) {
		t_entrada* entrada = (t_entrada*) nodo;
		return (strcmp(clave, entrada->clave) == 0);
	}

	// Busco la clave en la tabla usando la funcion magica
	t_entrada* entrada = (t_entrada*) list_find(tabla_entradas, comparadorClaveActual);

	if (dumpearClave(entrada) > 0) {
		log_info(logger, "Se persistió la clave-valor");
		return 1;
	} else {
		log_error(logger, "No se persistió la clave-valor");
		return -1;
	}

	/*char* _nombreArchivo;
	char* _valor;
	int _archivoClave;

	_valor = malloc(sizeof(char) * tam_entrada);
	strncpy(_valor, bloque_instancia+entrada->entrada_asociada, entrada->size_valor_almacenado);
	//_valor[entrada->size_valor_almacenado] = '\0';
	_nombreArchivo = string_new();
	string_append(&_nombreArchivo, ruta_directorio);
	string_append(&_nombreArchivo, entrada->clave);
	string_append(&_nombreArchivo, ".txt");

	//MANEJO DE ERRORES!
	if ((_archivoClave = open(_nombreArchivo, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) > 0) {
		if ((int)write(_archivoClave, _valor, string_length(_valor)) > 0) {
			//SE ESCRIBIÓ PERSISTIÓ LA CLAVE

			log_info(logger, "Se persistió la clave-valor");
			close(_archivoClave);
			return 1;
		} else{
			//MANEJO DE ERRORES!
			log_error(logger, "No se persistió la clave-valor");
			perror("Error:");
			close(_archivoClave);
			return -1;
		}
	} else {
		log_error(logger, "No se pudo abrir el archivo");
		perror("Error:");
		return -1;
	}*/
}

int operacion_SET_reemplazo(t_entrada* entrada, char* valor) {
	int entradas_a_ocupar = obtenerEntradasAOcupar(valor);

	//Verificamos el tamaño del nuevo valor.
	if (entradas_a_ocupar <= entrada->entradas_ocupadas) {
		//Ocupa lo mismo que el valor anterior.
		liberarEntrada(entrada);
		escribirEntrada(entrada, valor);
		entrada->entradas_ocupadas = entradas_a_ocupar;
		entrada->size_valor_almacenado = strlen(valor);
		entrada->ultima_referencia = referencia_actual;
		actualizarCantidadEntradasLibres();
		log_info(logger, "Se reemplazo el valor de la clave %s", entrada->clave);
		return 1;
	} else {
		log_error(logger, "El valor a registrar supera la cantidad actual de entradas ocupadas por la clave");
		return -1;
	}
		 /*}
		//Ocupa más que el valor anterior.
		int entradas_extra = entradas_a_ocupar - entradas_ocupadas;
		//Libero el valor a reemplazar
		if(entradas_extra <= entradas_libres){
			//Entonces, pregunto si hay entradas libres y contigüas.
			if(1){
				//Escribo
			} else {
				//Compacto el almacenamiento
				//Escribo nuevo valor
			}
		} else {
			//Devuelve la entrada a reemplazar
			t_entrada* entrada = algoritmoDeReemplazo(entradas_a_ocupar);
			//Libero la entrada a reemplazar
			//Entonces, pregunto si hay entradas libres y contigüas.
			if(1){
				//Escribo
			} else {
				//Compacto el almacenamiento
				//Escribo nuevo valor
			}
		}*/

	//Actualizamos el diccionario con el nuevo valor para la clave.
}

t_entrada* algoritmoBSU(t_list* tabla_entradas_atomicas) {
	/*
	 * Por último, el algoritmo BSU lleva registro del tamaño dentro de la entrada atomica que está siendo ocupado,
	 * y en el momento de un reemplazo, escoge aquél que ocupa más espacio dentro de una entrada.
	 */

	bool mayorTamanoAlmacenado(void* nodo1, void* nodo2) {
		t_entrada* entrada1 = (t_entrada*) nodo1;
		t_entrada* entrada2 = (t_entrada*) nodo2;
		log_trace(logger, "comparacion valores: %d > %d", entrada1->size_valor_almacenado, entrada2->size_valor_almacenado);
		return entrada1->size_valor_almacenado > entrada2->size_valor_almacenado;
	}

	bool igualAMayorTamanoAlmacenado(void* nodo) {
		t_entrada* entrada = (t_entrada*) nodo;
		return entrada->size_valor_almacenado == mayorTamano;
	}

	list_sort(tabla_entradas_atomicas, mayorTamanoAlmacenado); // TODO: CHEQUEAR SI SON TODOS NO ATOMICAS

	// Busco si hay empate
	t_entrada* entrada = list_get(tabla_entradas_atomicas, 0);
	mayorTamano = entrada->size_valor_almacenado;
	t_list* tabla_entradas_empatadas = list_filter(tabla_entradas_atomicas, igualAMayorTamanoAlmacenado);
	if (list_size(tabla_entradas_empatadas) > 1) {
		log_warning(logger, "Hubo empate, utilizo el algoritmo Circular");
		entrada = algoritmoCircular(tabla_entradas_empatadas);
	}

	return entrada;
}

t_entrada* algoritmoLRU(t_list* tabla_entradas_atomicas) {
	/*
	 * El algoritmo LRU se basa en llevar registro de hace cuánto fue referenciada cada entrada.
	 * Llegado el momento de reemplazar una entrada, se selecciona aquella entrada que ha sido referenciada hace mayor tiempo.
	 */

	bool masTiempoReferenciada(void* nodo1, void* nodo2) {
		t_entrada* entrada1 = (t_entrada*) nodo1;
		t_entrada* entrada2 = (t_entrada*) nodo2;
		return entrada1->ultima_referencia < entrada2->ultima_referencia;
	}

	bool igualAMayorReferencia(void* nodo) {
		t_entrada* entrada = (t_entrada*) nodo;
		return entrada->ultima_referencia == mayorReferencia;
	}

	list_sort(tabla_entradas_atomicas, masTiempoReferenciada);

	// Busco si hay empate
	t_entrada* entrada = list_get(tabla_entradas_atomicas, 0);
	mayorReferencia = entrada->ultima_referencia;
	t_list* tabla_entradas_empatadas = list_filter(tabla_entradas_atomicas, igualAMayorReferencia);
	if (list_size(tabla_entradas_empatadas) > 1) {
		log_warning(logger, "Hubo empate, utilizo el algoritmo Circular");
		entrada = algoritmoCircular(tabla_entradas_empatadas);
	}

	return entrada;
}

bool buscadorEntradaConPuntero(void* nodo) {
	t_entrada* entrada = (t_entrada*) nodo;
	return (puntero_circular == entrada->entrada_asociada);
}

t_entrada* algoritmoCircular(t_list* tabla_entradas_atomicas) {
	t_entrada* entrada_apuntada = NULL;

	while (!entrada_apuntada) {
		entrada_apuntada = list_find(tabla_entradas_atomicas, buscadorEntradaConPuntero);
		puntero_circular++;
		if (puntero_circular > cant_entradas) puntero_circular = 1;
	}
	return entrada_apuntada;
}

void quitarEntrada(t_entrada* entrada) {
	bool comparadorClaveReemplazo(void* nodo) {
		t_entrada* entrada = (t_entrada*) nodo;
		return (strcmp(entrada->clave, entrada_a_reemplazar->clave) == 0);
	}

	list_remove_and_destroy_by_condition(tabla_entradas, comparadorClaveReemplazo, destruirEntrada);
}

bool valorEsAtomico(void* nodo) {
	t_entrada* entrada = (t_entrada*) nodo;
	return entrada->entradas_ocupadas == 1;
}

void algoritmoDeReemplazo() {
	tabla_entradas_atomicas = list_filter(tabla_entradas, valorEsAtomico); // Solo evaluo los valores atomicos

	if (list_size(tabla_entradas_atomicas) == 0) {
		list_destroy(tabla_entradas_atomicas);
		entrada_a_reemplazar = NULL;
		return;
	}

	switch (protocolo_reemplazo) {
	case LRU:
		entrada_a_reemplazar = algoritmoLRU(tabla_entradas_atomicas);
		break;

	case BSU:
		entrada_a_reemplazar = algoritmoBSU(tabla_entradas_atomicas);
		log_trace(logger, "entrada a reemplazar: %s", entrada_a_reemplazar->clave);
		break;

	default: // Circular
		entrada_a_reemplazar = algoritmoCircular(tabla_entradas_atomicas);
	}

	if (!entrada_a_reemplazar) return; // No hay valores atomicos para reemplazar
	liberarEntrada(entrada_a_reemplazar);
	actualizarCantidadEntradasLibres();
	log_warning(logger, "Se ha liberado la entrada %d de clave %s", entrada_a_reemplazar->entrada_asociada, entrada_a_reemplazar->clave);
	list_destroy(tabla_entradas_atomicas);
}

int operacion_SET(t_instruccion* instruccion) {
	int entradas_a_ocupar = obtenerEntradasAOcupar(instruccion->valor);

	log_info(logger, "El valor a almacenar requiere %i entradas", entradas_a_ocupar);

	if (entradas_a_ocupar <= entradas_libres) {
		// Entonces, pregunto si hay entradas libres y contiguas.
		log_info(logger, "Hay %d entradas libres para almacenar el valor", entradas_a_ocupar);
		if (hayEntradasContiguas(entradas_a_ocupar) >= 0) {
			log_info(logger, "Hay %d entradas contiguas", entradas_a_ocupar);
		} else {
			log_info(logger, "No hay %d entradas contiguas para almacenar el valor", entradas_a_ocupar);
			if (solicitarCompactacion() < 0) return -1;
		}
	} else {
		// Devuelve la entrada a reemplazar
		log_info(logger, "No hay %d entradas para almacenar el valor, aplico algoritmo de reemplazo %s", entradas_a_ocupar, algoritmo_reemplazo);

		while (entradas_a_ocupar > entradas_libres) {
			algoritmoDeReemplazo();
			if (!entrada_a_reemplazar) {
				log_error(logger, "No hay ningun valor atomico para reemplazar");
				return -1;
			}
			char* clave_reemplazada = string_new();
			string_append(&clave_reemplazada, entrada_a_reemplazar->clave);
			list_add(reemplazos_recientes, clave_reemplazada);
			quitarEntrada(entrada_a_reemplazar);
		}
		log_info(logger, "Hay %d entradas libres para almacenar el valor", entradas_a_ocupar);
		// Entonces, pregunto si hay contiguas.
		if (hayEntradasContiguas(entradas_a_ocupar) >= 0) {
			log_info(logger, "Hay %d entradas contiguas", entradas_a_ocupar);
		} else {
			log_info(logger, "No hay %d entradas contiguas para almacenar el valor", entradas_a_ocupar);
			if (solicitarCompactacion() < 0) return -1;
		}
	}

	t_entrada* entrada = (t_entrada*) malloc(sizeof(t_entrada));
	entrada->clave = string_new();
	string_append(&(entrada->clave), instruccion->clave);
	entrada->size_valor_almacenado = strlen(instruccion->valor);
	entrada->entradas_ocupadas = entradas_a_ocupar;
	entrada->ultima_referencia = referencia_actual;
	entrada->entrada_asociada = hayEntradasContiguas(entradas_a_ocupar);

	escribirEntrada(entrada, instruccion->valor);
	actualizarCantidadEntradasLibres();

	list_add(tabla_entradas, entrada);
	log_info(logger, "Se ha escrito la entrada");

	return 1;
}

int validarArgumentosInstruccion(t_instruccion* instruccion) {
	log_info(logger, "Validando que la instruccion sea ejecutable...");

	printf("La instruccion recibida es: ");
	switch (instruccion->operacion) {
	case opGET:
		printf("GET %s\n", instruccion->clave);
		log_error(logger, "Una Instancia no puede ejecutar GET");
		send(socketCoordinador, &PAQUETE_ERROR, sizeof(uint32_t), 0);
		return -1;
		break;

	case opSET:
		printf("SET %s %s\n", instruccion->clave, instruccion->valor);
		break;

	case opSTORE:
		printf("STORE %s\n", instruccion->clave);
		break;

	default:
		log_error(logger, "No comprendo la instruccion, le informo al Coordinador que no se puede ejecutar");
		send(socketCoordinador, &PAQUETE_ERROR, sizeof(uint32_t), 0);
		return -1;
	}

	return 1;
}

void actualizarEntradaAsociada(void* nodo) {
	t_entrada* entrada = (t_entrada*) nodo;
	if ((entrada->entrada_asociada * tam_entrada) > pos_a_pisar) {
		entrada->entrada_asociada--;
	}
}

void compactarAlmacenamiento() {
	log_warning(logger, "Se inicia la compactacion del almacenamiento");

	for (int x = 0; x < cant_entradas * tam_entrada; x += tam_entrada) {
		if (bloque_instancia[x] == '0') {
			for (int y = x + tam_entrada; x < cant_entradas * tam_entrada; y += tam_entrada) {
				if (bloque_instancia[y] != '0') {
					char* porcion = string_new();
					for (int z = y; z < cant_entradas * tam_entrada; z++) {
						string_append_with_format(&porcion, "%c", bloque_instancia[z]);
					}
					while ((x + strlen(porcion)) < strlen(bloque_instancia)) {
						string_append_with_format(&porcion, "%c", '0'); // Se agregan los '0' al final ya que strncpy no lo hace
					}
					strncpy(bloque_instancia + x, porcion, strlen(porcion)); // Pisa la entrada vacia con el bloque posterior
					pos_a_pisar = x; // Posicion respecto a la que se deben actualizar las entradas asociadas

					list_iterate(tabla_entradas, actualizarEntradaAsociada); // Se actualizan las entradas asociadas si corresponde

					break;
				}
			}
		}
	}
	log_info(logger, "Se compactaron todas las entradas");
	log_debug(logger, "%s", bloque_instancia);
}

int dumpearClave(t_entrada* entrada) {
	char* _nombreArchivo = string_new();
	string_append(&_nombreArchivo, montaje);
	string_append(&_nombreArchivo, entrada->clave);
	string_append(&_nombreArchivo, ".txt");

	int _fd = open(_nombreArchivo, O_RDWR, S_IRUSR | S_IWUSR);
	if (_fd < 0) { // Si el archivo no existia => lo crea, y crea el mapa
		_fd = open(_nombreArchivo, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
		if (_fd < 0) {
			log_error(logger, "El archivo %s no se puede crear", _nombreArchivo);
			perror("Error");
			free(_nombreArchivo);
			return -1;
		}
	}
	ftruncate(_fd, entrada->size_valor_almacenado);
	entrada->fd = _fd;
	entrada->mapa_archivo = mmap(NULL, entrada->size_valor_almacenado, PROT_READ | PROT_WRITE, MAP_SHARED, entrada->fd, 0);

	int pos_inicial = (entrada->entrada_asociada - 1) * tam_entrada;
	for (int i = pos_inicial; i < pos_inicial + entrada->size_valor_almacenado; i++) {
		entrada->mapa_archivo[i - pos_inicial] = bloque_instancia[i];
	}

	free(_nombreArchivo);
	munmap(entrada->mapa_archivo, entrada->size_valor_almacenado);
	close(_fd);
	return 1;

	/*if (_fd < 0) {
		log_error(logger, "Error al abrir archivo para DUMP");
		return -1;
	} else {
		if(sb.st_size > 0) {
			printf("hay algo en el archivo...\n");
			if (entrada->size_valor_almacenado <= strlen(entrada->mapa_archivo)) {
				printf("tamaño nuevo valor es menor o igual que el almacenado\n");
				memset(entrada->mapa_archivo, '0', strlen(entrada->mapa_archivo));
				strncpy(entrada->mapa_archivo, bloque_instancia + ((entrada->entrada_asociada - 1) * tam_entrada), strlen(entrada->mapa_archivo));
			} else {
				printf("tamaño nuevo valor es mayor que el almacenado\n");
				ftruncate(entrada->fd, entrada->size_valor_almacenado);
				munmap(entrada->mapa_archivo, strlen(entrada->mapa_archivo));
				entrada->mapa_archivo = string_new();
				entrada->mapa_archivo = mmap(NULL, entrada->size_valor_almacenado, PROT_READ | PROT_WRITE, MAP_SHARED, entrada->fd, 0);
				//entrada->mapa_archivo = mremap(NULL, strlen(entrada->mapa_archivo), entrada->size_valor_almacenado, 0);
			}
		} else {
			puts("el archivo no existe, hay que crearlo");
			entrada->fd = _fd;
			entrada->mapa_archivo = mmap(NULL, entrada->size_valor_almacenado, PROT_READ | PROT_WRITE, MAP_SHARED, entrada->fd, 0);
			entrada->mapa_archivo = string_substring(bloque_instancia, (entrada->entrada_asociada - 1) * tam_entrada, entrada->size_valor_almacenado);
		}
		close(_fd);
	}*/
}

void dumpAutomatico() {
	void dumpeoConReturnVoid(void* nodo) { // Para que no tire el warning pelotudo
		t_entrada* entrada = (t_entrada*) nodo;
		dumpearClave(entrada);
	}

	while (ejecutarDumpAutomatico) {
		sleep(intervalo_dump);
		pthread_mutex_lock(&mutexDumpeo);
		list_iterate(tabla_entradas, dumpeoConReturnVoid);
		pthread_mutex_unlock(&mutexDumpeo);
	}
}

void escribirEntrada(t_entrada* entrada, char* valor) {
	//strncpy(bloque_instancia + entrada->entrada_asociada, valor, entrada->entradas_ocupadas * tam_entrada);
	int pos_inicial = (entrada->entrada_asociada - 1) * tam_entrada;
	for (int i = pos_inicial; i < pos_inicial + strlen(valor); i++) {
		bloque_instancia[i] = valor[i - pos_inicial];
	}
}

t_entrada* crearEntradaDesdeArchivo(char* archivo) {
	struct stat sb;
	t_entrada* entrada = (t_entrada*) malloc(sizeof(t_entrada));

	entrada->clave = string_new();
	char** vector_clave = string_split(archivo, ".");
	string_append(&(entrada->clave), vector_clave[0]);
	int i = 0;
	while(vector_clave[i] != NULL) {
		free(vector_clave[i]);
		i++;
	}
	free(vector_clave);

	char* path = string_new();
	string_append(&path, montaje);
	string_append(&path, archivo);
	entrada->fd = open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	free(path);
	fstat(entrada->fd, &sb);

	entrada->mapa_archivo = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, entrada->fd, 0);

	entrada->size_valor_almacenado = sb.st_size;

	llenarAlmacenamiento(entrada);

	entrada->ultima_referencia = referencia_actual;

	return entrada;
}

int iniciarDirectorio() {
	tabla_entradas = list_create();

	DIR* dirp = opendir(montaje);
	// TODO: Chequear los permisos del mkdir
	if (!dirp) return mkdir(montaje, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); // Si no existe creo el directorio

	closedir(dirp);
	return 1;
}

void llenarAlmacenamiento(t_entrada* entrada) {
	int entradas_a_ocupar = obtenerEntradasAOcupar(entrada->mapa_archivo);

	for (int i = 0; i < cant_entradas * tam_entrada; i += tam_entrada) {
		if (bloque_instancia[i] == '0') {
			strncpy(bloque_instancia + i, entrada->mapa_archivo, strlen(entrada->mapa_archivo));
			entrada->entrada_asociada = (i / tam_entrada) + 1;
			entrada->entradas_ocupadas = entradas_a_ocupar;
			break;
		}
	}
	bloque_instancia[cant_entradas * tam_entrada] = '\0';
}

int hayEntradasContiguas(int entradas_necesarias) {
	int contador = 0;
	int primer_entrada;

	for (int i = 0; i < tam_entrada * cant_entradas; i = i + tam_entrada) {
		if(bloque_instancia[i] == '0') {
			contador++;
			if (contador == 1) {
				primer_entrada = i;
			}
			if (contador == entradas_necesarias) {
				return (primer_entrada / tam_entrada) + 1;
			}
		} else {
			contador = 0;
			primer_entrada = 0;
		}
	}
	return -1;
}

void liberarEntrada(t_entrada* entrada) {
	//strncpy(bloque_instancia+entrada->entrada_asociada, '0', entrada->entradas_ocupadas*tam_entrada);
	memset(bloque_instancia + (entrada->entrada_asociada - 1) * tam_entrada, '0', entrada->entradas_ocupadas * tam_entrada);
}

void actualizarCantidadEntradasLibres() {
	int cont = 0;

	// Recorre el almacenamiento por cada entrada, preguntando si el primer valor en c/u es nulo.
	for (int i = 0; i < tam_entrada * cant_entradas; i = i + tam_entrada) {
		if (bloque_instancia[i] == '0') {
			cont++;
		}
	}
	entradas_libres = cont;
}

void inicializarBloqueInstancia() {
	bloque_instancia = (char*) malloc(sizeof(char) * ((cant_entradas * tam_entrada) + 1));
	memset(bloque_instancia, '0', cant_entradas * tam_entrada);
	bloque_instancia[cant_entradas * tam_entrada] = '\0';
}

int procesar(t_instruccion* instruccion) {
	log_info(logger, "Procesando instruccion...");

	// Funcion magica para comparar si esta la clave que quiero en la tabla de entradas
	bool comparadorClaveInstruccion(void* nodo) {
		t_entrada* entrada = (t_entrada*) nodo;
		return (strcmp(instruccion->clave, entrada->clave) == 0);
	}

	// Busco la clave en la tabla usando la funcion magica
	t_entrada* entrada = (t_entrada*) list_find(tabla_entradas, comparadorClaveInstruccion);

	// Evaluo como procesar segun las condiciones
	if (!entrada) { // la entrada no estaba
		log_warning(logger, "La clave no esta registrada en la Tabla de Entradas");

		switch (instruccion->operacion) {
		case opSET: // SET de clave ausente
			log_debug(logger, "Operacion SET de clave nueva");
			return operacion_SET(instruccion);
		case opSTORE: // STORE de clave ausente
			log_error(logger, "Intento de STORE para una clave que no tiene valor cargado");
			return 1;
		}
	} else { // la entrada si estaba
		log_warning(logger, "La clave esta registrada en la Tabla de Entradas");

		switch (instruccion->operacion) {
		case opSET: // SET de clave presente.
			log_debug(logger, "Operacion SET con reemplazo");
			return operacion_SET_reemplazo(entrada, instruccion->valor);
		case opSTORE: // STORE de clave presente.
			log_debug(logger, "Operacion STORE para clave existente");
			return operacion_STORE(instruccion->clave);
		}
	}
	return -1;
}

t_instruccion* recibirInstruccion(int socketCoordinador) {
	// Recibo linea de script parseada
	uint32_t tam_paquete;
	if (recv(socketCoordinador, &tam_paquete, sizeof(uint32_t), 0) < 1) {
		log_error(logger, "Error de Comunicacion: se ha roto la conexion con el Coordinador, me aborto");
		finalizar(EXIT_FAILURE);
	}

	if (tam_paquete == CHEQUEO_INSTANCIA_ACTIVA) {
		send(socketCoordinador, &CHEQUEO_INSTANCIA_ACTIVA, sizeof(uint32_t), 0);
		return NULL; // Esto lo usa el Coordinador para saber si estoy activa
	} else if (tam_paquete == PEDIDO_COMPACTACION) {
		log_info(logger, "El Coordinador me informa que debo compactar");
		compactarAlmacenamiento();
		return NULL;
	}

	char* paquete = (char*) malloc(sizeof(char) * tam_paquete);
	if (recv(socketCoordinador, paquete, tam_paquete, 0) < 1) {
		log_error(logger, "Error de Comunicacion: se ha roto la conexion con el Coordinador, me aborto");
		finalizar(EXIT_FAILURE);
	}

	log_info(logger, "Recibi un paquete que me envia el Coordinador");
	log_debug(logger, "%s", paquete);

	t_instruccion* instruccion = desempaquetarInstruccion(paquete, logger);
	destruirPaquete(paquete);

	return instruccion;
}

// Protocolo numerico de ALGORITMO_DISTRIBUCION
void establecerProtocoloReemplazo() {
	if (strcmp(algoritmo_reemplazo, "LRU") == 0) {
		protocolo_reemplazo = LRU;
	} else if (strcmp(algoritmo_reemplazo, "BSU") == 0) {
		protocolo_reemplazo = BSU;
	} else {
		protocolo_reemplazo = CIRC;
		puntero_circular = 1;
	}
}

t_control_configuracion cargarConfiguracion() {
	error_config = false;

	// Importo los datos del archivo de configuracion
	config = conectarAlArchivo(logger, "/home/utnso/workspace/tp-2018-1c-El-Rejunte/instancia/config_instancia.cfg", &error_config);

	ip_coordinador = obtenerCampoString(logger, config, "IP_COORDINADOR", &error_config);
	port_coordinador = obtenerCampoString(logger, config, "PORT_COORDINADOR", &error_config);
	nombre_instancia = obtenerCampoString(logger, config, "NOMBRE", &error_config);
	log_info(logger, "Soy %s", nombre_instancia);
	montaje = obtenerCampoString(logger, config, "MONTAJE", &error_config);
	algoritmo_reemplazo = obtenerCampoString(logger, config, "ALGORITMO_REEMPLAZO", &error_config);
	intervalo_dump = obtenerCampoInt(logger, config, "INTERVALO_DUMP", &error_config);

	establecerProtocoloReemplazo();

	// Valido si hubo errores
	if (error_config) {
		log_error(logger, "No se pudieron obtener todos los datos correspondientes");
		return CONFIGURACION_ERROR;
	}
	return CONFIGURACION_OK;
}

void destruirEntrada(void* nodo) {
	t_entrada* entrada = (t_entrada*) nodo;
	if (!entrada) return;
	if (entrada->clave != NULL) free(entrada->clave);
	if (entrada->mapa_archivo != NULL) munmap(entrada->mapa_archivo, entrada->size_valor_almacenado);
	free(entrada);
}

void finalizar(int cod) {
	if (socketCoordinador > 0) finalizarSocket(socketCoordinador);
	ejecutarDumpAutomatico = false;
	pthread_mutex_destroy(&mutexDumpeo);
	if (reemplazos_recientes != NULL) list_destroy_and_destroy_elements(reemplazos_recientes, free);
	if (tabla_entradas != NULL) list_destroy_and_destroy_elements(tabla_entradas, destruirEntrada);
	log_destroy(logger);
	free(bloque_instancia);
	finalizarConexionArchivo(config);
	exit(cod);
}

void signalHandler(int senal) {
	finalizar(EXIT_FAILURE);
}

int main() {
	signal(SIGINT, signalHandler);

	// Creo el logger
	logger = log_create("instancia.log", "Instancia", true, LOG_LEVEL_DEBUG);

	if (cargarConfiguracion() == CONFIGURACION_ERROR) {
		log_error(logger, "No se pudo cargar la configuracion");
		finalizar(EXIT_FAILURE); // Si hubo error, se corta la ejecucion.
	}

	socketCoordinador = conectarComoCliente(logger, ip_coordinador, port_coordinador);
	if (socketCoordinador < 0) {
		log_error(logger, "Error de Comunicacion: no me pude conectar con el Coordinador, me aborto");
		finalizar(EXIT_FAILURE);
	}

	uint32_t handshake = INSTANCIA;
	send(socketCoordinador, &handshake, sizeof(uint32_t), 0);

	// Le aviso cual es mi ID
	char* porcion_id = string_substring_from(nombre_instancia, strlen("Inst"));
	id_instancia = atoi(porcion_id); // Agarro lo que viene despues de "Inst"
	free(porcion_id);
	send(socketCoordinador, &id_instancia, sizeof(uint32_t), 0);

	int resp1 = recv(socketCoordinador, &cant_entradas, sizeof(uint32_t), 0);
	int resp2 = recv(socketCoordinador, &tam_entrada, sizeof(uint32_t), 0);
	if (resp1 < 1 || resp2 < 1) {
		log_error(logger, "El Coordinador no me permite conectarme");
		finalizar(EXIT_FAILURE);
	}

	log_info(logger, "Se recibio la cantidad y tamaño de las entradas correctamente");

	log_debug(logger, "Cantidad de entradas: %d\n", cant_entradas);
	log_debug(logger, "Tamaño de cada entrada: %d\n", tam_entrada);

	inicializarBloqueInstancia();

	if (iniciarDirectorio() < 0) {
		log_error(logger, "El directorio %s no es valido", montaje);
		finalizar(EXIT_FAILURE);
	}

	uint32_t cant_claves_cargadas;
	if (recv(socketCoordinador, &cant_claves_cargadas, sizeof(uint32_t), 0) < 1) {
		log_error(logger, "Error de Comunicacion: se ha roto la conexion con el Coordinador, me aborto");
		finalizar(EXIT_FAILURE);
	}
	log_info(logger, "El Coordinador me informa que tenia %d claves cargadas", cant_claves_cargadas);
	for (int i = 0; i < cant_claves_cargadas; i++) {
		uint32_t tam_clave_cargada;
		if (recv(socketCoordinador, &tam_clave_cargada, sizeof(uint32_t), 0) < 1) {
			log_error(logger, "Error de Comunicacion: se ha roto la conexion con el Coordinador, me aborto");
			finalizar(EXIT_FAILURE);
		}
		char* clave_cargada = malloc(sizeof(char) * tam_clave_cargada);
		if (recv(socketCoordinador, clave_cargada, tam_clave_cargada, 0) < 1) {
			log_error(logger, "Error de Comunicacion: se ha roto la conexion con el Coordinador, me aborto");
			finalizar(EXIT_FAILURE);;
		}
		char* archivo = string_new();
		string_append(&archivo, clave_cargada);
		free(clave_cargada);
		string_append(&archivo, ".txt");
		list_add(tabla_entradas, crearEntradaDesdeArchivo(archivo));
		free(archivo);
	}

	log_debug(logger, "BLOQUE DE MEMORIA: %s", bloque_instancia);

	//Generamos temporizador
	ejecutarDumpAutomatico = true;
	pthread_create(&hiloTemporizador, NULL, (void*) dumpAutomatico, NULL);

	actualizarCantidadEntradasLibres();

	while (1) {
		entrada_a_reemplazar = NULL;
		reemplazos_recientes = list_create();

		t_instruccion* instruccion = recibirInstruccion(socketCoordinador);
		if (instruccion != NULL) {
			if (validarArgumentosInstruccion(instruccion) > 0) {

				referencia_actual++;

				pthread_mutex_lock(&mutexDumpeo);
				int resultado = procesar(instruccion);
				pthread_mutex_unlock(&mutexDumpeo);

				if (resultado > 0) {
					log_info(logger, "Le aviso al Coordinador que se proceso la instruccion");

					if (entrada_a_reemplazar != NULL) { // Le informo al Coordinador que claves fueron reemplazada
						uint32_t cant_claves_reemplazadas = list_size(reemplazos_recientes);
						send(socketCoordinador, &cant_claves_reemplazadas, sizeof(uint32_t), 0);
						for (int i = 0; i < cant_claves_reemplazadas; i++) {
							char* clave_reemplazada = list_get(reemplazos_recientes, i);
							uint32_t tam_clave_reemplazada = strlen(clave_reemplazada) + 1;
							send(socketCoordinador, &tam_clave_reemplazada, sizeof(uint32_t), 0);
							send(socketCoordinador, clave_reemplazada, tam_clave_reemplazada, 0);
						}
					} else {
						uint32_t sin_reemplazos = 0;
						send(socketCoordinador, &sin_reemplazos, sizeof(uint32_t), 0);
					}

					send(socketCoordinador, &entradas_libres, sizeof(uint32_t), 0);
				} else {
					log_error(logger, "Le aviso al Coordinador que no se pudo procesar la instrucción");
					send(socketCoordinador, &PAQUETE_ERROR, sizeof(uint32_t), 0);
				}

				if (instruccion->operacion == 2) printf("\x1b[34m	INSTRUCCION:%d %s %s\x1b[0m\n", instruccion->operacion, instruccion->clave, instruccion->valor);
				if (instruccion->operacion == 3) printf("\x1b[34m	INSTRUCCION:%d %s\x1b[0m\n", instruccion->operacion, instruccion->clave);

				log_debug(logger, "Cantidad de entradas libres: %d", entradas_libres);
				log_warning(logger, "CANTIDAD DE CLAVES CARGADAS: %d", list_size(tabla_entradas));
				log_debug(logger, "Soy la Instancia %d", id_instancia);
				log_debug(logger, "BLOQUE DE MEMORIA: %s", bloque_instancia);
				//if (list_size(tabla_entradas) > 0) imprimirTablaDeEntradas(tabla_entradas);
			}
			destruirInstruccion(instruccion);
		}
		list_destroy_and_destroy_elements(reemplazos_recientes, free);
	}
	finalizar(EXIT_SUCCESS);
}
