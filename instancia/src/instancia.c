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
int socketCoordinador, intervalo_dump, fd;
uint32_t cant_entradas, tam_entradas, entradas_libres;
t_list* tabla_entradas;
t_dictionary* dic_entradas;
char* mapa_archivo;
char* bloque_instancia;

struct stat sb;

const uint32_t PAQUETE_OK = 1;
const int32_t PAQUETE_ERR = -1;

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
	char* _valor;
	int _archivoClave;

	// Funcion magica para comparar si esta la clave que quiero en la tabla de entradas
	bool comparadorDeClaves(void* estructura) {
		t_entrada* entrada = (t_entrada*) estructura;
		return (strcmp(clave, entrada->clave) == 0);
	}

	// Busco la clave en la tabla usando la funcion magica
	t_entrada* entrada = (t_entrada*) list_find(tabla_entradas, comparadorDeClaves);

	_valor = string_new();

	strncpy(_valor, bloque_instancia+entrada->entrada_asociada, entrada->size_valor_almacenado);

	_nombreArchivo = clave;

	//MANEJO DE ERRORES!
	if((_archivoClave = open(_nombreArchivo, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) > 0){
		if ((int)write(_archivoClave, _valor, string_length(_valor)) > 0) {
			//SE ESCRIBIÓ PERSISTIÓ LA CLAVE
			close(_archivoClave);
		} else{
			//MANEJO DE ERRORES!
			close(_archivoClave);
		}
	}

}

void setClaveValor(t_entrada* entrada, char* valor) {
	double entradas_ocupadas;

	//Verificamos el tamaño de la nueva clave.
	if(strlen(valor) <= 100){
		//Puede usar una sola entrada.
		if(strlen((char*) dictionary_get(dic_entradas, entrada->clave)) <= 100) {
			//El valor anterior ocupaba una sola entrada.
			strncpy(bloque_instancia+entrada->entrada_asociada, valor, 100);
		} else {
			//El valor anterior ocupaba más de una entrada.
			//Calcula cuantas entradas ocupaba, y el resultado es redondeado hacia arriba si no es entero.
			entradas_ocupadas = ceilf((float) entrada->size_valor_almacenado / 100.0F);
			//Copio el nuevo valor, hasta donde corresponda, reemplazando lo restante por caracteres nulos.
			strncpy(bloque_instancia+entrada->entrada_asociada, valor, entradas_ocupadas*tam_entradas);
		}
	} else {
		//Ocupa más de una entrada.
		//¿Cuantas va a ocupar?
		entradas_ocupadas = ceil((float) strlen(valor) / 100.0F);

	}

	//Actualizamos el diccionario con el nuevo valor para la clave.
	dictionary_remove(dic_entradas, entrada->clave);
	dictionary_put(dic_entradas, entrada->clave, (char*) valor);

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

void actualizarMapaMemoria(){
	//Reescribe el mapa de memoria con los últimos valores y claves en memoria.


}

void agregarAlDiccionario(char* key, char* val){
	dictionary_put(dic_entradas, key, val);
}

void almacenarValorYGenerarTabla(char* val, char* clave){
	t_entrada* entrada = (t_entrada*) malloc(sizeof(t_entrada));

	if(strlen(val) <= 100){
		for(int i = 0; i < cant_entradas * tam_entradas; i = i + tam_entradas){
			if(bloque_instancia[i] == '0'){
				strncpy(bloque_instancia+i, val, 100);
				entrada->clave = clave;
				entrada->entrada_asociada = i;
				entrada->size_valor_almacenado = strlen(val);
				list_add(tabla_entradas, entrada);
				break;
			}
		}
	} else {
		for(int i = 0; i < cant_entradas * tam_entradas; i = i + tam_entradas){
			if(bloque_instancia[i] == '0'){
				strncpy(bloque_instancia+i, val, strlen(val));
				entrada->clave = val;
				entrada->entrada_asociada = i;
				entrada->size_valor_almacenado = strlen(val);
				list_add(tabla_entradas, entrada);
				break;
			}
		}
	}

}

void dumpMemoria(){
	int _fd;
	char* _nombreArchivo;
	// Funcion magica para comparar si esta la clave que quiero en la tabla de entradas
	void obtenerClaves(char* key, char* val) {
		_nombreArchivo = key;
		_fd = open(_nombreArchivo, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
		if(fd < 0){
			//ERROR AL ABRIR ARCHIVO
		} else {
			write(_fd, val, strlen(val));
			close(_fd);
		}
	}
	// Busco la clave en la tabla usando la funcion magica
	dictionary_iterator(dic_entradas, obtenerClaves);
}

void generarTablaDeEntradas() {
	int contador;
	char* una_entrada;
	char** vec_clave_valor;


	log_info(logger, "Cargo la Tabla de Entradas con lo que esta en el disco");

	//Creo la tabla de entradas de la instancia, que consiste en una lista.
	tabla_entradas = list_create();
	//Creo el diccionario de (claves -> valores).
	dic_entradas = dictionary_create();

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

		bloque_instancia = (char*) malloc(cant_entradas * tam_entradas);
		memset(bloque_instancia, '0', cant_entradas * tam_entradas);
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
			printf("--> i = %i \n mapa_archivo(i): %c \n", i, mapa_archivo[i]);
			if (mapa_archivo[i] == ';') {
				una_entrada = string_new();
				una_entrada = string_substring(mapa_archivo, contador, i - contador);
				vec_clave_valor = string_split(una_entrada, "-");
				agregarAlDiccionario(vec_clave_valor[0], vec_clave_valor[1]);
				almacenarValorYGenerarTabla(vec_clave_valor[1], vec_clave_valor[0]);
				/*t_entrada* entrada = (t_entrada*) malloc(sizeof(t_entrada));
				entrada->clave = vec_clave_valor[0];
				entrada->entrada_asociada = contador;
				entrada->size_valor_almacenado = string_length(vec_clave_valor[1]);
				list_add(tabla_entradas, entrada);*/
				contador = i + 1;
			}
		}

	} else {
		//El archivo fue creado y está vacío
	}

	printf("Lista size: %i\n", list_size(tabla_entradas));
}

uint32_t obtenerCantidadEntradasLibres(){
	int cont = 0;

	// Recorre el almacenamiento por cada entrada, preguntando si el primer valor en c/u es nulo.
	for (int i = 0; i < tam_entradas * cant_entradas; i = i + tam_entradas){
		if(bloque_instancia[i] == '0'){
			cont++;
		}
	}

	return cont;
}

int procesar(t_instruccion* instruccion) {
	int bandera = 0;

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
		log_warning(logger, "LA CLAVE NO EXISTE EN LA TABLA");

		switch (instruccion->operacion){
		case 2: //SET de clave ausente
			//Se agrega la tabla
			break;
		case 3: //STORE de clave ausente
			log_error(logger, "Intento de STORE para una clave inexistente");
			bandera = 1;
			break;
		}
		/*
		if (instruccion->operacion == 1) {
			// es GET: crearla
			log_info(logger, "Creo la clave en la tabla");
			t_entrada* nueva_entrada = (t_entrada*) malloc(sizeof(t_entrada));
			nueva_entrada->clave = instruccion->clave;
			nueva_entrada->entrada_asociada = tabla_entradas->elements_count;
			nueva_entrada->size_valor_almacenado = strlen(instruccion->clave);
			list_add(tabla_entradas, nueva_entrada);
		} else if (instruccion->operacion == 2) {
			log_error(logger, "Intento de SET a una clave inexistente");
			bandera = 1;
		} else {
			log_error(logger, "Intento de STORE para una clave inexistente");
			bandera = 1;
		}*/
	} else { // la entrada si estaba
		log_warning(logger, "LA CLAVE EXISTE EN LA TABLA");
		switch (instruccion->operacion){
		case 2: //SET de clave presente.
			setClaveValor(entrada, instruccion->valor);
			break;
		case 3: //STORE de clave presente.
			operacionStore(instruccion->clave);
			break;
		}
		/*if (instruccion->operacion == 1) {
			// es GET: bloquearla
			log_info(logger, "Bloqueo la clave");
		} else if (instruccion->operacion == 2) {
			// es SET: insertar valor
			setClaveValor(entrada, instruccion->valor);
		} else {
			// es STORE: insertar valor
			operacionStore(instruccion->clave);
		}*/
	}
	imprimirTablaDeEntradas();

	if(bandera == 0){
		entradas_libres = obtenerCantidadEntradasLibres();
		return entradas_libres;
	}

	return bandera;
	//log_info(logger, "Le aviso al coordinador que pude procesar correctamente");
	//send(socketCoordinador, &PAQUETE_OK, sizeof(uint32_t), 0); // DEBERIA AVISAR LA CANT_ENTRADAS_LIBRES
}

t_instruccion* recibirInstruccion(int socketCoordinador) {
	// Recibo linea de script parseada
	uint32_t tam_paquete;
	recv(socketCoordinador, &tam_paquete, sizeof(uint32_t), 0); // Recibo el header

	char* paquete = (char*) malloc(sizeof(char) * tam_paquete);
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
	munmap(mapa_archivo, sizeof(mapa_archivo));
	close(fd);
	free(bloque_instancia);
	free(tabla_entradas);
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
	printf("Entradas libres: %i\n", obtenerCantidadEntradasLibres());

	//operacionStore("futbol:messi");

	while (1) {
		t_instruccion* instruccion = recibirInstruccion(socketCoordinador);
		if (procesar(instruccion) >= 0){
			log_info(logger, "Le aviso al coordinador que pude procesar correctamente");
			send(socketCoordinador, &entradas_libres, sizeof(uint32_t), 0);
		} else {
			log_error(logger, "Le aviso al coordinador que no pude procesar correctamente");
			send(socketCoordinador, &PAQUETE_ERR, sizeof(uint32_t), 0);
			break;
		}
	}
	finalizar();
	return EXIT_SUCCESS;
}
