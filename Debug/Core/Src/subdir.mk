################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/LIS3DSH.c \
../Core/Src/blinky.c \
../Core/Src/bsp.c \
../Core/Src/main.c \
../Core/Src/spi_manager.c \
../Core/Src/sst.c \
../Core/Src/sst_port.c \
../Core/Src/stm32f4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/system_stm32f4xx.c \
../Core/Src/tim.c 

OBJS += \
./Core/Src/LIS3DSH.o \
./Core/Src/blinky.o \
./Core/Src/bsp.o \
./Core/Src/main.o \
./Core/Src/spi_manager.o \
./Core/Src/sst.o \
./Core/Src/sst_port.o \
./Core/Src/stm32f4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/system_stm32f4xx.o \
./Core/Src/tim.o 

C_DEPS += \
./Core/Src/LIS3DSH.d \
./Core/Src/blinky.d \
./Core/Src/bsp.d \
./Core/Src/main.d \
./Core/Src/spi_manager.d \
./Core/Src/sst.d \
./Core/Src/sst_port.d \
./Core/Src/stm32f4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/system_stm32f4xx.d \
./Core/Src/tim.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -Wextra -pedantic -Wmissing-include-dirs -Wswitch-default -Wconversion -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/LIS3DSH.cyclo ./Core/Src/LIS3DSH.d ./Core/Src/LIS3DSH.o ./Core/Src/LIS3DSH.su ./Core/Src/blinky.cyclo ./Core/Src/blinky.d ./Core/Src/blinky.o ./Core/Src/blinky.su ./Core/Src/bsp.cyclo ./Core/Src/bsp.d ./Core/Src/bsp.o ./Core/Src/bsp.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/spi_manager.cyclo ./Core/Src/spi_manager.d ./Core/Src/spi_manager.o ./Core/Src/spi_manager.su ./Core/Src/sst.cyclo ./Core/Src/sst.d ./Core/Src/sst.o ./Core/Src/sst.su ./Core/Src/sst_port.cyclo ./Core/Src/sst_port.d ./Core/Src/sst_port.o ./Core/Src/sst_port.su ./Core/Src/stm32f4xx_it.cyclo ./Core/Src/stm32f4xx_it.d ./Core/Src/stm32f4xx_it.o ./Core/Src/stm32f4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/system_stm32f4xx.cyclo ./Core/Src/system_stm32f4xx.d ./Core/Src/system_stm32f4xx.o ./Core/Src/system_stm32f4xx.su ./Core/Src/tim.cyclo ./Core/Src/tim.d ./Core/Src/tim.o ./Core/Src/tim.su

.PHONY: clean-Core-2f-Src

