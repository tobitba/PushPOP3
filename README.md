## Ubicación de todo lo del informe
## Generación del Ejecutable
Simplemente pararse en la raíz del directorio y correr el comando
    ```make all```
## Ubicación de Artefactos Generados
Se generarán dos ejecutables en la raíz del directorio, client.out y pop3.out
## Proceso de Ejecución
Para inicializar el servidor, correr el comando
    ```./pop3.out -d [path/to/maildir] -u ususario:contraseña```
Para conectarse, usar el comando
    ```ncat -C localhost 2252```
Para pedir metricas o cambiar la configuración esta el ejecutable client.out. Para ejecutarlo correr el comando
    ```./client.out -p 2252```
Por defecto el servidor utiliza el puerto 2252