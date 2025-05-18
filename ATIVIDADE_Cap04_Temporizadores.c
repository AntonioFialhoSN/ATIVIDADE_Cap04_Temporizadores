#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"

// Definição dos pinos
#define LED_VERDE 11
#define LED_VERMELHO 13
#define BOTAO_PEDESTRE 5
#define BUZZER_PIN 21
#define FREQ_BUZZER 1000  // Frequência mais audível

// Constantes para estados do semáforo
#define SINAL_PARADO 0
#define SINAL_LIVRE 1
#define SINAL_ATENCAO 2
#define SINAL_PEDESTRE 3

// Variáveis globais
volatile int sinal_atual = SINAL_PARADO;
volatile bool pedido_travessia = false;
volatile bool som_ativado = false;
volatile absolute_time_t tempo_fim_som;

struct repeating_timer temporizador_principal;
struct repeating_timer temporizador_buzzer;

// Configuração do PWM para o buzzer
void configurar_buzzer_pwm(uint pino) {
    gpio_set_function(pino, GPIO_FUNC_PWM);
    uint num_slice = pwm_gpio_to_slice_num(pino);
    
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, clock_get_hz(clk_sys) / (FREQ_BUZZER * 4096));
    pwm_init(num_slice, &cfg, true);
    pwm_set_gpio_level(pino, 0);
}

// Ativa o buzzer por um período específico
void ativar_buzzer(uint duracao_ms) {
    pwm_set_gpio_level(BUZZER_PIN, 2048); // 50% duty cycle
    som_ativado = true;
    tempo_fim_som = make_timeout_time_ms(duracao_ms);
}

// Callback para desativar o buzzer
bool callback_buzzer(struct repeating_timer *t) {
    if (som_ativado && time_reached(tempo_fim_som)) {
        pwm_set_gpio_level(BUZZER_PIN, 0);
        som_ativado = false;
    }
    return true;
}

// Função para configurar os LEDs
void inicializar_leds() {
    gpio_init(LED_VERDE);
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_put(LED_VERDE, 0);
    gpio_put(LED_VERMELHO, 1); // Inicia com o vermelho ligado
}

void callback_botao(uint gpio, uint32_t eventos) {
    if (gpio == BOTAO_PEDESTRE) {
        pedido_travessia = true;
    }
}

void configurar_botao() {
    gpio_init(BOTAO_PEDESTRE);
    gpio_set_dir(BOTAO_PEDESTRE, GPIO_IN);
    gpio_pull_up(BOTAO_PEDESTRE);
    gpio_set_irq_enabled_with_callback(BOTAO_PEDESTRE, GPIO_IRQ_EDGE_FALL, true, &callback_botao);
}

void alterar_sinal() {
    switch (sinal_atual) {
        case SINAL_PARADO:
            ativar_buzzer(100);
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 0);
            printf("Estado: Vermelho - Pare\n");
            break;
        case SINAL_LIVRE:
            gpio_put(LED_VERMELHO, 0);
            gpio_put(LED_VERDE, 1);
            printf("Estado: Verde - Siga\n");
            break;
        case SINAL_ATENCAO:
        case SINAL_PEDESTRE:
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 1);
            printf("Estado: Amarelo - Atenção\n");
            break;
    }
}

bool callback_temporizador(struct repeating_timer *t) {
    static int contagem = 10; // Inicia com 10 segundos no vermelho
    
    if (pedido_travessia && sinal_atual != SINAL_PEDESTRE) {
        sinal_atual = SINAL_PEDESTRE;
        contagem = 3;
        pedido_travessia = false;
        alterar_sinal();
        return true;
    }
    
    contagem--;
    
    if (contagem <= 0) {
        switch (sinal_atual) {
            case SINAL_PARADO:
                sinal_atual = SINAL_LIVRE;
                contagem = 10;
                break;
            case SINAL_LIVRE:
                sinal_atual = SINAL_ATENCAO;
                contagem = 3;
                break;
            case SINAL_ATENCAO:
                sinal_atual = SINAL_PARADO;
                contagem = 10;
                break;
            case SINAL_PEDESTRE:
                sinal_atual = SINAL_PARADO;
                contagem = 10;
                break;
        }
        alterar_sinal();
    }
    return true;
}

int main() {
    stdio_init_all();
    sleep_ms(2000); // Aguarda inicialização USB
    printf("Iniciando controle de semáforo...\n");

    inicializar_leds();
    configurar_botao();
    configurar_buzzer_pwm(BUZZER_PIN); // Descomente se for usar o buzzer
    
    // Configura temporizadores
    add_repeating_timer_ms(1000, callback_temporizador, NULL, &temporizador_principal);
    add_repeating_timer_ms(100, callback_buzzer, NULL, &temporizador_buzzer);

    // Estado inicial já configurado como vermelho
    sinal_atual = SINAL_PARADO;
    alterar_sinal();
    
    while (1) {
        tight_loop_contents();
    }
    
    return 0;
}