/**
 * @file codigo_fsm_morse.c
 * @brief Snippets de código para o projeto STM32 Morse Chat (Dupla Modulação IR)
 * 
 * Instruções:
 * 1. Gere o projeto no STM32CubeMX conforme as especificações.
 * 2. Copie os blocos de código abaixo e cole nas respectivas marcações 
 *    (USER CODE BEGIN ...) no arquivo Core/Src/main.c
 */

/* ========================================================================== */
/* COPIE PARA: USER CODE BEGIN PV (Private Variables)                         */
/* ========================================================================== */
/* --- VARIAVEIS DO RECEPTOR OTICO (ENVELOPE DETECTOR) --- */
volatile uint8_t  sinal_ir_ativo = 0;
volatile uint32_t timer_de_ausencia = 0;
volatile uint32_t duracao_sinal_ativo = 0;
volatile uint32_t silence_timer = 0;

volatile uint8_t  pacote_recebido_flag = 0;
volatile uint32_t pacote_duracao_final = 0;

volatile uint8_t  pending_space = 0;
volatile uint8_t  pending_word = 0;
volatile uint8_t  flag_silencio_linha = 0;
volatile uint8_t  linha_vazia = 1;

/* --- VARIAVEIS DO TRANSMISSOR OTICO (FSM) --- */
typedef enum {
    TX_IDLE = 0,
    TX_PONTO,
    TX_TRACO,
    TX_ESPACO_SIMBOLO,
    TX_ESPACO_LETRA,
    TX_ESPACO_PALAVRA
} TX_State_t;

volatile TX_State_t tx_state = TX_IDLE;
volatile uint32_t   tx_timer = 0;
volatile uint32_t   tx_duration_target = 0;

#define TX_BUFFER_SIZE 256
char tx_buffer[TX_BUFFER_SIZE];
volatile uint16_t tx_read_idx = 0;
volatile uint16_t tx_write_idx = 0;

/* --- VARIAVEIS DA UART --- */
uint8_t uart_rx_byte;


/* ========================================================================== */
/* COPIE PARA: USER CODE BEGIN PFP (Private Function Prototypes)              */
/* ========================================================================== */
void Process_TX_Symbol(void);


/* ========================================================================== */
/* COPIE PARA: USER CODE BEGIN 0                                              */
/* ========================================================================== */
void Process_TX_Symbol(void) 
{
    while (tx_read_idx != tx_write_idx) 
    {
        char c = tx_buffer[tx_read_idx];
        tx_read_idx = (tx_read_idx + 1) % TX_BUFFER_SIZE;
        tx_timer = 0; 
        
        switch (c) 
        {
            case '.':
                tx_state = TX_PONTO;
                tx_duration_target = 80;
                return;
            case '-':
                tx_state = TX_TRACO;
                tx_duration_target = 240;
                return;
            case ' ':
                tx_state = TX_ESPACO_LETRA;
                tx_duration_target = 240;
                return;
            case '/':
                tx_state = TX_ESPACO_PALAVRA;
                tx_duration_target = 560;
                return;
            default:
                // Ignora '\n', '\r' e caracteres inválidos
                break;
        }
    }
    
    // Se o buffer esvaziou sem encontrar caracteres válidos
    tx_state = TX_IDLE;
}


/* ========================================================================== */
/* COPIE PARA: USER CODE BEGIN 2 (Inicialização, antes do while(1))           */
/* ========================================================================== */
/* 1. Armar interrupcao da UART2 para receber comandos do PC */
HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1);

/* 2. Iniciar Timer PWM (38kHz). Mantem saida CC2E desabilitada por padrao (Silencio) */
HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
TIM2->CCER &= ~TIM_CCER_CC2E;

/* 3. Iniciar o gerador de base de tempo de 1ms */
HAL_TIM_Base_Start_IT(&htim3);


/* ========================================================================== */
/* COPIE PARA: USER CODE BEGIN WHILE (Dentro do while(1))                     */
/* ========================================================================== */
/* PROCESSA SIMBOLOS RECEBIDOS */
if (pacote_recebido_flag) 
{
    pacote_recebido_flag = 0;
    
    // Lazy Printing: envia os espacos pendentes ANTES do simbolo atual
    if (pending_word) {
        uint8_t b = '/';
        HAL_UART_Transmit(&huart2, &b, 1, 10);
    } else if (pending_space) {
        uint8_t e = ' ';
        HAL_UART_Transmit(&huart2, &e, 1, 10);
    }
    
    pending_space = 0;
    pending_word = 0;
    linha_vazia = 0;
    
    if (pacote_duracao_final >= 40 && pacote_duracao_final <= 140) 
    {
        uint8_t p = '.';
        HAL_UART_Transmit(&huart2, &p, 1, 10);
    } 
    else if (pacote_duracao_final > 140 && pacote_duracao_final <= 350) 
    {
        uint8_t t = '-';
        HAL_UART_Transmit(&huart2, &t, 1, 10);
    }
}

/* PROCESSA FIM DE LINHA (TIMEOUT 1000ms) */
if (flag_silencio_linha) 
{
    flag_silencio_linha = 0;
    
    // Na quebra de linha, descarta os espacos pendentes (nao sao enviados)
    pending_space = 0;
    pending_word = 0;
    
    if (!linha_vazia) {
        uint8_t n[] = "\r\n";
        HAL_UART_Transmit(&huart2, n, 2, 10);
        linha_vazia = 1;
    }
}


/* ========================================================================== */
/* COPIE PARA: USER CODE BEGIN 4 (Callbacks no final do arquivo)              */
/* ========================================================================== */

/* --- INTERRUPCAO DA BASE DE TEMPO (1kHz / 1ms) --- */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) 
    {
        /* === ENVELOPE DETECTOR (RECEPTOR) === */
        if (sinal_ir_ativo) 
        {
            timer_de_ausencia++;
            duracao_sinal_ativo++;
            
            // Timeout de seguranca de 4ms (>3.5ms). Indica fim do simbolo.
            if (timer_de_ausencia >= 4) 
            {
                sinal_ir_ativo = 0;
                pacote_duracao_final = duracao_sinal_ativo;
                duracao_sinal_ativo = 0; // Correção Bug 1: Zera o acumulador
                pacote_recebido_flag = 1;
                silence_timer = 0;
            }
        } 
        else 
        {
            silence_timer++;
            
            // Lazy Printing: apenas registra a intenção do espaço
            if (silence_timer == 240) {
                pending_space = 1;
            }
            if (silence_timer == 560) {
                pending_space = 0; // Palavra sobrescreve letra
                pending_word = 1;
            }
            if (silence_timer == 1000) {
                flag_silencio_linha = 1; // Fecha a string no PC
            }
        }

        /* === FSM TRANSMISSORA (DUPLA MODULACAO) === */
        if (tx_state != TX_IDLE) 
        {
            tx_timer++;
            
            // Sub-portadora: intercala a geracao do PWM em pulsos de 1ms
            if (tx_state == TX_PONTO || tx_state == TX_TRACO) 
            {
                if (tx_timer % 2 != 0) { 
                    TIM2->CCER |= TIM_CCER_CC2E;  // ms IMPAR -> LIGA PWM 38kHz
                } else {
                    TIM2->CCER &= ~TIM_CCER_CC2E; // ms PAR -> DESLIGA PWM
                }
            } 
            else 
            {
                // Silencio entre letras, palavras e simbolos
                TIM2->CCER &= ~TIM_CCER_CC2E; 
            }

            // Transicao de Estado
            if (tx_timer >= tx_duration_target) 
            {
                TIM2->CCER &= ~TIM_CCER_CC2E; // Garante o desligamento 
                
                if (tx_state == TX_PONTO || tx_state == TX_TRACO) {
                    // Após acender o LED, insere o espaco intra-letra (80ms padrao)
                    tx_state = TX_ESPACO_SIMBOLO;
                    tx_duration_target = 80;
                    tx_timer = 0;
                } else {
                    // Após concluir qualquer espaco, busca o proximo bit do buffer
                    Process_TX_Symbol();
                }
            }
        }
    }
}

/* --- INTERRUPCAO EXTERNA (Pino PA0 / TSOP) --- */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0) 
    {
        // Ao detectar a borda de descida de um pulso 38kHz
        sinal_ir_ativo = 1;
        timer_de_ausencia = 0;
    }
}

/* --- INTERRUPCAO DA UART (Recepcao de Comandos do PC) --- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) 
    {
        uint16_t next_write = (tx_write_idx + 1) % TX_BUFFER_SIZE;
        
        if (next_write != tx_read_idx) // Ring buffer OK (nao esta cheio)
        {
            tx_buffer[tx_write_idx] = (char)uart_rx_byte;
            tx_write_idx = next_write;
            
            // Gatilho de Envio: Detectou quebra de linha do terminal e FSM esta ociosa
            if ((uart_rx_byte == '\n' || uart_rx_byte == '\r') && tx_state == TX_IDLE) 
            {
                Process_TX_Symbol(); 
            }
        }
        else
        {
            // OVERFLOW: Trava de seguranca. Reseta os ponteiros para destravar.
            tx_read_idx = 0;
            tx_write_idx = 0;
        }
        
        // Rearma a porta para escutar o proximo byte passivamente
        HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1);
    }
}
