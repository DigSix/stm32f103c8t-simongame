/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdlib.h> // CONCEITO: "Includes" são como pegar ferramentas ou manuais 
                    // emprestados de uma biblioteca. O "stdlib.h" nos dá a 
                    // ferramenta para gerar números aleatórios (para sortear os LEDs).
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// CONCEITO: "Struct" (Estrutura) é como criar uma ficha de cadastro personalizada.
// Em vez de termos variáveis soltas para cada botão, criamos uma "ficha" chamada Botao 
// que guarda todas as informações que precisamos saber sobre ele em um lugar só.
typedef struct {
  uint8_t   evento;             // O botão foi apertado ou solto? (1 = sim, 0 = não)
  uint32_t  ultimoTick;         // Quando foi a última vez que mexeram nele? (para ignorar ruídos)
  uint8_t   pressionado;        // Ele está afundado agora? (1 = sim, 0 = não)
  uint32_t  tempoPressionado;   // Em que milissegundo ele foi afundado?
} Botao;

// CONCEITO: "Enum" é uma lista de opções fixas, como as marchas de um carro (P, R, N, D).
// Aqui definimos os 3 "modos de humor" (Estados) em que a nossa placa pode estar.
typedef enum {
  ESTADO_IDLE,        // Modo de descanso (luzes piscando em círculo)
  ESTADO_JOGAR,       // Modo de jogo valendo
  ESTADO_CONFIGURAR   // Modo para escolher a dificuldade
} Estado;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// CONCEITO: "Defines" são apelidos. Para o humano é mais fácil ler "LED_VERM" (Led Vermelho).
// Para a máquina, é mais fácil entender "GPIO_PIN_1". O define faz a tradução automática.
#define PORTA_DOS_LEDS  GPIOA
#define LED_VERM        GPIO_PIN_1
#define LED_AMAR        GPIO_PIN_3
#define LED_AZUL        GPIO_PIN_4
#define LED_VERD        GPIO_PIN_5

#define PORTA_DOS_BOTOES GPIOA
#define BOTAO_VERM       GPIO_PIN_0
#define BOTAO_AMAR       GPIO_PIN_2
#define BOTAO_AZUL       GPIO_PIN_7
#define BOTAO_VER        GPIO_PIN_6

#define PORTA_DO_BUZZER GPIOB
#define BUZZER          GPIO_PIN_0     

#define MS_DE_DEBOUNCE 40     // Tempo para ignorar o "tremor" mecânico da mola do botão
#define MS_SEGURANDO 800      // Tempo mínimo (em milissegundos) para considerar que o botão foi "segurado"

/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

// CONCEITO: Variáveis são "caixinhas" na memória onde guardamos números.
// Quando usamos colchetes [4], criamos um "Array" (como um armário com 4 gavetas),
// onde guardamos os pinos de todos os 4 LEDs de uma vez só.
uint16_t leds[4] = {
    LED_VERM,
    LED_AMAR,
    LED_AZUL,
    LED_VERD
};

// Criamos 4 "fichas de cadastro" (Structs que definimos lá em cima), uma para cada botão.
Botao botoes[4];

uint16_t pinos_botoes[4] = { 
  BOTAO_VERM, 
  BOTAO_AMAR, 
  BOTAO_AZUL, 
  BOTAO_VER
};

Estado estado_atual = ESTADO_IDLE; // A placa sempre liga no modo de descanso (IDLE)

// Configurações do jogo
uint8_t modo_pvp     = 0; // 0 = joga contra a máquina, 1 = joga contra outra pessoa
uint8_t dificuldade  = 1; // 0 = difícil, 1 = médio, 3 = fácil
uint32_t tempo_led   = 700;
uint32_t tempo_input = 2500;

// Variáveis do motor do jogo
uint8_t sequencia[32];     // Um armário com 32 gavetas: guarda até 32 cores sorteadas!
uint8_t sequencia_len = 0; // Quantas cores a sequência atual tem?
uint8_t passo_atual   = 0; // Em qual cor o jogador está no momento?
uint8_t fase_jogo     = 0; // 0 = Mostrar cores, 1 = Repetir, 2 = Criar cor (PVP), 3 = Pausa
uint32_t timer_jogo   = 0; // Um cronômetro interno para o jogo

uint8_t vez_do_p1     = 0;

static uint32_t ultima_vez_no_systick = 0; // Cronômetro para o LED da placa piscar

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void processarJogo(void); // Avisamos o sistema que esta função existe
/* USER CODE BEGIN PFP */

// CONCEITO: Uma "Função" é como uma receita de bolo. Você dá os ingredientes (no parênteses)
// e ela te devolve um resultado final. Aqui, damos o pino eletrônico e ela nos diz
// se é o botão 0, 1, 2 ou 3.
int obterIndiceDoBotaoPorPino(uint16_t pino){
  if (pino == BOTAO_VERM) return 0;
  if (pino == BOTAO_AMAR) return 1;
  if (pino == BOTAO_AZUL) return 2;
  if (pino == BOTAO_VER) return 3;
  return -1; // Se não for nenhum, retorna -1 (erro)
}

void iniciarJogo(void) {
    srand(HAL_GetTick());
    sequencia_len = 0;     
    passo_atual   = 0;
    vez_do_p1     = 1; // marca de quem é a vez

    if (modo_pvp == 0) {
        // Contra a máquina: já sorteia a primeira cor e vai pra PAUSA
        sequencia[sequencia_len++] = rand() % 4;  
        fase_jogo = 3; // Começa na fase de "respiro"
        timer_jogo = HAL_GetTick();
    } else {
        // PVP: Sequência vazia, vai direto pedir pro Player 1 criar a cor
        fase_jogo = 2; // Fase de criar a cor
        timer_jogo = HAL_GetTick();
    }
}

void trocarEstado(void){
  // Troca a "marcha" do jogo dependendo de onde estamos agora
  switch (estado_atual) {
    case ESTADO_CONFIGURAR:
      estado_atual = ESTADO_JOGAR;
      iniciarJogo(); // Quando sai da configuração, liga o jogo!
      break;

    case ESTADO_JOGAR:
      estado_atual = ESTADO_IDLE;
      break;

    case ESTADO_IDLE:
      estado_atual = ESTADO_CONFIGURAR;
      break;

  }
}

void ajustarLeds(void){
  // Primeiro, apaga todos os LEDs
  for (int i = 0; i < 4; i++){
    HAL_GPIO_WritePin(PORTA_DOS_LEDS, leds[i], GPIO_PIN_RESET);
  }
  // Se estivermos na tela de configurar, acende apenas o LED da dificuldade escolhida
  if (estado_atual == ESTADO_CONFIGURAR){
    HAL_GPIO_WritePin(PORTA_DOS_LEDS, leds[dificuldade], GPIO_PIN_SET);
  }
}

void game_over(void) {
    // Pisca todos os LEDs e apita 3 vezes para avisar que perdeu
    for (int x = 0; x < 3; x++) {
        HAL_GPIO_WritePin(PORTA_DOS_LEDS, LED_VERM|LED_AMAR|LED_AZUL|LED_VERD, GPIO_PIN_SET);
        HAL_GPIO_WritePin(PORTA_DO_BUZZER, BUZZER, GPIO_PIN_SET);
        HAL_Delay(100); // Congela a placa por 100 milissegundos, aqui não faz mal congelar ela.
        HAL_GPIO_WritePin(PORTA_DOS_LEDS, LED_VERM|LED_AMAR|LED_AZUL|LED_VERD, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(PORTA_DO_BUZZER, BUZZER, GPIO_PIN_RESET);
        HAL_Delay(100);
    }
    trocarEstado();  // Sai do JOGAR e vai para IDLE
}

// Essa função olha para todos os botões e decide se alguém apertou algo
void processarBotao(void){
    uint32_t agora = HAL_GetTick(); // Que horas são agora? (em milissegundos)

    // Repete o bloco abaixo 4 vezes (uma para cada botão)
    for (int i = 0; i < 4; i++){
        if (!botoes[i].evento) continue; // Se o botão não avisou de novidade, ignora.

        // Se a novidade aconteceu rápido demais (menos de 40ms), é ruído do metal. Ignora!
        if (agora - botoes[i].ultimoTick < MS_DE_DEBOUNCE){
            botoes[i].evento = 0;
            continue;
        }

        // Lê se o botão está no fundo (RESET/Apertado) ou em cima (SET/Solto)
        uint8_t estado_do_pino = HAL_GPIO_ReadPin(PORTA_DOS_BOTOES, pinos_botoes[i]);

        // ── PRESSIONADO ─────────────────────────────────
        if (estado_do_pino == GPIO_PIN_RESET)
        {
            botoes[i].pressionado = 1;
            botoes[i].tempoPressionado = agora; // Marca no relógio a hora que afundou
            botoes[i].evento = 0;

            // NOVO: Feedback visual! Se é a vez do jogador, acende o LED assim que ele afunda o dedo.
            if (estado_atual == ESTADO_JOGAR && fase_jogo == 1) {
                HAL_GPIO_WritePin(PORTA_DOS_LEDS, leds[i], GPIO_PIN_SET);
            }
        }
        // ── SOLTO ────────────────────────────────────────
        else
        {
            // Calcula quanto tempo ficou afundado
            uint32_t duracao = agora - botoes[i].tempoPressionado;
            botoes[i].pressionado = 0;
            botoes[i].evento = 0;
            botoes[i].ultimoTick = agora;

            // NOVO: O dedo soltou o botão, então apagamos o LED imediatamente.
            if (estado_atual == ESTADO_JOGAR && fase_jogo == 1) {
                HAL_GPIO_WritePin(PORTA_DOS_LEDS, leds[i], GPIO_PIN_RESET);
            }

            if (duracao >= MS_SEGURANDO) // Se ficou segurando mais de 800ms
            {
                if (i == 1){ // Se foi o botão amarelo (índice 1)
                  trocarEstado();
                  ajustarLeds();
                }
            }
            else // Se foi um clique rápido
            {
                switch (estado_atual) {
                  case ESTADO_IDLE:
                    break; // Não faz nada no descanso

                  case ESTADO_JOGAR:
                    if (fase_jogo == 1) { // ── O JOGADOR ESTÁ REPETINDO ──
                      if (i == sequencia[passo_atual]) {  // Acertou a cor!
                          HAL_GPIO_TogglePin(PORTA_DO_BUZZER, BUZZER); 
                          passo_atual++; 
                          timer_jogo = HAL_GetTick();  

                          if (passo_atual >= sequencia_len) {
                              // Acertou tudo! O que acontece agora?
                              if (modo_pvp == 0) {
                                  // Máquina: Sorteia uma cor sozinha e vai pra pausa
                                  if (sequencia_len < 32) {
                                    sequencia[sequencia_len++] = rand() % 4;
                                  } else {
                                      // jogador ganhou, faz algo especial
                                      game_over();  // ou uma animação de vitória
                                  }
                                  passo_atual = 0;
                                  fase_jogo = 3; 
                                  timer_jogo = HAL_GetTick();
                              } else {
                                  // PVP: Fica aguardando o jogador inventar o próximo passo
                                  fase_jogo = 2; 
                                  timer_jogo = HAL_GetTick();
                              }
                          }
                      } else {
                          game_over();  // Apertou o botão errado.
                      }
                    } 
                    else if (fase_jogo == 2) { // ── PVP: O JOGADOR ESTÁ CRIANDO A COR ──
                        HAL_GPIO_TogglePin(PORTA_DO_BUZZER, BUZZER); 
                        if (sequencia_len < 32) {
                          sequencia[sequencia_len++] = rand() % 4;
                        } else {
                          // jogador ganhou, faz algo especial
                          game_over();  // ou uma animação de vitória
                        }
                        passo_atual = 0;
                        vez_do_p1 = !vez_do_p1; // Troca a vez do jogador
                        fase_jogo = 3; // Vai para a pausa de 1 segundo
                        timer_jogo = HAL_GetTick();
                    }
                    break;

                  case ESTADO_CONFIGURAR:
                    if (i == 0) { // Dificil (Botão vermelho)
                        dificuldade = 0;
                        tempo_led   = 400;  // Pisca mais rápido
                        tempo_input = 1500; // Menos tempo pra pensar
                        HAL_GPIO_WritePin(PORTA_DOS_LEDS, LED_VERM, GPIO_PIN_SET);
                        HAL_GPIO_WritePin(PORTA_DOS_LEDS, LED_AMAR, GPIO_PIN_RESET);
                        HAL_GPIO_WritePin(PORTA_DOS_LEDS, LED_VERD, GPIO_PIN_RESET);
                    }
                    if (i == 1) { // Média (Botão amarelo)
                        dificuldade = 1;
                        tempo_led   = 700;
                        tempo_input = 2500;
                        HAL_GPIO_WritePin(PORTA_DOS_LEDS, LED_VERM, GPIO_PIN_RESET);
                        HAL_GPIO_WritePin(PORTA_DOS_LEDS, LED_AMAR, GPIO_PIN_SET);
                        HAL_GPIO_WritePin(PORTA_DOS_LEDS, LED_VERD, GPIO_PIN_RESET);
                    }
                    if (i == 3) { // Fácil (Botão verde)
                        dificuldade = 3;
                        tempo_led   = 1000; // Pisca bem devagar
                        tempo_input = 4000; // Dá 4 segundos para pensar
                        HAL_GPIO_WritePin(PORTA_DOS_LEDS, LED_VERM, GPIO_PIN_RESET);
                        HAL_GPIO_WritePin(PORTA_DOS_LEDS, LED_AMAR, GPIO_PIN_RESET);
                        HAL_GPIO_WritePin(PORTA_DOS_LEDS, LED_VERD, GPIO_PIN_SET);
                    }
                    if (i == 2) { // PVP (Botão azul)
                        modo_pvp = !modo_pvp; // Inverte o modo (liga/desliga)
                    }
                    break;
                }
            }
        }
    }
}

// CONCEITO: Essa é a função principal que processa a lógica de tempo do jogo.
void processarJogo(void) {
    uint32_t agora = HAL_GetTick(); // Pega a hora atual do cronômetro interno
    
    // Variáveis "static" não morrem quando a função acaba, elas guardam o último valor.
    static uint32_t timer_animacao = 0;
    static uint8_t led_acesso_i_idle = 0;
    static uint8_t pulsos = 0;

    switch (estado_atual) {
        case ESTADO_IDLE:
            // Faz um giro pelos LEDs a cada 150 milissegundos
            if (agora - timer_animacao >= 150) {
                timer_animacao = agora;
                // Apaga o LED anterior e acende o atual
                HAL_GPIO_WritePin(PORTA_DOS_LEDS, leds[(led_acesso_i_idle + 3) % 4], GPIO_PIN_RESET);
                HAL_GPIO_WritePin(PORTA_DOS_LEDS, leds[led_acesso_i_idle], GPIO_PIN_SET);      
                led_acesso_i_idle = (led_acesso_i_idle + 1) % 4; // Avança para o próximo da fila
            }
            break;

        case ESTADO_JOGAR:
            if (fase_jogo == 3) { // Pausa dramática antes de mostrar
                // Fica 1000ms (1 seg) aguardando antes de começar a piscar
                if (agora - timer_jogo > 1000) {
                    fase_jogo = 0; // Acabou a pausa, vai para a fase de mostrar
                    timer_jogo = agora;
                }
            }
            else if (fase_jogo == 0) {  // Modo: Máquina mostrando as cores
                if (agora - timer_jogo < tempo_led) {
                    HAL_GPIO_WritePin(PORTA_DOS_LEDS, leds[sequencia[passo_atual]], GPIO_PIN_SET);
                    HAL_GPIO_WritePin(PORTA_DO_BUZZER, BUZZER, GPIO_PIN_SET); 
                } 
                else if (agora - timer_jogo < (tempo_led + 200)) {
                    HAL_GPIO_WritePin(PORTA_DOS_LEDS, leds[sequencia[passo_atual]], GPIO_PIN_RESET);
                    HAL_GPIO_WritePin(PORTA_DO_BUZZER, BUZZER, GPIO_PIN_RESET); 
                } 
                else {
                    passo_atual++; 
                    timer_jogo = agora;

                    if (passo_atual >= sequencia_len) {
                        passo_atual = 0;    
                        fase_jogo   = 1;    // Passa a vez pro jogador repetir
                        timer_jogo  = agora;
                    }
                }
            }
            else if (fase_jogo == 1) {  // Modo: Esperando clique (Repetir ou Criar)
                if (agora - timer_jogo > tempo_input) {
                    game_over();  // Demorou demais pra apertar? Morreu.
                }
            }
            break;

        case ESTADO_CONFIGURAR:
            // Animação para mostrar se estamos no modo PVP ou Máquina
            if (agora - timer_animacao >= 150) {
                timer_animacao = agora;
                if (modo_pvp == 0) {
                    if (pulsos < 1) {
                        HAL_GPIO_TogglePin(PORTA_DOS_LEDS, leds[2]); // Pisca o LED azul
                        pulsos++;
                    } else {
                        pulsos = 0;
                    }
                } else {
                    HAL_GPIO_TogglePin(PORTA_DOS_LEDS, leds[2]);
                    pulsos++;
                    if (pulsos >= 4) pulsos = 0;
                }
            }
            break;
    }
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// CONCEITO: Uma "Interrupção" (Callback) é como alguém tocando a campainha da sua casa.
// Não importa o que a placa esteja fazendo (cozinhando, limpando), ela para imediatamente, 
// abre a porta, e executa essa função. Aqui, a porta é um botão sendo apertado.
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
  int i = obterIndiceDoBotaoPorPino(GPIO_Pin);
  if (i < 0) return; // Se não é um dos 4 botões, ignora.

  botoes[i].evento = 1; // "Levanta a bandeira" avisando que esse botão foi tocado.
}

// CONCEITO: O "SysTick" é o coração da placa batendo a cada 1 milissegundo.
// Limpamos ele para fazer o mínimo de esforço possível, assim a placa não trava.
void HAL_SYSTICK_Callback(void){
  uint32_t agora = HAL_GetTick();

  // Acende e apaga o LED da placa (PC13) a cada 150ms apenas para sabermos que ela está ligada.
  if (agora - ultima_vez_no_systick >= 50) {
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
      ultima_vez_no_systick = agora;   
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  
  // CONCEITO: O Loop Principal (while(1)) é a rotina eterna do microcontrolador.
  // Ele vai ler e executar essas duas linhas sem parar, milhões de vezes por segundo,
  // até alguém puxar o cabo da tomada.
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    processarBotao(); // 1. Verifica se alguém apertou algum botão
    processarJogo();  // 2. Processa as regras, luzes e tempos do jogo
  }
  /* USER CODE END 3 */
}

// NOTA: As funções de configuração de hardware (SystemClock_Config, MX_GPIO_Init, etc.) 
// geradas automaticamente pelo STM32Cube foram mantidas intactas, ocultadas aqui por brevidade
// já que você não precisa alterá-las para a lógica do jogo funcionar.

/**
  * @brief System Clock Configuration
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_2|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */