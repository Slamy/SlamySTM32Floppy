# CMakeLists files in this project can
# refer to the root source directory of the project as ${HELLO_SOURCE_DIR} and
# to the root binary directory of the project as ${HELLO_BINARY_DIR}.

cmake_minimum_required (VERSION 2.8.11)

project (SlamySTM32Floppy C)


# -lstdc++ 

add_executable (slamyfloptest
	main.c
	../src/floppy_crc.c
	../src/floppy_mfm.c
	../src/floppy_sector.c
)

target_include_directories (slamyfloptest PUBLIC
	.
	../src/pt-1.4
	../src
	stubs
	)

