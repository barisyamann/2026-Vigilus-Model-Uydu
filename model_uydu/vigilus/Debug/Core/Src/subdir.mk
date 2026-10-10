################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/SensorManager.c \
../Core/Src/aras.c \
../Core/Src/bmp388.c \
../Core/Src/buzzer.c \
../Core/Src/command.c \
../Core/Src/esp32cam.c \
../Core/Src/flight_fsm.c \
../Core/Src/gps.c \
../Core/Src/ina226.c \
../Core/Src/logger.c \
../Core/Src/lora_iot.c \
../Core/Src/main.c \
../Core/Src/motor.c \
../Core/Src/mpu9250.c \
../Core/Src/safety.c \
../Core/Src/sd_spi.c \
../Core/Src/servo.c \
../Core/Src/stm32f4xx_hal_msp.c \
../Core/Src/stm32f4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f4xx.c \
../Core/Src/telemetry.c \
../Core/Src/xbee_api.c 

OBJS += \
./Core/Src/SensorManager.o \
./Core/Src/aras.o \
./Core/Src/bmp388.o \
./Core/Src/buzzer.o \
./Core/Src/command.o \
./Core/Src/esp32cam.o \
./Core/Src/flight_fsm.o \
./Core/Src/gps.o \
./Core/Src/ina226.o \
./Core/Src/logger.o \
./Core/Src/lora_iot.o \
./Core/Src/main.o \
./Core/Src/motor.o \
./Core/Src/mpu9250.o \
./Core/Src/safety.o \
./Core/Src/sd_spi.o \
./Core/Src/servo.o \
./Core/Src/stm32f4xx_hal_msp.o \
./Core/Src/stm32f4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f4xx.o \
./Core/Src/telemetry.o \
./Core/Src/xbee_api.o 

C_DEPS += \
./Core/Src/SensorManager.d \
./Core/Src/aras.d \
./Core/Src/bmp388.d \
./Core/Src/buzzer.d \
./Core/Src/command.d \
./Core/Src/esp32cam.d \
./Core/Src/flight_fsm.d \
./Core/Src/gps.d \
./Core/Src/ina226.d \
./Core/Src/logger.d \
./Core/Src/lora_iot.d \
./Core/Src/main.d \
./Core/Src/motor.d \
./Core/Src/mpu9250.d \
./Core/Src/safety.d \
./Core/Src/sd_spi.d \
./Core/Src/servo.d \
./Core/Src/stm32f4xx_hal_msp.d \
./Core/Src/stm32f4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f4xx.d \
./Core/Src/telemetry.d \
./Core/Src/xbee_api.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../Middlewares/Third_Party/FatFs/src -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/SensorManager.cyclo ./Core/Src/SensorManager.d ./Core/Src/SensorManager.o ./Core/Src/SensorManager.su ./Core/Src/aras.cyclo ./Core/Src/aras.d ./Core/Src/aras.o ./Core/Src/aras.su ./Core/Src/bmp388.cyclo ./Core/Src/bmp388.d ./Core/Src/bmp388.o ./Core/Src/bmp388.su ./Core/Src/buzzer.cyclo ./Core/Src/buzzer.d ./Core/Src/buzzer.o ./Core/Src/buzzer.su ./Core/Src/command.cyclo ./Core/Src/command.d ./Core/Src/command.o ./Core/Src/command.su ./Core/Src/esp32cam.cyclo ./Core/Src/esp32cam.d ./Core/Src/esp32cam.o ./Core/Src/esp32cam.su ./Core/Src/flight_fsm.cyclo ./Core/Src/flight_fsm.d ./Core/Src/flight_fsm.o ./Core/Src/flight_fsm.su ./Core/Src/gps.cyclo ./Core/Src/gps.d ./Core/Src/gps.o ./Core/Src/gps.su ./Core/Src/ina226.cyclo ./Core/Src/ina226.d ./Core/Src/ina226.o ./Core/Src/ina226.su ./Core/Src/logger.cyclo ./Core/Src/logger.d ./Core/Src/logger.o ./Core/Src/logger.su ./Core/Src/lora_iot.cyclo ./Core/Src/lora_iot.d ./Core/Src/lora_iot.o ./Core/Src/lora_iot.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/motor.cyclo ./Core/Src/motor.d ./Core/Src/motor.o ./Core/Src/motor.su ./Core/Src/mpu9250.cyclo ./Core/Src/mpu9250.d ./Core/Src/mpu9250.o ./Core/Src/mpu9250.su ./Core/Src/safety.cyclo ./Core/Src/safety.d ./Core/Src/safety.o ./Core/Src/safety.su ./Core/Src/sd_spi.cyclo ./Core/Src/sd_spi.d ./Core/Src/sd_spi.o ./Core/Src/sd_spi.su ./Core/Src/servo.cyclo ./Core/Src/servo.d ./Core/Src/servo.o ./Core/Src/servo.su ./Core/Src/stm32f4xx_hal_msp.cyclo ./Core/Src/stm32f4xx_hal_msp.d ./Core/Src/stm32f4xx_hal_msp.o ./Core/Src/stm32f4xx_hal_msp.su ./Core/Src/stm32f4xx_it.cyclo ./Core/Src/stm32f4xx_it.d ./Core/Src/stm32f4xx_it.o ./Core/Src/stm32f4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f4xx.cyclo ./Core/Src/system_stm32f4xx.d ./Core/Src/system_stm32f4xx.o ./Core/Src/system_stm32f4xx.su ./Core/Src/telemetry.cyclo ./Core/Src/telemetry.d ./Core/Src/telemetry.o ./Core/Src/telemetry.su ./Core/Src/xbee_api.cyclo ./Core/Src/xbee_api.d ./Core/Src/xbee_api.o ./Core/Src/xbee_api.su

.PHONY: clean-Core-2f-Src

