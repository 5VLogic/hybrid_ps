
default: exe.ihex

# Upload:
u: exe.ihex
	avrdude -p atmega88pb -c stk500v2 -b 115200 -P usb -U flash:w:exe.ihex:i


exe.ihex: exe.elf
	avr-objcopy -O ihex -R .eeprom exe.elf exe.ihex


exe.elf: main.c ./lib/usart.h ./lib/i2c_atmega88pb.h
	avr-gcc -mmcu=atmega88pb -O1 main.c -o exe.elf


asm: main.c
	avr-gcc -mmcu=atmega88pb -S -O0 main.c -o main.asm


clean:
	rm *.hex *.ihex *.o *.asm *.elf
