################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/MassStorage/usbhost_ms.c 

OBJS += \
./src/MassStorage/usbhost_ms.o 

C_DEPS += \
./src/MassStorage/usbhost_ms.d 


# Each subdirectory must supply rules for building sources it contributes
src/MassStorage/%.o: ../src/MassStorage/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__USE_CMSIS=CMSISv1p30_LPC17xx -D__CODE_RED -D__NEWLIB__ -I"C:\Users\247644\Desktop\EmbeddedSystems\SysWbud\Lib_CMSISv1p30_LPC17xx\inc" -I"C:\Users\247644\Desktop\EmbeddedSystems\SysWbud\usbhostlite\src\Include" -I"C:\Users\247644\Desktop\EmbeddedSystems\SysWbud\usbhostlite\src" -O0 -g3 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fmerge-constants -fmacro-prefix-map="../$(@D)/"=. -mcpu=cortex-m3 -mthumb -D__NEWLIB__ -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


