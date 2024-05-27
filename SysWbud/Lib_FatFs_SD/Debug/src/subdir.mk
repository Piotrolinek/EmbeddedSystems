################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/ff.c \
../src/mmc.c 

OBJS += \
./src/ff.o \
./src/mmc.o 

C_DEPS += \
./src/ff.d \
./src/mmc.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__CODE_RED -D__REDLIB__ -I"C:\Users\247810\Downloads\sysWbud24_05\Lib_CMSISv1p30_LPC17xx\inc" -I"C:\Users\247810\Downloads\sysWbud24_05\Lib_FatFs_SD\inc" -I"C:\Users\247810\Downloads\sysWbud24_05\Lib_MCU\inc" -O0 -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fmerge-constants -fmacro-prefix-map="../$(@D)/"=. -mcpu=cortex-m3 -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


