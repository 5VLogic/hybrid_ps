# Hybrid Power Supply FW
This readme file contains some useful information about the uC part
of the project.

## Setting the fuse bytes
The project code is based on the assumption that the atmega88pb has
its fuse bytes set as explained in the following.

### Brown Out Detector
For some reason at power on, the supply voltage rise is not adequate
and the uC doesn't start up. To correct this, set the brown out detector
voltage to 2.7V by writing BODLEVEL1 fuse bit in the high fuse byte to 0:

### CLK Frequency
The uC's clock frequency is set to 8MHz by disabling the internal clock
divider (by 8). This is done by setting the low fuse byte as follows:

### EEPROM preservation during chip erase
To avoid the EEPROM being reset at poweroff. (Add EESAVE bit = 0 on high fuse byte.)

### Fuse Byte setting
Based on the above, the low and high fuse bytes are set with the following commands:

_avrdude -p atmega88pb -c stk500v2 -b 115200 -P usb -U lfuse:w:0xe2:m_

_avrdude -p atmega88pb -c stk500v2 -b 115200 -P usb -U hfuse:w:0xd5:m_
