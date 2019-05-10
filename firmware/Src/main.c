/*

Copyright 2019 Ravikiran Bukkasagara <contact@ravikiranb.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include "main.h"
#include "usb_device.h"
#include "DAP.h"
#include "DAP_config.h"
#include "cmsis_compiler.h"
#include "usbd_custom_hid_if.h"

UART_HandleTypeDef huart2;

extern USBD_HandleTypeDef hUsbDeviceFS;
uint8_t  USB_DAP_Requests [DAP_PACKET_COUNT][DAP_PACKET_SIZE];  
uint8_t  USB_DAP_Responses[DAP_PACKET_COUNT][DAP_PACKET_SIZE];

static volatile uint32_t CommonIndex;  //Current "Request - Response" being processed index linked together.
static volatile uint32_t ResponseIndexSend;
static volatile uint32_t ResponsePendingCount;

volatile uint32_t RequestPendingCount;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

int main(void)
{
  USBD_CUSTOM_HID_HandleTypeDef *hhid;
  
	HAL_Init();

  SystemClock_Config();

  CommonIndex = 0;
	ResponseIndexSend = 0;
	ResponsePendingCount = 0;
	RequestPendingCount = 0;
	
	MX_GPIO_Init();
	
	// SWO trace not implemented
	//MX_USART2_UART_Init();
	
  MX_USB_DEVICE_Init();
	
  DAP_Setup();
  
  while (1) 
	{
    /* 
			Cortex-M0 does not support synchronizing intstructions.
			RequestPendingCount variable has a potential racing condition.
			Disabling interrupts while update is a choice.
			Systick interrupt will cause wakeup in case of out of sync between
			USB interrupt and WFI call here.
		*/
		
		if (RequestPendingCount == 0) {
			__WFI();
		}
		
		while ((RequestPendingCount > 0) && (ResponsePendingCount < DAP_PACKET_COUNT)) {
			
			// Handle Queue Commands??
      if (USB_DAP_Requests[CommonIndex][0] == ID_DAP_QueueCommands) {
				USB_DAP_Requests[CommonIndex][0] = ID_DAP_ExecuteCommands;
      }
			
			DAP_ExecuteCommand(USB_DAP_Requests[CommonIndex], USB_DAP_Responses[CommonIndex]);
			
			RequestPendingCount--;
			ResponsePendingCount++;
			CommonIndex++;
			if (CommonIndex >= DAP_PACKET_COUNT) {
				CommonIndex = 0;
			}
		}
		
		while (ResponsePendingCount > 0) {
			
			hhid = (USBD_CUSTOM_HID_HandleTypeDef*) hUsbDeviceFS.pClassData;
			
			if (hhid->state == CUSTOM_HID_IDLE) {
				USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, USB_DAP_Responses[ResponseIndexSend], DAP_PACKET_SIZE);
				ResponseIndexSend++;
				if (ResponseIndexSend >= DAP_PACKET_COUNT) {
					ResponseIndexSend = 0;
				}
				ResponsePendingCount--;
			}
		}
	}
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInit = {0};
  RCC_ClkInitTypeDef RCC_ClkInit = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInit.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInit.HSEState = RCC_HSE_ON;
  RCC_OscInit.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInit.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInit.PLL.PLLMUL = RCC_PLL_MUL6;
  RCC_OscInit.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInit) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInit.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInit.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInit.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInit.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInit, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
  /** Enables the Clock Security System 
  */
  HAL_RCC_EnableCSS();
}

#if 0
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 38400;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}
#endif

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_Init = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pins : PA0 PA1 PA4 PA5 
                           PA6 PA7 */
  GPIO_Init.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5 
                          |GPIO_PIN_6|GPIO_PIN_7;
  GPIO_Init.Mode = GPIO_MODE_INPUT;
  GPIO_Init.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_Init);

  /*Configure GPIO pin : PB1 */
  GPIO_Init.Pin = GPIO_PIN_1;
  GPIO_Init.Mode = GPIO_MODE_INPUT;
  GPIO_Init.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_Init);
}

void Error_Handler(void)
{
  while(1);
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(char *file, uint32_t line)
{ 
	while(1);
}
#endif /* USE_FULL_ASSERT */