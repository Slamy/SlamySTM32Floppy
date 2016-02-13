/**
 * @author  Tilen Majerle
 * @email   tilen@majerle.eu
 * @website http://stm32f4-discovery.com
 * @link    http://stm32f4-discovery.com/2014/08/library-24-virtual-com-port-vcp-stm32f4xx/
 * @version v1.2
 * @ide     Keil uVision
 * @license GNU GPL v3
 * @brief   USB Virtual COM Port for STM32F4xx devices
 *	
@verbatim
   ----------------------------------------------------------------------
    Copyright (C) Tilen Majerle, 2015
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.
     
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
   ----------------------------------------------------------------------
@endverbatim
 */
#ifndef TM_USB_VCP_H
#define TM_USB_VCP_H   120

/* C++ detection */
#ifdef __cplusplus
extern C {
#endif

/**
 * @addtogroup TM_STM32F4xx_Libraries
 * @{
 */

/**
 * @defgroup TM_USB_VCP
 * @brief    USB Virtual COM Port for STM32F4xx devices - http://stm32f4-discovery.com/2014/08/library-24-virtual-com-port-vcp-stm32f4xx/
 * @{
 *
 * With this library, your STM32F4xx will be seen to your computer as Virtual COM Port (VCP).
 * To be able to work, you have to install ST's VCP Driver, from link below:
 *
 *		http://www.st.com/web/en/catalog/tools/PF257938
 *
 * This library can work in 2 ways.
 * First and default is Full-Speed mode, second option is High-Speed mode.
 * Also, different modes have different pinouts.
 *
 * In default settings, USB FS mode is selected.
 *
 * STM32F4-Discovery has USB connected to FS mode, but
 * STM32F429-Discovery has connected it to USB HS in FS mode.
 * But if you have cable, like me, USB->4wires, you can connect Data+ and Data- to any pin on Discovery board.
 * I did this, to check, if both mdoes work on bots discovery boards and yes, it worked.
 * For security reasons set 22Ohm resistors in serial to your data pins.
 *
 * USB FS MODE (micro USB connected on STM32F4 Discovery board)
 *	- This is default option and don't need any special settings.
 *
 * \par Pinout for USB FS mode
 *
@verbatim
USB			STM32F4xx
Data +		PA12
Data -		PA11
@endverbatim
 *
 * USB HS in FS mode (micro USB connected on STM32F429 Discovery board)
 *
 *	If you are working with STM32F429 Discovery board, and you want to use microUSB connector for VCP,
 *	then set define below in your defines.h file
 *
@verbatim
//Activate USB HS in FS mode
#define USE_USB_OTG_HS
@endverbatim
 *
 * \par Pinout for USB HS in FS mode
 *
@verbatim
USB			STM32F4xx
Data +		PB15
Data -		PB14
@endverbatim
 *
 *
 * \par Changelog
 *
@verbatim
 Version 1.2
  - March 08, 2015
  - Added options to get user settings from terminal
  - Baudrate, stop bits, parity, data bits.
  - Useful if you make USB->UART converter like FTDI

 Version 1.1
  - December 27, 2014
  - Added advanced functions for string operations
  - Now, Gets function will wait till buffer is full or \n is received
  - This is prevent for while loop if \n character is not received

 Version 1.0
  - First release
@endverbatim
 *
 * \par Dependencies
 *
@verbatim
 - STM32F4xx
 - STM32F4xx RCC
 - STM32F4xx GPIO
 - STM32F4xx EXTI
 - misc.h
 - defines.h
 - USB CDC DEVICE
@endverbatim
 */

#include "stm32f4xx.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_exti.h"
#include "misc.h"
#include "defines.h"
/* Parts of USB device */
#include "usbd_cdc_core.h"
#include "usb_conf.h"
#include "usbd_desc.h"
/**
 * @defgroup TM_USB_VCP_Macros
 * @brief    Library defines
 * @{
 */

/**
 * @}
 */
 
/**
 * @defgroup TM_USB_VCP_Typedefs
 * @brief    Library Typedefs
 * @{
 */

/**
 * @brief VCP Result Enumerations
 */
typedef enum {
	TM_USB_VCP_OK,                  /*!< Everything ok */
	TM_USB_VCP_ERROR,               /*!< An error occurred */
	TM_USB_VCP_RECEIVE_BUFFER_FULL, /*!< Receive buffer is full */
	TM_USB_VCP_DATA_OK,             /*!< Data OK */
	TM_USB_VCP_DATA_EMPTY,          /*!< Data empty */
	TM_USB_VCP_NOT_CONNECTED,       /*!< Not connected to PC */
	TM_USB_VCP_CONNECTED,           /*!< Connected to PC */
	TM_USB_VCP_DEVICE_SUSPENDED,    /*!< Device is suspended */
	TM_USB_VCP_DEVICE_RESUMED       /*!< Device is resumed */
} TM_USB_VCP_Result;

/**
 * @}
 */

/**
 * @defgroup TM_USB_VCP_Functions
 * @brief    Library Functions
 * @{
 */
 
/**
 * @brief  Initializes USB VCP
 * @param  None
 * @retval TM_USB_VCP_OK
 */
TM_USB_VCP_Result TM_USB_VCP_Init(void);

/**
 * @}
 */
 
/**
 * @}
 */
 
/**
 * @}
 */

/* C++ detection */
#ifdef __cplusplus
}
#endif

#endif

