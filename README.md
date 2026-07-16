# STM32 Infrared Morse Transceiver

A robust, Half-Duplex optical chat system built on the STM32F411 (BlackPill). This project transmits and receives text messages using Morse code over an infrared optical link, interfacing with a host PC via USB CDC (Virtual COM Port).

## System Architecture

The system bridges a Python-based PC terminal with raw infrared hardware, relying on precise hardware timers to modulate/demodulate the optical signals without blocking the main execution loop.

*   **MCU:** STM32F411CEU6 (BlackPill) running at 96 MHz (HSE + PLL).
*   **Carrier Generation:** Hardware Timer (`TIM2`) generating a 38 kHz PWM signal for the IR LED.
*   **Signal Sampling:** Hardware Timer (`TIM3`) polling the IR receiver (`PA0`) with microsecond precision.
*   **Host Communication:** USB CDC (Device FS) providing a plug-and-play Virtual COM Port interface.
*   **Optical Receiver:** TSOP38238 (or equivalent 38 kHz IR receiver).

## Key Engineering Challenges Solved

Building an optical link in the real world involves overcoming analog and hardware constraints. This firmware addresses three major engineering hurdles:

### 1. The TSOP AGC Saturation (Signal Dropping)
IR receivers like the TSOP38238 feature an Automatic Gain Control (AGC) designed to reject continuous light (like fluorescent bulbs). A standard Morse "Dash" (240ms of continuous 38kHz PWM) triggers this filter, blinding the sensor mid-transmission and corrupting the symbol.
*   **The Solution:** The firmware applies a sub-modulation layer. Instead of a continuous PWM, symbols are transmitted in rapid bursts of **2ms ON / 2ms OFF** (50% Duty Cycle). This provides enough average optical energy for detection while tricking the AGC into maintaining high sensitivity.

### 2. Optical Echoing (Half-Duplex Enforcement)
Because infrared light bounces off nearby surfaces, a transceiver's sensor can easily read its own LED pulses, creating a chaotic feedback loop (echo).
*   **The Solution:** A strict Software Half-Duplex mechanism is enforced inside the `TIM3` interrupt. When the `txState` machine is active, all reception variables and counters are zeroed out, effectively deafening the receiver while transmitting.

### 3. USB CDC Race Conditions (USBD_BUSY)
The STM32 USB CDC stack drops packets if `CDC_Transmit_FS()` is called sequentially faster than the USB peripheral can flush its buffer, causing random letters to disappear from words.
*   **The Solution:** Implemented an atomic software buffer array in the main loop. Symbols and spaces are packed into a single array and transmitted together, featuring a strict `while(CDC_Transmit_FS() == USBD_BUSY)` lock to guarantee data integrity.

## Hardware Setup

| Component | STM32F411 Pin | Description |
| :--- | :--- | :--- |
| **TSOP38238 (OUT)** | `PA0` | External interrupt / polling pin for signal reception. |
| **IR LED (Anode)** | `PA1` | `TIM2_CH2` PWM output (modulated at 38 kHz). |
| **USB-C** | `Built-in` | For power and USB CDC communication. |

## Build and Flash (Linux / Fedora)

This project uses the standard `arm-none-eabi-gcc` toolchain and CMake.

**1. Build the Firmware:**
```bash
cmake --build build/Release --clean-first
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program build/Release/stm32_semb_morse.elf verify reset exit"
```

## Usage

1. Flash the firmware onto two separate STM32F411 boards.
2. Connect both boards to the PC via USB-C data cables.
3. Position the boards so their LEDs face each other's receivers.
4. Ensure your user has permissions to read/write serial ports (`sudo chmod 666 /dev/ttyACM*`).
5. Launch the client script for each board in separate terminals:

```bash
# Terminal 1 (Board A)
python chat_morse.py /dev/ttyACM0

# Terminal 2 (Board B)
python chat_morse.py /dev/ttyACM1
```