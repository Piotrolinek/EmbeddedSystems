################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/cr_startup_lpc17.c \
../src/down.c \
../src/main.c \
../src/sound_8k.c \
../src/up.c 

OBJS += \
./src/cr_startup_lpc17.o \
./src/down.o \
./src/main.o \
./src/sound_8k.o \
./src/up.o 

C_DEPS += \
./src/cr_startup_lpc17.d \
./src/down.d \
./src/main.d \
./src/sound_8k.d \
./src/up.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__USE_CMSIS=CMSISv1p30_LPC17xx -D__CODE_RED -D__NEWLIB__ -I"C:\Users\247810\Desktop\EmbeddedSystems\demo\Lib_CMSISv1p30_LPC17xx\inc" -I"C:\Users\247810\Desktop\EmbeddedSystems\demo\Lib_EaBaseBoard\inc" -I"C:\Users\247810\Desktop\EmbeddedSystems\demo\Lib_MCU\inc" -O0 -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fmerge-constants -fmacro-prefix-map="../$(@D)/"=. -mcpu=cortex-m3 -mthumb -D__NEWLIB__ -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


