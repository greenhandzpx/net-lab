#include "http.h"
#include "tcp.h"
#include "net.h"
#include "assert.h"

#define TCP_FIFO_SIZE 40

typedef struct http_fifo {
    tcp_connect_t* buffer[TCP_FIFO_SIZE];
    uint8_t front, tail, count;
} http_fifo_t;

static http_fifo_t http_fifo_v;

static void http_fifo_init(http_fifo_t* fifo) {
    fifo->count = 0;
    fifo->front = 0;
    fifo->tail = 0;
}

static int http_fifo_in(http_fifo_t* fifo, tcp_connect_t* tcp) {
    if (fifo->count >= TCP_FIFO_SIZE) {
        return -1;
    }
    fifo->buffer[fifo->front] = tcp;
    fifo->front++;
    if (fifo->front >= TCP_FIFO_SIZE) {
        fifo->front = 0;
    }
    fifo->count++;
    return 0;
}

static tcp_connect_t* http_fifo_out(http_fifo_t* fifo) {
    if (fifo->count == 0) {
        return NULL;
    }
    tcp_connect_t* tcp = fifo->buffer[fifo->tail];
    fifo->tail++;
    if (fifo->tail >= TCP_FIFO_SIZE) {
        fifo->tail = 0;
    }
    fifo->count--;
    return tcp;
}

static size_t get_line(tcp_connect_t* tcp, char* buf, size_t size) {
    size_t i = 0;
    while (i < size) {
        char c;
        if (tcp_connect_read(tcp, (uint8_t*)&c, 1) > 0) {
            if (c == '\n') {
                break;
            }
            if (c != '\n' && c != '\r') {
                buf[i] = c;
                i++;
            }
        }
        net_poll();
    }
    buf[i] = '\0';
    return i;
}

static size_t http_send(tcp_connect_t* tcp, const char* buf, size_t size) {
    size_t send = 0;
    printf("http_send: size %lld\n", size);
    while (send < size) {
        size_t cnt = tcp_connect_write(tcp, (const uint8_t*)buf + send, size - send);
        printf("http_send: cnt %lld\n", cnt);
        send += cnt;
        net_poll();
    }
    return send;
}

static void close_http(tcp_connect_t* tcp) {
    tcp_connect_close(tcp);
    printf("http closed.\n");
}



static void send_file(tcp_connect_t* tcp, const char* url) {
    FILE* file;
    uint32_t size = 0;

    size_t max_file_size = 30000;
    // const char* content_type = "text/html";
    char file_path[255];
    char *tx_buffer = malloc(max_file_size);
    memset(tx_buffer, 0, max_file_size);
    char *content = malloc(max_file_size);
    memset(content, 0, max_file_size);
    /*
    解析url路径，查看是否是查看XHTTP_DOC_DIR目录下的文件
    如果不是，则发送404 NOT FOUND
    如果是，则用HTTP/1.0协议发送

    注意，本实验的WEB服务器网页存放在XHTTP_DOC_DIR目录中
    */

   // TODO
    printf("url %s\n", url);
        // sprintf(tx_buffer, "HTTP/1.0 ")
        // FILE *file = fopen("../htmldocs/index.html", "r+");
        // // bytes = fread(tx_buffer, sizeof(char), 1024, file);
        // sprintf(tx_buffer, "hello world");
        // bytes = strlen(tx_buffer);
        // printf("read %lld bytes\n", bytes);

    if (strcmp(url, "/") == 0 || strcmp(url, "/index.html") == 0
        || strcmp(url, "/img1.jpg") == 0
        || strcmp(url, "/img2.jpg") == 0
        || strcmp(url, "/img3.jpg") == 0
        || strcmp(url, "/img4.jpg") == 0
        || strcmp(url, "/img5.jpg") == 0
        || strcmp(url, "/img6.jpg") == 0
        || strcmp(url, "/page1.html") == 0) {
        strcpy(file_path, XHTTP_DOC_DIR);
        if (strcmp(url, "/") == 0) {
            strcat(file_path, "/index.html");
        } else {
            strcat(file_path, url);
        }
        printf("file path %s\n", file_path);
        FILE *file = fopen(file_path, "rb");
        if (file == NULL) {
            printf("read file fail\n");
        }
        // strcat(tx_buffer, "hello world");
        // size = strlen(tx_buffer);
        size_t read_bytes = fread(content, sizeof(char), max_file_size, file);
        printf("content length %lld\n", read_bytes);
        strcpy(tx_buffer, "HTTP/1.0 200 OK\r\n");
        if (
            strcmp(url, "/img1.jpg") == 0
            || strcmp(url, "/img2.jpg") == 0
            || strcmp(url, "/img3.jpg") == 0
            || strcmp(url, "/img4.jpg") == 0
            || strcmp(url, "/img5.jpg") == 0
            || strcmp(url, "/img6.jpg") == 0
        ) {
            strcat(tx_buffer, "Content-Type: image/png \r\n");
        }
        sprintf(tx_buffer + strlen(tx_buffer), "Content-Length: %lld\r\n\r\n", read_bytes);
        size = strlen(tx_buffer);
        memcpy(tx_buffer + strlen(tx_buffer), content, read_bytes);
        size += read_bytes;
    } else {
        strcpy(tx_buffer, "HTTP/1.0 404 NOTFOUND\r\n \r\n\r\n");
        strcat(tx_buffer, "404 NOT FOUND");
        size = strlen(tx_buffer);
    }
    // size = strlen(tx_buffer);
    http_send(tcp, tx_buffer, size);

    free(tx_buffer);
    free(content);
}

static void http_handler(tcp_connect_t* tcp, connect_state_t state) {
    if (state == TCP_CONN_CONNECTED) {
        http_fifo_in(&http_fifo_v, tcp);
        printf("http conntected.\n");
    } else if (state == TCP_CONN_DATA_RECV) {
    } else if (state == TCP_CONN_CLOSED) {
        printf("http closed.\n");
    } else {
        assert(0);
    }
}


// 在端口上创建服务器。

int http_server_open(uint16_t port) {
    if (tcp_open(port, http_handler)) {
        printf("cannot open http server\n");
        return -1;
    }
    printf("http server open\n");
    http_fifo_init(&http_fifo_v);
    return 0;
}

// 从FIFO取出请求并处理。新的HTTP请求时会发送到FIFO中等待处理。

void http_server_run(void) {
    tcp_connect_t* tcp;
    char url_path[255];
    char rx_buffer[1024];

    // printf("http server run\n");

    while ((tcp = http_fifo_out(&http_fifo_v)) != NULL) {
        int i;
        char* c = rx_buffer;

        printf("fetch a tcp from fifo\n");

        /*
        1、调用get_line从rx_buffer中获取一行数据，如果没有数据，则调用close_http关闭tcp，并继续循环
        */

        // TODO
        int cnt = 0;
        if ((cnt = get_line(tcp, c, 1024)) == 0) {
            close_http(tcp);
            continue;
        }


        /*
        2、检查是否有GET请求，如果没有，则调用close_http关闭tcp，并继续循环
        */

        // TODO
        if (cnt < 3) {
            close_http(tcp);
            continue;
        }
        if (!(c[0] == 'G' && c[1] == 'E' && c[2] == 'T')) {
            close_http(tcp);
            continue;
        }


        /*
        3、解析GET请求的路径，注意跳过空格，找到GET请求的文件，调用send_file发送文件
        */

        // TODO
        printf("first line: %s\n", c);
        int start_idx = -1;
        int n = 0;
        for (int i = 0; i < cnt; ++i) {
            if (c[i] == '/' && start_idx == -1) {
                start_idx = i;
            }
            if (c[i] == ' ' && start_idx != -1) {
                n = i - start_idx;
                break;
            }
        }
        printf("start idx %d, n %d\n", start_idx, n);
        snprintf(url_path, n + 1, &c[start_idx]);
        // printf("url_path %c\n", c[start_idx]);
        // printf("url_path %c\n", url_path[0]);
        send_file(tcp, url_path);

        /*
        4、调用close_http关掉连接
        */

        // TODO
        close_http(tcp);


        printf("!! final close\n");
    }
}
