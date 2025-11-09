

#include "../include/tls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

void configure_context(SSL_CTX *ctx, const char* cert, const char* key);

struct addrinfo {
               int              ai_flags;
               int              ai_family;
               int              ai_socktype;
               int              ai_protocol;
               socklen_t        ai_addrlen;
               struct sockaddr *ai_addr;
               char            *ai_canonname;
               struct addrinfo *ai_next;
           };

int getaddrinfo(const char* hostname, const char* service, const struct addrinfo* hints, struct addrinfo* res[]);
void freeaddrinfo(struct addrinfo *ai);

int open_tls_socket(const char* host, const char* port, tls_client* tls, const char* cert, const char* key) {

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return 1;
    }

    configure_context(ctx,cert,key);

    struct addrinfo hints,*res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) {
        perror("getaddrinfo");
        SSL_CTX_free(ctx);
        return -1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        perror("socket");
        freeaddrinfo(res);
        SSL_CTX_free(ctx);
        return -11;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        perror("connect");
        close(sock);
        freeaddrinfo(res);
        SSL_CTX_free(ctx);
        return -11;
    }

    freeaddrinfo(res);

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);

    SSL_set_tlsext_host_name(ssl, host);

    X509_VERIFY_PARAM *param = SSL_get0_param(ssl);
    X509_VERIFY_PARAM_set1_host(param, host, 0);
//    SSL_set_verify(ssl, SSL_VERIFY_PEER, NULL);

    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(sock);
        SSL_CTX_free(ctx);
        return -1;
    }

    tls->ssl=ssl;
    tls->ctx=ctx;

    return 1;
}


void free_client_socket(void* ctx, void* ssl){

    SSL_shutdown((SSL*)ssl);
    SSL_free((SSL*)ssl);
    SSL_CTX_free((SSL_CTX*)ctx);
    EVP_cleanup();

}
