################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Sources/FIFO.c \
../Sources/Flash.c \
../Sources/LEDs.c \
../Sources/UART.c \
../Sources/main.c \
../Sources/packet.c 

OBJS += \
./Sources/FIFO.o \
./Sources/Flash.o \
./Sources/LEDs.o \
./Sources/UART.o \
./Sources/main.o \
./Sources/packet.o 

C_DEPS += \
./Sources/FIFO.d \
./Sources/Flash.d \
./Sources/LEDs.d \
./Sources/UART.d \
./Sources/main.d \
./Sources/packet.d 


# Each subdirectory must supply rules for building sources it contributes
Sources/%.o: ../Sources/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g3 -I"C:/Users/11840954/Desktop/Lab2/Static_Code/PDD" -I"C:/Users/11840954/Desktop/Lab2/Static_Code/IO_Map" -I"C:/Users/11840954/Desktop/Lab2/Sources" -I"C:/Users/11840954/Desktop/Lab2/Generated_Code" -std=c99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


