/**
 * main.c - servidor proxy socks concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo.
 *
 * Todas las conexiones entrantes se manejarán en éste hilo.
 *
 * Se descargará en otro hilos las operaciones bloqueantes (resolución de
 * DNS utilizando getaddrinfo), pero toda esa complejidad está oculta en
 * el selector.
 */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "include/selector.h"
#include "include/buffer.h"

static bool done = false;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

void
socksv5_passive_accept(struct selector_key *key) ;

int
main(const int argc, const char **argv) {

    unsigned port = 2252;

    //insert args parser

    // no tenemos nada que leer de stdin
    close(0);

    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    struct sockaddr_in addr;  //OJO esto es solo ipv4
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server < 0) {
        err_msg = "unable to create socket";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", port);

    // man 7 ip. no importa reportar nada si falla.
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        err_msg = "unable to bind socket";
        goto finally;
    }

    if (listen(server, 20) < 0) {
        err_msg = "unable to listen";
        goto finally;
    }

    // registrar sigterm es útil para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    if(selector_fd_set_nio(server) == -1) {
        err_msg = "getting server socket flags";
        goto finally;
    }
    const struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec  = 10,
            .tv_nsec = 0,
        },
    };
    if(0 != selector_init(&conf)) {
        err_msg = "initializing selector";
        goto finally;
    }

    selector = selector_new(1024);
    if(selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }
    const struct fd_handler socksv5 = {
        .handle_read       = socksv5_passive_accept,
        .handle_write      = NULL,
        .handle_close      = NULL, // nada que liberar
    };
    ss = selector_register(selector, server, &socksv5,
                                              OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }
    for(;!done;) {
        err_msg = NULL;
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "serving";
            goto finally;
        }
    }
    if(err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;
finally:
    if(ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                                  ss == SELECTOR_IO
                                      ? strerror(errno)
                                      : selector_error(ss));
        ret = 2;
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if(selector != NULL) {
        selector_destroy(selector);
    }
    selector_close();

    //socksv5_pool_destroy();

    if(server >= 0) {
        close(server);
    }
    return ret;
}

struct echo {
    int pointer_a , pointer_b;
    uint8_t raw_buff_a[1024], raw_buff_b[1024];

    int free_a ;
    buffer * readBuff, * writeBuff;
};

static void
echo_read(struct selector_key *key) {
    printf("estoy en handler de read");
    struct echo* datos = key->data;
    size_t count;
    uint8_t* ptr = buffer_write_ptr(datos->readBuff, &count);
    ssize_t n = recv(key->fd, ptr, count, 0);
    if(n <= 0)
    {
        printf("no pude leer datos : ( me voy");
        int fd = key->fd;
        selector_unregister_fd(key->s, fd);
        close(fd);
        return;
    }
    buffer_write_adv(datos->readBuff, n);
    selector_set_interest_key(key, OP_WRITE);

    printf("estoy escribiendo en el buffer");


}
static void
echo_write(struct selector_key *key) {
    struct echo* datos = key->data;
    size_t count;
    uint8_t* ptr = buffer_read_ptr(datos->readBuff, &count);

    const ssize_t n = send(key->fd, ptr,count, MSG_NOSIGNAL);
    if (n == -1)
    {
        int fd = key->fd;
        selector_unregister_fd(key->s, fd);
        close(fd);
        return;
    }
    if(n < 0)
    {
        printf("no pude mandar datos : (");
    }else
    {
        buffer_read_adv(datos->readBuff, n);
        selector_set_interest_key(key, OP_READ);
    }

}

static const struct fd_handler socks5_handler = {
    .handle_read   = echo_read,
    .handle_write  = echo_write,
    .handle_close  = NULL,
    .handle_block  = NULL,
};



/** Intenta aceptar la nueva conexión entrante*/
void
socksv5_passive_accept(struct selector_key *key) {

    struct echo                *datos           = NULL;
    printf("RECIBI CONEXION JAJA ");
    const int client = accept(key->fd, NULL, NULL);
    printf("PASE EL ACCEPTS!!!! ");
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    datos = malloc(sizeof(struct echo));
    datos->readBuff = malloc(sizeof(struct buffer));
    datos->writeBuff = malloc(sizeof(struct buffer));
    buffer_init(datos->readBuff,1024,datos->raw_buff_a);
    buffer_init(datos->writeBuff,1024,datos->raw_buff_b);

   /* if(state == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberó alguna conexión.
        goto fail;
    }
    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;*/

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &socks5_handler,
                                              OP_WRITE, datos)) {
        goto fail;
                                              }
    return ;
    fail:
        if(client != -1) {
            close(client);
        }
   // socks5_destroy(state);
}