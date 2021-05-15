/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Ender3 V2 with STM32F103RC cpu bootloader
 ******************************************************************************
 *
 * Free to use at own risk
 * by Walter Senekal
 *
 * V1.0
 * 14 May 2021
 * First release
 *
 * USAGE:
 * place the bin file in root of SD card (must have extension .bin)
 * the first bin file that it finds will be used in the root folder
 * it will rename the file to bin.old
 * this will prevent the bootloader from installing it every time the cpu restarts
 * e.g foo.bin will become foo.bin.old
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BOOTLOADER_SIZE 0x7000U //must be in increments of FLASH_PAGE_SIZE
#define MAIN_APP_START_ADDR ( FLASH_BASE + BOOTLOADER_SIZE )//0x8007000U
#define FLASH_PAGES_PER_BANK 256
#define FLASH_PAGES_FOR_BOATLOADER (BOOTLOADER_SIZE / FLASH_PAGE_SIZE) //14

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SD_HandleTypeDef hsd;
uint8_t sd_init_state = MSD_ERROR;
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void
SystemClock_Config(void);
static void
MX_GPIO_Init(void);
static void
MX_SDIO_SD_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

bool
EraseMainAppArea(void)
{
  bool bRetVal = true;
  FLASH_EraseInitTypeDef pEraseInit;
  uint32_t PageError;
  //Delete From Bank1
  pEraseInit.Banks = FLASH_BANK_1;
  //calculate how many pages from Bank1 to erase
  pEraseInit.NbPages = FLASH_PAGES_PER_BANK - FLASH_PAGES_FOR_BOATLOADER;
  pEraseInit.PageAddress = MAIN_APP_START_ADDR;
  pEraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);
  HAL_FLASHEx_Erase(&pEraseInit, &PageError);
  HAL_FLASH_Lock();
  return bRetVal;
}

bool
FindNewFirmware(char *filename)
{
  static FILINFO fno;
  bool bRetVal = false;
  FRESULT res;
  DIR dir;
  char path[256];
  memset(path, 0, 256);
  strcpy(path, SDPath);

  res = f_opendir(&dir, path); /* Open the directory */
  if (res == FR_OK)
  {
    for (;;)
    {
      res = f_readdir(&dir, &fno); /* Read a directory item */
      if (res != FR_OK || fno.fname[0] == 0)
        break; /* Break on error or end of dir */
      if ((fno.fattrib & AM_ARC) && (strlen(fno.fname) > 5) && ((strncmp(&fno.fname[strlen(fno.fname) - 4], ".bin", 4) == 0) || (strncmp(&fno.fname[strlen(fno.fname) - 4], ".BIN", 4) == 0)))
      { /* It is a .bin file */
        printf("/%s", fno.fname);
        strcpy(filename, fno.fname);
        bRetVal = true;
        break;
      }
    }
    f_closedir(&dir);
  }

  return bRetVal;
}

bool
FlashWrite(uint32_t address,
           uint64_t data)
{
  bool bRetVal = true;
  bRetVal = bRetVal && (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, data) == HAL_OK);
  bRetVal = bRetVal && (*(uint64_t*) address == data);
  return bRetVal;
}

bool
RenameFile(char *filename)
{
  char new_filename[128];
  strcpy(new_filename, filename);
  strcat(new_filename, ".old");
  return (f_rename(filename, new_filename) == FR_OK);
}

bool
LoadNewFirmware(char *filename)
{
  FIL BinFile;
  FRESULT fr;
  bool bRetVal = true;

  //Open the Bin file to be loaded
  bRetVal = bRetVal && (f_open(&BinFile, filename, FA_READ) == FR_OK);
  //Clear the User Application space, if file was loaded up
  bRetVal = bRetVal && EraseMainAppArea();
  if (bRetVal)
  {
    uint32_t flash_ptr = MAIN_APP_START_ADDR;
    uint64_t data;
    UINT num = 0;
    /* Unlock flash */
    HAL_FLASH_Unlock();
    do
    {
      //lets start with empty data
      data = 0xFFFFFFFFFFFFFFFF;
      fr = f_read(&BinFile, &data, 8, &num);
      if (num)
      {
        bRetVal = FlashWrite(flash_ptr, data);
        flash_ptr += 8;
      }
    }
    while ((fr == FR_OK) && (num > 0) && bRetVal);
    HAL_FLASH_Lock();
    f_close(&BinFile);
    bRetVal = bRetVal && RenameFile(filename);
  }

  return bRetVal;
}

void
StartMainApplication()
{
  void (*app_reset_handler)(void);
  uint32_t msp_value = *(volatile uint32_t*) MAIN_APP_START_ADDR;
  __disable_irq();
  __set_MSP(msp_value);
  uint32_t resethandler_address = *(volatile uint32_t*) (MAIN_APP_START_ADDR + 4);
  app_reset_handler = (void*) resethandler_address;
  app_reset_handler();
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int
main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SDIO_SD_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */

  //if SD card is initualized, we will check for firmware file
  if (sd_init_state == MSD_OK)
  {
    bool bAllGood = true;
    char filename[128];
    //Mount the SD card
    bAllGood = bAllGood && (f_mount(&SDFatFS, (TCHAR const*) SDPath, 1) == FR_OK);
    //if SD card is mounted, check for new fw file
    bAllGood = bAllGood && FindNewFirmware(filename);
    //If we have new Firmware, install it
    bAllGood = bAllGood && LoadNewFirmware(filename);
    //Be Nice and unmount SD card
    f_mount(&SDFatFS, (TCHAR const*) NULL, 0);
  }
  //Jump to main application
  StartMainApplication();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    //We will never get here
    __NOP();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void
SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct =
  { 0 };
  RCC_ClkInitTypeDef RCC_ClkInitStruct =
  { 0 };

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
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
  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
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
 * @brief SDIO Initialization Function
 * @param None
 * @retval None
 */
static void
MX_SDIO_SD_Init(void)
{

  /* USER CODE BEGIN SDIO_Init 0 */

  /* USER CODE END SDIO_Init 0 */

  /* USER CODE BEGIN SDIO_Init 1 */

  /* USER CODE END SDIO_Init 1 */
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_ENABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_ENABLE;
  hsd.Init.ClockDiv = 36;
  /* USER CODE BEGIN SDIO_Init 2 */
  //Set the sd_init_state, it will indicate if SD card present
  sd_init_state = BSP_SD_Init();
  /* USER CODE END SDIO_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void
MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct =
  { 0 };

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void
Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
