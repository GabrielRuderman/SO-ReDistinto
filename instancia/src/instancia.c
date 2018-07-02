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

t_log* logger;
bool error_config;
char* ip_coordinador;
char* port_coordinador;
uint32_t id_instancia;
int socketCoordinador, intervalo_dump;
uint32_t cant_entradas, tam_entradas;

t_list* tabla_entradas;
char* mapa_archivo;
int algoritmo_reemplazo;
const uint32_t PAQUETE_OK = 1;


void algoritmoDeReemplazo(int valor, t_entrada* nva_entrada){

	switch(valor){
	case 1: return algoritmoCircular(nva_entrada);
	break;
	case 2: return algoritmoLRU(nva_entrada);
	break;
	//case 3: return algortimoBSU(nva_entrada);
	//break;
	}
}

/**
	* @NAME: algortimoCircular
	* @DESC: Va iterando entre las entradas secuencialmente hasta el final de la tabla.
	*/

void algoritmoCircular(t_entrada* nva_entrada){

	for(int cont=0; cant_entradas >= cont ; cont++){

		list_add_in_index(tabla_entradas, cont, nva_entrada);

		while(cont == cant_entradas){ // vuelve el contador al principio de la tabla de entradas
			cont = 0;
		}
		break;
	}
}

/**
	* @NAME: algortimoLSU (Least Recently Used)
	* @DESC: selecciona la entrada que fue referenciada hace más tiempo y la reemplaza. (para ello cada entrada guarda
	* la variable cantOperaciones).
	*/

bool compararOperaciones(void* nodo1, void* nodo2){

	t_entrada* entrada1 = (t_entrada*) nodo1;
	t_entrada* entrada2 = (t_entrada*) nodo2;

	if(entrada1->cantOperaciones > entrada2->cantOperaciones){
		return true;
	} else {
		return false;
	}
}

void algoritmoLRU(t_entrada* nueva_entrada){

	if(!(list_is_empty(tabla_entradas))){

	list_sort(tabla_entradas, compararOperaciones);

	list_remove(tabla_entradas, 0);

	list_add_in_index(tabla_entradas, 0, nueva_entrada);

	} else {
		list_add_in_index(tabla_entradas, 0, nueva_entrada);
	}
}

/**
	* @NAME: algortimoBSU (Biggest Space Used)
	* @DESC: lleva registro del tamaño dentro de la entrada atomica que está siendo ocupada y selecciona la que tiene
	* mayor espacio libre.
	*/

//void algoritmoBSU(){ //Biggest Space Used

//}

void imprimirTablaDeEntradas() {
	printf("\n_______TABLA DE ENTRADAS_______\n");
	for(int i = 0; i < list_size(tabla_entradas); i++) {
		t_entrada* entrada = list_get(tabla_entradas, i);
		printf("%s - %d - %d\n", entrada->clave, entrada->entrada_asociada, entrada->size_valor_almacenado);
	}
	printf("\n");
}

void operacionStore(char* clave) {
	char* _nombreArchivo;
	char* _entrada;
	char* _valor;
	char** _vecClaveValor;
	int _contador = 0;
	int _estaClave = 0;
	int _file;

	//Recorre las entradas hasta encontrar la clave pedida.
	for(int i = 0; i < string_length(mapa_archivo); i++) {
			if (mapa_archivo[i] == ';'){
				_entrada = string_new();
				_entrada = string_substring(mapa_archivo, _contador, i - _contador);
				_vecClaveValor = string_split(_entrada, "-");
				//Si el string del vector es igual al string de la clave pasada, guardame el valor.
				if (strcmp(_vecClaveValor[0], clave)) {
					_estaClave = 1;
					_valor = string_new();
					strncpy(_valor, _vecClaveValor[1], string_length(_vecClaveValor[1]));
					break;
				}
				_contador = i + 1;
			}
		}
	//Si fué encontrada la clave y el valor, creame un archivo con el nombre de la clave
	//y después guardame el valor dentro.
	if (_estaClave > 0) {
		_nombreArchivo = string_new();
		strcpy(_nombreArchivo, _vecClaveValor[0]);
		_file = open(_nombreArchivo, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
		if (_file < 0) {
			perror("Error al crear archivo para la clave");
			close(_file);
			exit(1);
		}
		if ((int)write(_file, _valor, string_length(_valor)) < 0) {
			perror("Error al escribir el valor en la entrada");
			close(_file);
			exit(1);
		}
		close(_file);
	}
}

void setClaveValor(t_entrada* entrada, char* valor) {
	// Guardar valor en el mapa de memoria
	int fd, mapa_pos, mapa_pos_valor;
	struct stat sb;
	mapa_archivo = string_new();

	abrirArchivoInstancia(&fd);
	if (fstat(fd, &sb) < 0) {
		log_error(logger, "No se pudo obtener el tamaño de archivo");
		finalizar();
	}
	mapa_pos = entrada->entrada_asociada;
	mapa_archivo = mmap(0, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	for(int i = mapa_pos; i < string_length(mapa_archivo); i++){
		if(mapa_archivo[i] == '-'){
			mapa_pos_valor = i + 1;
			break;
		}
	}
	int contador = 0;
	for(int j = mapa_pos_valor; j <= entrada->size_valor_almacenado; j++){
		mapa_archivo[j] = valor[contador];
		contador++;
	}

	munmap(mapa_archivo, sizeof(mapa_archivo));
	close(fd);
}

void imprimirArgumentosInstruccion(t_instruccion* instruccion) {
	printf("La instruccion recibida es: ");
	switch (instruccion->operacion) {
	case 1:
		printf("GET %s\n", instruccion->clave);
		break;

	case 2:
		printf("SET %s %s\n", instruccion->clave, instruccion->valor);
		break;

	case 3:
		printf("STORE %s\n", instruccion->clave);
		break;

	default:
		log_error(logger, "No comprendo la instruccion.\n");
		break;
	}
}




void abrirArchivoInstancia(int* fileDescriptor) {
	/*
	 * La syscall open() nos permite abrir un archivo para escritura/lectura
	 * con permisos de usuario para realizar dichas operaciones.
	 */
	*fileDescriptor = open("instancia.txt", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

	if (*fileDescriptor < 0) {
		log_error(logger, "Error al abrir el archivo de Instancia");
		finalizar();
	}
}

void generarTablaDeEntradas() {
	int fd, contador;
	char* una_entrada;
	char** vec_clave_valor;
	struct stat sb;

	log_info(logger, "Cargo la Tabla de Entradas con lo que esta en el disco");

	//Creo la tabla de entradas de la instancia, que consiste en una lista.
	tabla_entradas = list_create();

	//void** storage_volatil = malloc(atoi(cant_entradas) * sizeof(atoi(tam_entradas)));

	abrirArchivoInstancia(&fd);
	if (fstat(fd, &sb) < 0) {
		log_error(logger, "No se pudo obtener el tamaño de archivo");
		finalizar();
	}

	printf("Tamaño de archivo: %ld\n", sb.st_size);

	//Si el tamaño del archivo es mayor a 0, es porque existía y tiene información.
	if (sb.st_size > 0) {
		/*
		 * Con mmap() paso el archivo a un bloque de memoria de igual tamaño
		 * con dirección elegida por el SO y permisos de lectura/escritura.
		 */
		/*
		 * Se crea un string vacío, donde se almacenará el contenido del archivo.
		 */
		mapa_archivo = string_new();
		mapa_archivo = mmap(0, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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
			if (mapa_archivo[i] == ';') {
				una_entrada = string_new();
				una_entrada = string_substring(mapa_archivo, contador, i - contador);
				vec_clave_valor = string_split(una_entrada, "-");
				t_entrada* entrada = (t_entrada*) malloc(sizeof(t_entrada));
				entrada->clave = vec_clave_valor[0];
				entrada->entrada_asociada = contador;
				entrada->size_valor_almacenado = string_length(vec_clave_valor[1]);
				list_add(tabla_entradas, entrada);
				contador = i + 1;
			}
		}

	} else {
		//El archivo fue creado y está vacío
	}

	printf("Lista size: %i\n", list_size(tabla_entradas));

	munmap(mapa_archivo, sizeof(mapa_archivo));
	close(fd);
}

void procesar(t_instruccion* instruccion) {
	log_info(logger, "Procesando instruccion...");

	// Funcion magica para comparar si esta la clave que quiero en la tabla de entradas
	bool comparadorDeClaves(void* estructura) {
		t_entrada* entrada = (t_entrada*) estructura;
		if (strcmp(instruccion->clave, entrada->clave) == 0)
			return true;
		return false;
	}

	// Busco la clave en la tabla usando la funcion magica
	t_entrada* entrada = (t_entrada*) list_find(tabla_entradas, comparadorDeClaves);

	// Evaluo como procesar segun las condiciones
	if (!entrada) { // la entrada no estaba
		log_warning(logger, "LA CLAVE NO EXISTE EN LA TABLA");
		if (instruccion->operacion == 1) {
			// es GET: crearla
			log_info(logger, "Creo la clave en la tabla");
			t_entrada* nueva_entrada = (t_entrada*) malloc(sizeof(t_entrada));
			nueva_entrada->clave = instruccion->clave;
			nueva_entrada->entrada_asociada = tabla_entradas->elements_count;
			nueva_entrada->size_valor_almacenado = strlen(instruccion->clave);
			nueva_entrada->cantOperaciones = 1;
			//acá se aplica alguno de los tres algoritmos de reemplazo
			algoritmoDeReemplazo(algoritmo_reemplazo, nueva_entrada);

			//list_add(tabla_entradas, nueva_entrada);
		} else if (instruccion->operacion == 2) {
			log_info(logger, "No hay que hacer nada");
			// es SET: no hacer nada
		} else {
			// es STORE
		}
	} else { // la entrada si estaba
		log_warning(logger, "LA CLAVE EXISTE EN LA TABLA");
		if (instruccion->operacion == 1) {
			// es GET: bloquearla
			log_info(logger, "Bloqueo la clave");
		} else if (instruccion->operacion == 2) {
			// es SET: insertar valor
			setClaveValor(entrada, instruccion->valor);
		} else {
			operacionStore(instruccion->clave);
		}
	}
	imprimirTablaDeEntradas();
	log_info(logger, "Le aviso al coordinador que pude procesar correctamente");
	send(socketCoordinador, &PAQUETE_OK, sizeof(uint32_t), 0); // DEBERIA AVISAR LA CANT_ENTRADAS_LIBRES
}

t_instruccion* recibirInstruccion(int socketCoordinador) {
	// Recibo linea de script parseada
	uint32_t tam_paquete;
	recv(socketCoordinador, &tam_paquete, sizeof(uint32_t), 0); // Recibo el header

	char* paquete = (char*) malloc(tam_paquete);
	recv(socketCoordinador, paquete, tam_paquete, 0); // MSG_WAITALL
	log_info(logger, "Recibi un paquete que me envia el Coordinador");

	t_instruccion* instruccion = desempaquetarInstruccion(paquete, logger);
	destruirPaquete(paquete);

	imprimirArgumentosInstruccion(instruccion);

	log_info(logger, "Le informo al Coordinador que el paquete llego correctamente");
	send(socketCoordinador, &PAQUETE_OK, sizeof(uint32_t), 0); // Envio respuesta al Coordinador

	return instruccion;
}

t_control_configuracion cargarConfiguracion() {
	// Importo los datos del archivo de configuracion
	t_config* config = conectarAlArchivo(logger, "/home/utnso/workspace/tp-2018-1c-El-Rejunte/instancia/config_instancia.cfg", &error_config);

	ip_coordinador = obtenerCampoString(logger, config, "IP_COORDINADOR", &error_config);
	port_coordinador = obtenerCampoString(logger, config, "PORT_COORDINADOR", &error_config);
	id_instancia = obtenerCampoInt(logger, config, "ID_INSTANCIA", &error_config);
	intervalo_dump = obtenerCampoInt(logger, config, "INTERVALO_DUMP", &error_config);

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
}

int main() {
	error_config = false;

	// Creo el logger
	logger = log_create("instancia.log", "Instancia", true, LOG_LEVEL_INFO);

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
	recv(socketCoordinador, &tam_entradas, sizeof(uint32_t), 0);

	printf("cant entradas: %d\n", cant_entradas);
	printf("tam entradas: %d\n", tam_entradas);

	log_info(logger, "Se recibio la cantidad y tamaño de las entradas correctamente");

	generarTablaDeEntradas(); // Traigo los clave-valor que hay en disco
	imprimirTablaDeEntradas();

	while (1) {
		t_instruccion* instruccion = recibirInstruccion(socketCoordinador);
		procesar(instruccion);
	}

	finalizar();
	return EXIT_SUCCESS;
}
