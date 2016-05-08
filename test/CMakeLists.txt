# CMakeLists files in this project can
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and
# to the root binary directory of the project as ${HELLO_BINARY_DIR}.

cmake_minimum_required (VERSION 2.8.11)

project (SlamySTM32Floppy C)

SET(CMAKE_C_FLAGS "-g -DCUNIT")
# -lstdc++ 

add_executable (slamyfloptest
	main.c
	../src_uC/floppy_crc.c
	../src_uC/floppy_mfm.c
	../src_uC/floppy_mfm_read.c
	../src_uC/floppy_gcr_read.c
	../src_uC/floppy_flux_write.c
	../src_uC/floppy_flux_read.c
	../src_uC/floppy_sector.c
	../src_uC/floppy_sector_iso.c
	../src_uC/floppy_sector_amiga.c
	../src_uC/floppy_sector_c64.c
	../src_uC/floppy_sector_raw.c
	../src_uC/floppy_settings.c
)

target_include_directories (slamyfloptest PUBLIC
	.
	../src_uC
	stubs
	)


add_executable (rawVerifyTest
	rawVerifyTest.c
)
