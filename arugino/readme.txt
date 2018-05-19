V7:
- Different millis() approach as LCD did not work in V6

V6:
- LCD added

V5:
- RTC added: set RTC and print time
- Updated button print
- Updated memory write to include time stamp

V4:
- EEPROM memory for sensor values (and later time)
- Interrupt by button press
- Replaced Float switch pin from 2 to 4 because external interrupt 0 requires pin 2

V3:
- Added moisture sensor check

V2:
- Removed LED lighting pin (attached LED directly to float switch)
- Bypassed moisture sensor with (sense value > 0) check
- Added long wait by millis()

V1:
- debug version

V0:
- initial version