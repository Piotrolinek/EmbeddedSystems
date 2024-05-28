################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/Uart/subdir.mk
-include src/MassStorage/subdir.mk
-include src/Main/subdir.mk
-include src/Host/subdir.mk
-include src/Fat/subdir.mk
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
all: usbhostlite.axf

# Tool invocations
usbhostlite.axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: MCU Linker'
	arm-none-eabi-gcc -nostdlib -L"C:\Users\247644\Desktop\EmbeddedSystems\SysWbud\Lib_CMSISv1p30_LPC17xx\Debug" -Xlinker --gc-sections -Xlinker -Map=usbhostlite.map -mcpu=cortex-m3 -mthumb -T "rdb1768cmsis_uart_Debug.ld" -o "usbhostlite.axf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) usbhostlite.axf
	-@echo ' '

post-build:
	-@echo 'Performing post-build steps'
	-arm-none-eabi-size usbhostlite.axf; # arm-none-eabi-objdump -h -S usbhostlite.axf >usbhostlite.lss
	-@echo ' '

.PHONY: all clean dependents post-build

-include ../makefile.targets
