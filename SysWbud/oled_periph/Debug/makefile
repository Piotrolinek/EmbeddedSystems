################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: oled_periph.axf

# Tool invocations
oled_periph.axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: MCU Linker'
	arm-none-eabi-gcc -nostdlib -L"C:\Users\247810\Downloads\sysWbud24_05\Lib_CMSISv1p30_LPC17xx\Debug" -L"C:\Users\247810\Downloads\sysWbud24_05\Lib_EaBaseBoard\Debug" -L"C:\Users\247810\Downloads\sysWbud24_05\Lib_MCU\Debug" -Xlinker --gc-sections -Xlinker -Map=oled_periph.map -mcpu=cortex-m3 -mthumb -T "rdb1768cmsis_uart_Debug.ld" -o "oled_periph.axf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) oled_periph.axf
	-@echo ' '

post-build:
	-@echo 'Performing post-build steps'
	-arm-none-eabi-size oled_periph.axf; # arm-none-eabi-objdump -h -S oled_periph.axf >oled_periph.lss
	-@echo ' '

.PHONY: all clean dependents post-build

-include ../makefile.targets
