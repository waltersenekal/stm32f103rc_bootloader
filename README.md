# stm32f103rc_bootloader
I have an Ender 3 V2 3d printer
It runs on a STM32F103RE CPU
I was messing around with it and damaged it.
I needed to replace it and could only find a STM32F103RC cpu
It is the same CPU with just have the flash size of the STM32F103RE

I tried to find a bootloader on Creality's website and various other places on the internet, and could not find what I needed
So I wrote a new one from scratch.

This is how the bootloader work
1. On startup, it checks for a SD card is present
2. If SD card is present it will search for any file in the root directory with the *.bin extention
3. If a bin file is found it will delete the flash area in the CPU and load the new bin file
4. It will rename the the old file by adding ".old" to the back of the file name
5. It will jump to the Application address.
6. If no sd card is detected, it will jump to Application address
7. And if sd card is found without a bin file in root, it will jump to Application address.

If download of bin file is interrupted or corrupt it will fail to run the main application and it will be stuck in bootloader.
You just need to put the bin file on the SD card again and bootup the machine, it should just start over in loading the new application to the correct area.
Because of the limited space on the CPU Flash, we can not keep a 2nd image on the CPU for fallback, but this is possible on bigger flash cpus.

Limitations:
Because of the limited space for the bootloader i did not implement long file names, so Short file name (8.3 format) is used.
https://en.wikipedia.org/wiki/8.3_filename#:~:text=An%208.3%20filename%20(also%20called,for%20compatibility%20with%20legacy%20programs.
if your file name is too long e.g firmware-20210515-131802.bin , it will get trunkated to FIRMWA~1.BIN so after bootload the file will be renamed to FIRMWA~1.BIN.OLD
but if the filename is firmware.bin it will not be trunkated and after the loading it will be renamed to firmware.bin.old


So the Bootloader address is 0x8000000, that is the address where the bootloader is loaded to.
For the marlin software the start address is 0x8007000,
So that means the bootloader can not exceed 0x7000 in size.

After you compile the code make sure it does not exceed the limit.

This project is for the STM32F103RC but can be used for STM32F103RE as well

