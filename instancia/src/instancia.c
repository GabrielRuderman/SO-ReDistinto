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
	CIRC = 0,
	LRU = 1,
	BSU = 3
} t_reemplazo;

t_log* logger;
bool error_config;
char* ip_coordinador;
char* port_coordinador;
char* algoritmo_reemplazo;
t_reemplazo protocolo_reemplazo;
uint32_t id_instancia;
int socketCoordinador, intervalo_dump, fd;
uint32_t cant_entradas, tam_entrada, entradas_libres;
t_list* tabla_entradas;
char* mapa_archivo;
char* bloque_instancia;
int puntero_circular;
t_instruccion* instruccion; // es la instruccion actual

struct stat sb;

const uint32_t PAQUETE_OK = 1;
const int32_t PAQUETE_ERROR = -1;

void imprimirBloqueEntradas(){
	for(int i = 0; i < strlen(bloque_instancia); i++) {
			printf("%c", bloque_instancia[i]);
		}
		printf("\n");
}

void imprimirTablaDeEntradas() {
	printf("\n_______TABLA DE ENTRADAS_______\n");
	for(int i = 0; i < list_size(tabla_entradas); i++) {
		t_entrada* entrada = list_get(tabla_entradas, i);
		printf("%s - %d - %d\n", entrada->clave, entrada->entrada_asociada, entrada->size_valor_almacenado);
	}
	printf("\n");
}

int operacion_STORE(char* clave) {
	char* _nombreArchivo;
	char* _valor;
	int _archivoClave;

	// Funcion magica para comparar si esta la clave que quiero en la tabla de entradas
	bool comparadorDeClaves(void* estructura) {
		t_entrada* entrada = (t_entrada*) estructura;
		return (strcmp(clave, entrada->clave) == 0);
	}

	// Busco la clave en la tabla usando la funcion magica
	t_entrada* entrada = (t_entrada*) list_find(tabla_entradas, comparadorDeClaves);

	_valor = malloc(sizeof(char) * tam_entrada);
	strncpy(_valor, bloque_instancia+entrada->entrada_asociada, entrada->size_valor_almacenado);
	_valor[entrada->size_valor_almacenado] = '\0';
	_nombreArchivo = clave;

	//MANEJO DE ERRORES!
	if((_archivoClave = open(_nombreArchivo, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) > 0){
		if ((int)write(_archivoClave, _valor, string_length(_valor)) > 0) {
			//SE ESCRIBIÓ PERSISTIÓ LA CLAVE
			actualizarMapaMemoria();
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
	}
}

int operacion_SET_reemplazo(t_entrada* entrada, char* valor) {
	int entradas_a_ocupar = (int) ceilf((float) strlen(valor) / (float) tam_entrada);

	//Verificamos el tamaño del nuevo valor.
	if(entradas_a_ocupar <= entrada->entradas_ocupadas){
		//Ocupa lo mismo que el valor anterior.
		escribirEntrada(entrada);
		strcpy(entrada->valor, valor);
		entrada->entradas_ocupadas = entradas_a_ocupar;
		entrada->size_valor_almacenado = strlen(valor);
		entrada->ultima_referencia++;
		actualizarCantidadEntradasLibres();
		return 1;
		log_info(logger, "Se reemplazo el valor de la clave %s", entrada->clave);
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

t_entrada* algoritmoBSU() {
	bool mayorValorAlmacenado(void* nodo1, void* nodo2) {
		t_entrada* entrada1 = (t_entrada*) nodo1;
		t_entrada* entrada2 = (t_entrada*) nodo2;
		return entrada1->size_valor_almacenado > entrada2->size_valor_almacenado;
	}

	list_sort(tabla_entradas, mayorValorAlmacenado);
	return list_get(tabla_entradas, 0);
}

void incrementarUltimasReferencias(void* nodo) {
	t_entrada* entrada = (t_entrada*) nodo;
	entrada->ultima_referencia++;
}

t_entrada* algoritmoLRU() {
	bool masTiempoReferenciada(void* nodo1, void* nodo2) {
		t_entrada* entrada1 = (t_entrada*) nodo1;
		t_entrada* entrada2 = (t_entrada*) nodo2;
		return entrada1->ultima_referencia > entrada2->ultima_referencia;
	}

	list_sort(tabla_entradas, masTiempoReferenciada);
	return list_get(tabla_entradas, 0);
}

t_entrada* algoritmoCircular() {
	bool buscadorEntradaConPuntero(void* nodo) {
		t_entrada* entrada = (t_entrada*) nodo;
		return (puntero_circular == entrada->entrada_asociada);
	}

	return list_find(tabla_entradas, buscadorEntradaConPuntero);
}

t_entrada* algoritmoDeReemplazo() {
	switch (protocolo_reemplazo) {
	case LRU:
		return algoritmoLRU();

	case BSU:
		return algoritmoBSU();

	default: // Circular
		return algoritmoCircular();
	}
}

int operacion_SET(t_instruccion* instruccion) {
	int entradas_a_ocupar = (int) ceilf((float) strlen(instruccion->valor) / (float)tam_entrada);
	t_entrada* entrada_a_reemplazar = NULL;
	log_info(logger, "El valor a almacenar requiere %i entradas", entradas_a_ocupar);

	if(entradas_a_ocupar <= entradas_libres){
		//Entonces, pregunto si hay entradas libres y contigüas.
		log_info(logger, "Hay %d entradas libres para almacenar el valor", entradas_a_ocupar);
		if (hayEntradasContiguas(entradas_a_ocupar) >= 0){
			log_info(logger, "Hay %d entradas contiguas", entradas_a_ocupar);
		} else {
			log_info(logger, "No hay %d entradas contiguas para almacenar el valor", entradas_a_ocupar);
			//compactarAlmacenamiento();
			log_info(logger, "Se ha compactado el almacenamiento");
		}
	} else {
		//Devuelve la entrada a reemplazar
		log_info(logger, "No hay %d entradas para almacenar el valor, aplico algoritmo %s", entradas_a_ocupar, algoritmo_reemplazo);
		entrada_a_reemplazar = algoritmoDeReemplazo(entradas_a_ocupar);
		//Libero la entrada a reemplazar
		//Entonces, pregunto si hay entradas libres y contiguas.
		if(hayEntradasContiguas(entradas_a_ocupar) >= 0){
			log_info(logger, "Hay %d entradas libres y contiguas para almacenar el valor", entradas_a_ocupar);
		} else {
			log_info(logger, "No hay %d entradas contiguas para almacenar el valor", entradas_a_ocupar);
			compactarAlmacenamiento();
			log_info(logger, "Se ha compactado el almacenamiento");
		}
	}

	t_entrada* entrada = (t_entrada*) malloc(sizeof(t_entrada));
	entrada->clave = string_new();
	string_append(&(entrada->clave), instruccion->clave);
	entrada->valor = string_new();
	string_append(&(entrada->valor), instruccion->valor);
	entrada->size_valor_almacenado = strlen(instruccion->valor);
	entrada->entradas_ocupadas = entradas_a_ocupar;
	entrada->ultima_referencia = 0;

	if (!entrada_a_reemplazar) {
		entrada->entrada_asociada = hayEntradasContiguas(entradas_a_ocupar);
	} else {
		liberarEntrada(entrada_a_reemplazar);
		entrada->entrada_asociada = entrada_a_reemplazar->entrada_asociada;
	}
	escribirEntrada(entrada);
	actualizarCantidadEntradasLibres();
	puntero_circular = entrada->entrada_asociada;
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
		//send(socketCoordinador, &PAQUETE_ERROR, sizeof(uint32_t), 0);
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
		//send(socketCoordinador, &PAQUETE_ERROR, sizeof(uint32_t), 0);
		return -1;
	}

	log_info(logger, "Le informo al Coordinador que el paquete llego correctamente");
	//send(socketCoordinador, &PAQUETE_OK, sizeof(uint32_t), 0);
	return 1;
}

void abrirArchivoInstancia(int* fileDescriptor) {
	/*
	 * La syscall open() nos permite abrir un archivo para escritura/lectura
	 * con permisos de usuario para realizar dichas operaciones.
	 */
	*fileDescriptor = open("/home/utnso/workspace/tp-2018-1c-El-Rejunte/instancia/instancia.txt", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

	if (*fileDescriptor < 0) {
		log_error(logger, "Error al abrir el archivo de Instancia");
		finalizar();
	}
}

void actualizarMapaMemoria(){
	//Reescribe el mapa de memoria con los últimos valores y claves en memoria.
	void guardarValoresEnMap(void* nodo){
		t_entrada* entrada_en_tabla = (t_entrada*) nodo;
		char* entrada_a_map = string_new();

		string_append(&entrada_a_map, entrada_en_tabla->clave);
		string_append(&entrada_a_map, "-");
		string_append(&entrada_a_map, entrada_en_tabla->valor);
		string_append(&entrada_a_map, ";");
		string_append(&entrada_a_map, "\0");

		for (int i = 0; i < string_length(entrada_a_map); i++){
			mapa_archivo[i] = entrada_a_map[i];
		}
	}

	list_iterate(tabla_entradas, guardarValoresEnMap);
}

void almacenarValorYGenerarTabla(char* valor, char* clave){
	t_entrada* entrada = (t_entrada*) malloc(sizeof(t_entrada));

	double entradas_a_ocupar = ceilf((float) entrada->size_valor_almacenado / (float)tam_entrada);

	for(int i = 0; i < cant_entradas * tam_entrada; i = i + tam_entrada){
		if(bloque_instancia[i] == '0'){
			strncpy(bloque_instancia+i, valor, strlen(valor));
			entrada->clave = string_new();
			string_append(&(entrada->clave), clave);
			entrada->valor = string_new();
			string_append(&(entrada->valor), valor);
			entrada->entrada_asociada = i;
			entrada->size_valor_almacenado = strlen(valor);
			entrada->entradas_ocupadas = entradas_a_ocupar;
			list_add(tabla_entradas, entrada);
			break;
		}
	}
}

void compactarAlmacenamiento() {
	log_debug(logger, "Se inicia la compactacion");
	for(int x = 0; x < cant_entradas * tam_entrada; x += tam_entrada) {
		if (bloque_instancia[x] == '0') {
			for(int y = x + tam_entrada; x < cant_entradas * tam_entrada; y += tam_entrada) {
				if (bloque_instancia[y] != '0') {
					log_debug(logger, "Encontre %d entradas libres", y - x);
					log_debug(logger, "Corro todos los valores posteriores hacia atras");
					int pos_reemplazo = x;
					char* porcion = string_new();
					for(int z = y; z < cant_entradas * tam_entrada; z++) {
						string_append_with_format(&porcion, "%c", bloque_instancia[z]);
					}
					strncpy(bloque_instancia + pos_reemplazo, porcion, strlen(porcion));
				}
			}
		}
	}
	log_debug(logger, "Se corrieron todas las entradas");
}

void dumpMemoria(){
	int _fd;

	// Funcion magica para comparar si esta la clave que quiero en la tabla de entradas
	void obtenerClaves(void* nodo) {
		t_entrada* entrada = (t_entrada*) nodo;
		char* _nombreArchivo = string_new();
		string_append(&_nombreArchivo, entrada->clave);
		_fd = open(_nombreArchivo, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
		if(fd < 0){
			log_error(logger, "Error al abrir archivo para DUMP");
			//ERROR AL ABRIR ARCHIVO
		} else {
			write(_fd, entrada->valor, strlen(entrada->valor));
			close(_fd);
		}
	}
	// Busco la clave en la tabla usando la funcion magica
	list_iterate(tabla_entradas, obtenerClaves);
}

void* dumpAutomatico() {
	while(1){
		sleep(intervalo_dump);
		dumpMemoria();
	}
	return NULL;
}

void escribirEntrada(t_entrada* entrada) {
	strncpy(bloque_instancia+entrada->entrada_asociada, entrada->valor, entrada->entradas_ocupadas*tam_entrada);
}

void generarTablaDeEntradas() {
	int contador;
	char* una_entrada;
	char** vec_clave_valor;

	log_info(logger, "Cargo la Tabla de Entradas con lo que esta en el disco");

	//Creo la tabla de entradas de la instancia, que consiste en una lista.
	tabla_entradas = list_create();

	//void** storage_volatil = malloc(atoi(cant_entradas) * sizeof(atoi(tam_entrada)));

	abrirArchivoInstancia(&fd);
	if (fstat(fd, &sb) < 0) {
		log_error(logger, "No se pudo obtener el tamaño de archivo");
		finalizar();
	}

	//printf("Tamaño de archivo: %ld\n", sb.st_size);

	mapa_archivo = string_new();
	mapa_archivo = mmap(0, tam_entrada*cant_entradas, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	bloque_instancia = (char*) malloc(cant_entradas * tam_entrada);
	memset(bloque_instancia, '0', cant_entradas * tam_entrada);

	//Si el tamaño del archivo es mayor a 0, es porque existía y tiene información.
	if (sb.st_size > 0) {
		/*
		 * Con mmap() paso el archivo a un bloque de memoria de igual tamaño
		 * con dirección elegida por el SO y permisos de lectura/escritura.
		 */
		/*
		 * Se crea un string vacío, donde se almacenará el contenido del archivo.
		 */

		//El contador se inicia en la posición inicial del archivo en memoria (byte 0)
		contador = 0;

		for (int i = 0; i < string_length(mapa_archivo); i++) {
			/*
			 * Cuando llego al caracter de fin de entrada, creo un string vacío donde guardarla,
			 * usando como datos el inicio (contador) y el byte actual (i) con string_substring.
			 * En un vector de strings, se divide la entrada en 2, clave y valor (separados por -).
			 * En la estructura entrada, se guarda la clave; en el campo entrada asociada se usa el contador;
			 * en el tamaño del valor almacenado, se usa la longitud del valor almacenado en el vector.
			 * Por último, se agrega a la lista un nuevo elemento con la estructura completa.
			 * El contador se actualiza a la posición siguiente al fin de entrada.
			 */
			//printf("--> i = %i \n mapa_archivo(i): %c \n", i, mapa_archivo[i]);
			if (mapa_archivo[i] == ';') {
				una_entrada = string_new();
				una_entrada = string_substring(mapa_archivo, contador, i - contador);
				vec_clave_valor = string_split(una_entrada, "-");
				almacenarValorYGenerarTabla(vec_clave_valor[1], vec_clave_valor[0]);
				contador = i + 1;
			}
		}
	} else {
		log_info(logger, "El archivo fue creado y está vacío");
	}

	printf("Lista size: %i\n", list_size(tabla_entradas));
}

int hayEntradasContiguas(int entradas_necesarias){
	int contador = 0;
	int primer_entrada;

	for (int i = 0; i < tam_entrada * cant_entradas; i = i + tam_entrada){
		if(bloque_instancia[i] == '0'){
			contador++;
			if(contador == 1){
				primer_entrada = i;
			}
			if(contador == entradas_necesarias){
				return primer_entrada;
			}
		} else {
			contador = 0;
			primer_entrada = 0;
		}
	}
	return -1;
}

void liberarEntrada(t_entrada* entrada){
	//strncpy(bloque_instancia+entrada->entrada_asociada, '0', entrada->entradas_ocupadas*tam_entrada);
	memset(bloque_instancia+entrada->entrada_asociada, 0, entrada->entradas_ocupadas*tam_entrada);
}

void actualizarCantidadEntradasLibres(){
	int cont = 0;

	// Recorre el almacenamiento por cada entrada, preguntando si el primer valor en c/u es nulo.
	for (int i = 0; i < tam_entrada * cant_entradas; i = i + tam_entrada){
		if(bloque_instancia[i] == '0'){
			cont++;
		}
	}

	entradas_libres = cont;
}

int procesar(t_instruccion* instruccion) {
	log_info(logger, "Procesando instruccion...");

	// Funcion magica para comparar si esta la clave que quiero en la tabla de entradas
	bool comparadorDeClaves(void* estructura) {
		t_entrada* entrada = (t_entrada*) estructura;
		return (strcmp(instruccion->clave, entrada->clave) == 0);
	}

	// Busco la clave en la tabla usando la funcion magica
	t_entrada* entrada = (t_entrada*) list_find(tabla_entradas, comparadorDeClaves);

	// Evaluo como procesar segun las condiciones
	if (!entrada) { // la entrada no estaba
		log_warning(logger, "La clave no esta registrada en la Tabla de Entradas");

		switch (instruccion->operacion) {
		case opSET: // SET de clave ausente
			return operacion_SET(instruccion);
		case opSTORE: // STORE de clave ausente
			log_error(logger, "Intento de STORE para una clave inexistente");
			return 1;
		}
	} else { // la entrada si estaba
		log_warning(logger, "La clave esta registrada en la Tabla de Entradas");

		switch (instruccion->operacion){
		case opSET: // SET de clave presente.
			return operacion_SET_reemplazo(entrada, instruccion->valor);
		case opSTORE: // STORE de clave presente.
			return operacion_STORE(instruccion->clave);
		}
	}
	return -1;
}

t_instruccion* recibirInstruccion(int socketCoordinador) {
	// Recibo linea de script parseada
	//uint32_t tam_paquete;
	//recv(socketCoordinador, &tam_paquete, sizeof(uint32_t), 0); // Recibo el header

	//char* paquete = (char*) malloc(sizeof(char) * tam_paquete);
	//recv(socketCoordinador, paquete, tam_paquete, 0);
	char* paquete = readline("Instruccion que llega del Coordinador: ");
	log_info(logger, "Recibi un paquete que me envia el Coordinador");

	t_instruccion* instruccion = desempaquetarInstruccion(paquete, logger);
	//destruirPaquete(paquete);

	return instruccion;
}

// Protocolo numerico de ALGORITMO_DISTRIBUCION
void establecerProtocoloReemplazo() {
	if (strcmp(algoritmo_reemplazo, "LRU")) {
		protocolo_reemplazo = LRU;
	} else if (strcmp(algoritmo_reemplazo, "BSU")) {
		protocolo_reemplazo = BSU;
	} else {
		protocolo_reemplazo = CIRC;
	}
}

t_control_configuracion cargarConfiguracion() {
	// Importo los datos del archivo de configuracion
	t_config* config = conectarAlArchivo(logger, "/home/utnso/workspace/tp-2018-1c-El-Rejunte/instancia/config_instancia.cfg", &error_config);

	ip_coordinador = obtenerCampoString(logger, config, "IP_COORDINADOR", &error_config);
	port_coordinador = obtenerCampoString(logger, config, "PORT_COORDINADOR", &error_config);
	algoritmo_reemplazo = obtenerCampoString(logger, config, "ALGORITMO_REEMPLAZO", &error_config);
	id_instancia = obtenerCampoInt(logger, config, "ID_INSTANCIA", &error_config);
	intervalo_dump = obtenerCampoInt(logger, config, "INTERVALO_DUMP", &error_config);

	establecerProtocoloReemplazo();

	// Valido si hubo errores
	if (error_config) {
		log_error(logger, "No se pudieron obtener todos los datos correspondientes");
		return CONFIGURACION_ERROR;
	}
	return CONFIGURACION_OK;
}

void finalizar() {
	if (socketCoordinador > 0) finalizarSocket(socketCoordinador);
	list_destroy(tabla_entradas);
	log_destroy(logger);
	munmap(mapa_archivo, sizeof(mapa_archivo));
	close(fd);
	free(bloque_instancia);
	free(tabla_entradas);
}

int main() {
	error_config = false;

	// Creo el logger
	logger = log_create("instancia.log", "Instancia", true, LOG_LEVEL_DEBUG);

	if (cargarConfiguracion() == CONFIGURACION_ERROR) {
		log_error(logger, "No se pudo cargar la configuracion");
		finalizar(); // Si hubo error, se corta la ejecucion.
		return EXIT_FAILURE;
	}

	// Me conecto con el Servidor y le mando mensajes
	socketCoordinador = conectarComoCliente(logger, ip_coordinador, port_coordinador);
	uint32_t handshake = INSTANCIA;
	send(socketCoordinador, &handshake, sizeof(uint32_t), 0);

	// Le aviso cual es mi ID
	send(socketCoordinador, &id_instancia, sizeof(uint32_t), 0);

	recv(socketCoordinador, &cant_entradas, sizeof(uint32_t), 0);
	recv(socketCoordinador, &tam_entrada, sizeof(uint32_t), 0);

	log_debug(logger, "cant entradas: %d\n", cant_entradas);
	log_debug(logger, "tam entrada: %d\n", tam_entrada);

	log_info(logger, "Se recibio la cantidad y tamaño de las entradas correctamente");

	generarTablaDeEntradas(); // Traigo los clave-valor que hay en disco
	imprimirTablaDeEntradas();

	imprimirBloqueEntradas();

	//Generamos temporizador
	pthread_t hiloTemporizador;
	pthread_create(&hiloTemporizador, NULL, dumpAutomatico, NULL);

	actualizarCantidadEntradasLibres();

	while (1) {
		log_debug(logger, "Cantidad de entradas libres: %d", entradas_libres);
		t_instruccion* instruccion = recibirInstruccion(socketCoordinador);
		if (validarArgumentosInstruccion(instruccion) > 0) {
			if(procesar(instruccion) > 0){
				list_iterate(tabla_entradas, incrementarUltimasReferencias);

				log_info(logger, "Le aviso al Coordinador que se proceso la instruccion");
				char** para_imprimir = string_split(mapa_archivo, ";");
				int i = 0;
				while (para_imprimir[i] != NULL) {
					printf("%s\n", para_imprimir[i]);
					i++;
				}
				//send(socketCoordinador, &entradas_libres, sizeof(uint32_t), 0);
			} else {
				log_error(logger, "Le aviso al Coordinador que no se pudo procesar la instrucción");
				//send(socketCoordinador, &PAQUETE_ERROR, sizeof(uint32_t), 0);
			}
		}
	}
	finalizar();
	return EXIT_SUCCESS;
}
