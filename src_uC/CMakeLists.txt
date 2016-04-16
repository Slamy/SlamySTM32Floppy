# CMakeLists files in this project can
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and
# to the root binary directory of the project as ${HELLO_BINARY_DIR}.

cmake_minimum_required (VERSION 2.8.11)

project (SlamySTM32Floppy C ASM)


SET(CMAKE_C_COMPILER   arm-none-eabi-gcc)
SET(CMAKE_ASM_COMPILER   arm-none-eabi-gcc)
SET(CMAKE_EXE_LINKER_FLAGS    "-T stm32_flash.ld -lm" )
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER>  <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET>")


set(CROSS_COMPILE arm-none-eabi- )

SET(CMAKE_CXX_COMPILER arm-none-eabi-g++)

SET(CMAKE_C_FLAGS "-mcpu=cortex-m4 -mthumb -g -Wall -O2 -DUSE_STDPERIPH_DRIVER -DUSE_STM32_DISCOVERY")



# -lstdc++ 

add_executable (slamyflop
	CMSIS/system_stm32f4xx.c
	CMSIS/startup_stm32f4xx.s
	main.c
	floppy_indexSim.c
	floppy_crc.c
	floppy_mfm.c
	floppy_mfm_read.c
	floppy_gcr_read.c
	floppy_flux_write.c
	floppy_flux_read.c
	floppy_sector.c
	floppy_sector_iso.c
	floppy_sector_amiga.c
	floppy_sector_c64.c
	floppy_sector_raw.c
	floppy_control.c
	floppy_settings.c
	libcsupport.c
	Utilities/stm32f4_discovery.c
	STM32F4xx_StdPeriph_Driver/src/stm32f4xx_rcc.c
	STM32F4xx_StdPeriph_Driver/src/stm32f4xx_gpio.c
	STM32F4xx_StdPeriph_Driver/src/stm32f4xx_exti.c
	STM32F4xx_StdPeriph_Driver/src/stm32f4xx_tim.c
	STM32F4xx_StdPeriph_Driver/src/misc.c
	STM32F4xx_StdPeriph_Driver/src/stm32f4xx_syscfg.c
	Tilzor/usb_cdc_device/usb_bsp.c
	Tilzor/usb_cdc_device/usb_core.c
	Tilzor/usb_cdc_device/usb_dcd.c
	Tilzor/usb_cdc_device/usbd_cdc_core.c
	Tilzor/usb_cdc_device/usb_dcd_int.c
	Tilzor/usb_cdc_device/usbd_core.c
	Tilzor/usb_cdc_device/usbd_desc.c
	Tilzor/usb_cdc_device/usbd_ioreq.c
	Tilzor/usb_cdc_device/usbd_req.c
	Tilzor/usb_cdc_device/usbd_usr.c
	Tilzor/tm_stm32f4_usb_vcp.c
	stm32f4xx_it.c
)

target_link_libraries (slamyflop LINK_PUBLIC Hello)

target_include_directories (slamyflop PUBLIC
	CMSIS
	.
	STM32F4xx_StdPeriph_Driver/inc/
	Utilities
	pt-1.4
	Tilzor
	Tilzor/usb_cdc_device/
	)

add_custom_command(OUTPUT slamyflop.bin
               COMMAND arm-none-eabi-objcopy -O binary slamyflop slamyflop.bin
               DEPENDS slamyflop
               COMMENT "elf -> bin ...")
               
add_custom_target(flashbin DEPENDS slamyflop.bin)



add_custom_command(OUTPUT doFlash
               COMMAND st-flash write slamyflop.bin 0x8000000
               DEPENDS slamyflop.bin
               COMMENT "flashing ...")
               
add_custom_target(flash DEPENDS doFlash)






