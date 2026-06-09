/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32u0xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32u0xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_adc1;
extern ADC_HandleTypeDef hadc1;
extern LPTIM_HandleTypeDef hlptim1;
extern DMA_HandleTypeDef hdma_lpuart1_rx;
extern DMA_HandleTypeDef hdma_lpuart2_tx;
extern DMA_HandleTypeDef hdma_lpuart2_rx;
extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart3_tx;
extern UART_HandleTypeDef hlpuart1;
extern UART_HandleTypeDef hlpuart2;
extern UART_HandleTypeDef huart3;
extern RTC_HandleTypeDef hrtc;
extern DMA_HandleTypeDef hdma_spi3_rx;
extern DMA_HandleTypeDef hdma_spi3_tx;
extern SPI_HandleTypeDef hspi3;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SVC instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32U0xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32u0xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles RTC and TAMP interrupts (combined EXTI lines 20 & 21).
  */
void RTC_TAMP_IRQHandler(void)
{
  /* USER CODE BEGIN RTC_TAMP_IRQn 0 */

  /* USER CODE END RTC_TAMP_IRQn 0 */
  HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
  /* USER CODE BEGIN RTC_TAMP_IRQn 1 */

  /* USER CODE END RTC_TAMP_IRQn 1 */
}

/**
  * @brief This function handles EXTI line 4 to 15 interrupts.
  */
void EXTI4_15_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI4_15_IRQn 0 */

  /* USER CODE END EXTI4_15_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(EN_BLE_Pin);
  /* USER CODE BEGIN EXTI4_15_IRQn 1 */

  /* USER CODE END EXTI4_15_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel 1 interrupt.
  */
void DMA1_Channel1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel1_IRQn 0 */

  /* USER CODE END DMA1_Channel1_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_lpuart2_tx);
  /* USER CODE BEGIN DMA1_Channel1_IRQn 1 */

  /* USER CODE END DMA1_Channel1_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel 2 and channel 3 interrupts.
  */
void DMA1_Channel2_3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel2_3_IRQn 0 */

  /* USER CODE END DMA1_Channel2_3_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_lpuart2_rx);
  HAL_DMA_IRQHandler(&hdma_lpuart1_rx);
  /* USER CODE BEGIN DMA1_Channel2_3_IRQn 1 */

  /* USER CODE END DMA1_Channel2_3_IRQn 1 */
}

/**
  * @brief This function handles DMAMUX_OVR_IT + DMA1 channel 4 to 7 + DMA2 channel 1 to 5.
  */
void DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX_OVR_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX_OVR_IRQn 0 */

  /* USER CODE END DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX_OVR_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_adc1);
  HAL_DMA_IRQHandler(&hdma_spi3_rx);
  HAL_DMA_IRQHandler(&hdma_spi3_tx);
  HAL_DMA_IRQHandler(&hdma_usart3_rx);
  HAL_DMA_IRQHandler(&hdma_usart3_tx);
  /* USER CODE BEGIN DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX_OVR_IRQn 1 */

  /* USER CODE END DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX_OVR_IRQn 1 */
}

/**
  * @brief This function handles ADC1, COMP1 and COMP2 Interrupts (combined with EXTI 17 & 18).
  */
void ADC_COMP1_2_IRQHandler(void)
{
  /* USER CODE BEGIN ADC_COMP1_2_IRQn 0 */

  /* USER CODE END ADC_COMP1_2_IRQn 0 */
  HAL_ADC_IRQHandler(&hadc1);
  /* USER CODE BEGIN ADC_COMP1_2_IRQn 1 */

  /* USER CODE END ADC_COMP1_2_IRQn 1 */
}

/**
  * @brief This function handles TIM6, DAC and LPTIM1 global Interrupts (combined with EXTI 31).
  */
void TIM6_DAC_LPTIM1_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_DAC_LPTIM1_IRQn 0 */

  /* USER CODE END TIM6_DAC_LPTIM1_IRQn 0 */
  HAL_LPTIM_IRQHandler(&hlptim1);
  /* USER CODE BEGIN TIM6_DAC_LPTIM1_IRQn 1 */

  /* USER CODE END TIM6_DAC_LPTIM1_IRQn 1 */
}

/**
  * @brief This function handles SPI2 and SPI3 global interrupt.
  */
void SPI2_3_IRQHandler(void)
{
  /* USER CODE BEGIN SPI2_3_IRQn 0 */

  /* USER CODE END SPI2_3_IRQn 0 */
  HAL_SPI_IRQHandler(&hspi3);
  /* USER CODE BEGIN SPI2_3_IRQn 1 */

  /* USER CODE END SPI2_3_IRQn 1 */
}

/**
  * @brief This function handles USART2 global interrupt (combined with EXTI 26) + LPUART2 global interrupt (combined with EXTI lines 35).
  */
void USART2_LPUART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_LPUART2_IRQn 0 */

  /* USER CODE END USART2_LPUART2_IRQn 0 */
  HAL_UART_IRQHandler(&hlpuart2);
  /* USER CODE BEGIN USART2_LPUART2_IRQn 1 */

  /* USER CODE END USART2_LPUART2_IRQn 1 */
}

/**
  * @brief This function handles USART3 (combined with EXTI 24) + LPUART1 global interrupt (combined with EXTI lines 28).
  */
void USART3_LPUART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART3_LPUART1_IRQn 0 */

  /* USER CODE END USART3_LPUART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart3);
  HAL_UART_IRQHandler(&hlpuart1);
  /* USER CODE BEGIN USART3_LPUART1_IRQn 1 */

  /* USER CODE END USART3_LPUART1_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
