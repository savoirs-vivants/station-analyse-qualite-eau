/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
#include "usart.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

UART_HandleTypeDef hlpuart1;
UART_HandleTypeDef hlpuart2;
UART_HandleTypeDef hlpuart3;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart4;
DMA_HandleTypeDef hdma_lpuart1_rx;
DMA_HandleTypeDef hdma_lpuart2_tx;
DMA_HandleTypeDef hdma_lpuart2_rx;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;

/* LPUART1 init function */

void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 9600;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_SWAP_INIT;
  hlpuart1.AdvancedInit.Swap = UART_ADVFEATURE_SWAP_ENABLE;
  hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}
/* LPUART2 init function */

void MX_LPUART2_UART_Init(void)
{

  /* USER CODE BEGIN LPUART2_Init 0 */

  /* USER CODE END LPUART2_Init 0 */

  /* USER CODE BEGIN LPUART2_Init 1 */

  /* USER CODE END LPUART2_Init 1 */
  hlpuart2.Instance = LPUART2;
  hlpuart2.Init.BaudRate = 9600;
  hlpuart2.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart2.Init.StopBits = UART_STOPBITS_1;
  hlpuart2.Init.Parity = UART_PARITY_NONE;
  hlpuart2.Init.Mode = UART_MODE_TX_RX;
  hlpuart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_HalfDuplex_Init(&hlpuart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART2_Init 2 */

  /* USER CODE END LPUART2_Init 2 */

}
/* LPUART3 init function */

void MX_LPUART3_UART_Init(void)
{

  /* USER CODE BEGIN LPUART3_Init 0 */

  /* USER CODE END LPUART3_Init 0 */

  /* USER CODE BEGIN LPUART3_Init 1 */

  /* USER CODE END LPUART3_Init 1 */
  hlpuart3.Instance = LPUART3;
  hlpuart3.Init.BaudRate = 209700;
  hlpuart3.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart3.Init.StopBits = UART_STOPBITS_1;
  hlpuart3.Init.Parity = UART_PARITY_NONE;
  hlpuart3.Init.Mode = UART_MODE_TX_RX;
  hlpuart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  hlpuart3.FifoMode = UART_FIFOMODE_DISABLE;
  if (HAL_UART_Init(&hlpuart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART3_Init 2 */

  /* USER CODE END LPUART3_Init 2 */

}
/* USART3 init function */

void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}
/* USART4 init function */

void MX_USART4_UART_Init(void)
{

  /* USER CODE BEGIN USART4_Init 0 */

  /* USER CODE END USART4_Init 0 */

  /* USER CODE BEGIN USART4_Init 1 */

  /* USER CODE END USART4_Init 1 */
  huart4.Instance = USART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart4, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart4, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART4_Init 2 */

  /* USER CODE END USART4_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  if(uartHandle->Instance==LPUART1)
  {
  /* USER CODE BEGIN LPUART1_MspInit 0 */

  /* USER CODE END LPUART1_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
    PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* LPUART1 clock enable */
    __HAL_RCC_LPUART1_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**LPUART1 GPIO Configuration
    PC0     ------> LPUART1_RX
    PC1     ------> LPUART1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF8_LPUART1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* LPUART1 DMA Init */
    /* LPUART1_RX Init */
    hdma_lpuart1_rx.Instance = DMA1_Channel3;
    hdma_lpuart1_rx.Init.Request = DMA_REQUEST_LPUART1_RX;
    hdma_lpuart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_lpuart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_lpuart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_lpuart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_lpuart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_lpuart1_rx.Init.Mode = DMA_NORMAL;
    hdma_lpuart1_rx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_lpuart1_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmarx,hdma_lpuart1_rx);

    /* LPUART1 interrupt Init */
    HAL_NVIC_SetPriority(USART3_LPUART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART3_LPUART1_IRQn);
  /* USER CODE BEGIN LPUART1_MspInit 1 */

  /* USER CODE END LPUART1_MspInit 1 */
  }
  else if(uartHandle->Instance==LPUART2)
  {
  /* USER CODE BEGIN LPUART2_MspInit 0 */

  /* USER CODE END LPUART2_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPUART2;
    PeriphClkInit.Lpuart2ClockSelection = RCC_LPUART2CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* LPUART2 clock enable */
    __HAL_RCC_LPUART2_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**LPUART2 GPIO Configuration
    PC6     ------> LPUART2_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF8_LPUART2;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* LPUART2 DMA Init */
    /* LPUART2_TX Init */
    hdma_lpuart2_tx.Instance = DMA1_Channel1;
    hdma_lpuart2_tx.Init.Request = DMA_REQUEST_LPUART2_TX;
    hdma_lpuart2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_lpuart2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_lpuart2_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_lpuart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_lpuart2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_lpuart2_tx.Init.Mode = DMA_NORMAL;
    hdma_lpuart2_tx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_lpuart2_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmatx,hdma_lpuart2_tx);

    /* LPUART2_RX Init */
    hdma_lpuart2_rx.Instance = DMA1_Channel2;
    hdma_lpuart2_rx.Init.Request = DMA_REQUEST_LPUART2_RX;
    hdma_lpuart2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_lpuart2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_lpuart2_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_lpuart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_lpuart2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_lpuart2_rx.Init.Mode = DMA_NORMAL;
    hdma_lpuart2_rx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_lpuart2_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmarx,hdma_lpuart2_rx);

    /* LPUART2 interrupt Init */
    HAL_NVIC_SetPriority(USART2_LPUART2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART2_LPUART2_IRQn);
  /* USER CODE BEGIN LPUART2_MspInit 1 */

  /* USER CODE END LPUART2_MspInit 1 */
  }
  else if(uartHandle->Instance==LPUART3)
  {
  /* USER CODE BEGIN LPUART3_MspInit 0 */

  /* USER CODE END LPUART3_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPUART3;
    PeriphClkInit.Lpuart3ClockSelection = RCC_LPUART3CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* LPUART3 clock enable */
    __HAL_RCC_LPUART3_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**LPUART3 GPIO Configuration
    PC4     ------> LPUART3_TX
    PC5     ------> LPUART3_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF8_LPUART3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN LPUART3_MspInit 1 */

  /* USER CODE END LPUART3_MspInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspInit 0 */

  /* USER CODE END USART3_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART3;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* USART3 clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**USART3 GPIO Configuration
    PB8     ------> USART3_TX
    PB9     ------> USART3_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* USART3 DMA Init */
    /* USART3_RX Init */
    hdma_usart3_rx.Instance = DMA1_Channel7;
    hdma_usart3_rx.Init.Request = DMA_REQUEST_USART3_RX;
    hdma_usart3_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart3_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart3_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart3_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart3_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart3_rx.Init.Mode = DMA_NORMAL;
    hdma_usart3_rx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart3_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmarx,hdma_usart3_rx);

    /* USART3_TX Init */
    hdma_usart3_tx.Instance = DMA2_Channel1;
    hdma_usart3_tx.Init.Request = DMA_REQUEST_USART3_TX;
    hdma_usart3_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart3_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart3_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart3_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart3_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart3_tx.Init.Mode = DMA_NORMAL;
    hdma_usart3_tx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart3_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmatx,hdma_usart3_tx);

    /* USART3 interrupt Init */
    HAL_NVIC_SetPriority(USART3_LPUART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART3_LPUART1_IRQn);
  /* USER CODE BEGIN USART3_MspInit 1 */

  /* USER CODE END USART3_MspInit 1 */
  }
  else if(uartHandle->Instance==USART4)
  {
  /* USER CODE BEGIN USART4_MspInit 0 */

  /* USER CODE END USART4_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART4;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* USART4 clock enable */
    __HAL_RCC_USART4_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART4 GPIO Configuration
    PA0     ------> USART4_TX
    PA1     ------> USART4_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART4;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN USART4_MspInit 1 */

  /* USER CODE END USART4_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==LPUART1)
  {
  /* USER CODE BEGIN LPUART1_MspDeInit 0 */

  /* USER CODE END LPUART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_LPUART1_CLK_DISABLE();

    /**LPUART1 GPIO Configuration
    PC0     ------> LPUART1_RX
    PC1     ------> LPUART1_TX
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_0|GPIO_PIN_1);

    /* LPUART1 DMA DeInit */
    HAL_DMA_DeInit(uartHandle->hdmarx);

    /* LPUART1 interrupt Deinit */
  /* USER CODE BEGIN LPUART1:USART3_LPUART1_IRQn disable */
    /**
    * Uncomment the line below to disable the "USART3_LPUART1_IRQn" interrupt
    * Be aware, disabling shared interrupt may affect other IPs
    */
    /* HAL_NVIC_DisableIRQ(USART3_LPUART1_IRQn); */
  /* USER CODE END LPUART1:USART3_LPUART1_IRQn disable */

  /* USER CODE BEGIN LPUART1_MspDeInit 1 */

  /* USER CODE END LPUART1_MspDeInit 1 */
  }
  else if(uartHandle->Instance==LPUART2)
  {
  /* USER CODE BEGIN LPUART2_MspDeInit 0 */

  /* USER CODE END LPUART2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_LPUART2_CLK_DISABLE();

    /**LPUART2 GPIO Configuration
    PC6     ------> LPUART2_TX
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_6);

    /* LPUART2 DMA DeInit */
    HAL_DMA_DeInit(uartHandle->hdmatx);
    HAL_DMA_DeInit(uartHandle->hdmarx);

    /* LPUART2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART2_LPUART2_IRQn);
  /* USER CODE BEGIN LPUART2_MspDeInit 1 */

  /* USER CODE END LPUART2_MspDeInit 1 */
  }
  else if(uartHandle->Instance==LPUART3)
  {
  /* USER CODE BEGIN LPUART3_MspDeInit 0 */

  /* USER CODE END LPUART3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_LPUART3_CLK_DISABLE();

    /**LPUART3 GPIO Configuration
    PC4     ------> LPUART3_TX
    PC5     ------> LPUART3_RX
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_4|GPIO_PIN_5);

  /* USER CODE BEGIN LPUART3_MspDeInit 1 */

  /* USER CODE END LPUART3_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspDeInit 0 */

  /* USER CODE END USART3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART3_CLK_DISABLE();

    /**USART3 GPIO Configuration
    PB8     ------> USART3_TX
    PB9     ------> USART3_RX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8|GPIO_PIN_9);

    /* USART3 DMA DeInit */
    HAL_DMA_DeInit(uartHandle->hdmarx);
    HAL_DMA_DeInit(uartHandle->hdmatx);

    /* USART3 interrupt Deinit */
  /* USER CODE BEGIN USART3:USART3_LPUART1_IRQn disable */
    /**
    * Uncomment the line below to disable the "USART3_LPUART1_IRQn" interrupt
    * Be aware, disabling shared interrupt may affect other IPs
    */
    /* HAL_NVIC_DisableIRQ(USART3_LPUART1_IRQn); */
  /* USER CODE END USART3:USART3_LPUART1_IRQn disable */

  /* USER CODE BEGIN USART3_MspDeInit 1 */

  /* USER CODE END USART3_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART4)
  {
  /* USER CODE BEGIN USART4_MspDeInit 0 */

  /* USER CODE END USART4_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART4_CLK_DISABLE();

    /**USART4 GPIO Configuration
    PA0     ------> USART4_TX
    PA1     ------> USART4_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0|GPIO_PIN_1);

  /* USER CODE BEGIN USART4_MspDeInit 1 */

  /* USER CODE END USART4_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

