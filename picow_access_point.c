#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#define TCP_PORT 80
#define DEBUG_printf printf
#define POLL_TIME_S 5
#define LED 13
#define BUZZER 15

#define HTML_HEADER "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
#define HTML_PAGE_TEMPLATE "<html><head><meta http-equiv=\"refresh\" content=\"2\"></head><body><h1>Simulador de Alarme</h1><p>Modo Alerta: %s</p><a href='?alarm=%d'>%s</a><p>Temperatura: %.2f &deg;C</p></body></html>"

struct tcp_pcb *tcp_server;

repeating_timer_t timer;
bool modo_alerta = false;
bool timer_ativo = false;

bool liga_led_buzzer_callback(repeating_timer_t *rt) {
    static bool estado = false;
    gpio_put(LED, estado);
    pwm_set_gpio_level(BUZZER, estado ? 2000 : 0);
    estado = !estado;
    return true;
}

float read_temperature() {
    adc_select_input(4);
    uint16_t raw = adc_read();
    const float conversion = 3.3f / (1 << 12);  // Corrigido: 3.3V é a tensão real de referência do ADC
    float voltage = raw * conversion;
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temperature;
}

void set_modo_alerta(bool ativo) {
    if (ativo && !timer_ativo) {
        add_repeating_timer_ms(500, liga_led_buzzer_callback, NULL, &timer);
        timer_ativo = true;
    } else if (!ativo && timer_ativo) {
        cancel_repeating_timer(&timer);
        timer_ativo = false;
        gpio_put(LED, 0);
        pwm_set_gpio_level(BUZZER, 0);
    }
    modo_alerta = ativo;
    DEBUG_printf("Modo alerta %s\n", ativo ? "ATIVADO" : "DESATIVADO");
}

char html_response[1024];

err_t test_server_content(char *request, char **response) {
    if (strstr(request, "?alarm=1")) {
        set_modo_alerta(true);
    } else if (strstr(request, "?alarm=0")) {
        set_modo_alerta(false);
    }

    float temp = read_temperature();
    snprintf(html_response, sizeof(html_response), HTML_HEADER HTML_PAGE_TEMPLATE,
             modo_alerta ? "ATIVADO" : "DESATIVADO",
             modo_alerta ? 0 : 1,
             modo_alerta ? "Desativar" : "Ativar",
             temp);

    *response = html_response;
    return ERR_OK;
}

err_t tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *data = malloc(p->tot_len + 1);
    pbuf_copy_partial(p, data, p->tot_len, 0);
    data[p->tot_len] = '\0';
    DEBUG_printf("Requisição recebida:\n%s\n", data);

    char *response;
    test_server_content(data, &response);

    tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    free(data);
    return ERR_OK;
}

err_t tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, tcp_recv_cb);
    return ERR_OK;
}

void init_pwm_buzzer() {
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_wrap(slice, 2500);
    pwm_set_clkdiv(slice, 4.0);
    pwm_set_enabled(slice, true);
}

int main() {
    stdio_init_all();
    DEBUG_printf("Inicializando...\n");

    if (cyw43_arch_init()) {
        DEBUG_printf("Erro ao iniciar WiFi\n");
        return -1;
    }

    adc_init();
    adc_set_temp_sensor_enabled(true); // Ativa o sensor interno de temperatura
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);
    init_pwm_buzzer();

    const char *ssid = "picow_test";
    const char *password = "12345678";
    cyw43_arch_enable_ap_mode(ssid, password, CYW43_AUTH_WPA2_AES_PSK);

    dhcp_server_t dhcp_server;
    ip_addr_t gw, mask;
    IP4_ADDR(&gw, 192, 168, 4, 1);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    dhcp_server_init(&dhcp_server, &gw, &mask);

    dns_server_t dns_server;
    dns_server_init(&dns_server, &gw);

    tcp_server = tcp_new();
    tcp_bind(tcp_server, IP_ADDR_ANY, TCP_PORT);
    tcp_server = tcp_listen_with_backlog(tcp_server, 1);
    tcp_accept(tcp_server, tcp_accept_cb);

    DEBUG_printf("Servidor iniciado em 192.168.4.1\n");
    while (1) {
        sleep_ms(1000);
    }

    return 0;
}
