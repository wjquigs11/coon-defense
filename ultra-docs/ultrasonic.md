<!-- HTML tables preserve the scanned leaflet's approximate two-column layout in Markdown viewers that support inline HTML. Keep the adjacent images directory with this file. -->

<table>
<tr>
<td width="50%" valign="top">
<h1>ULTRASONIC DISTANCE SENSOR</h1>
<h2 align="center">HC-SR04 Compatible</h2>
<p align="center"><strong>4 Interface Modes · High Accuracy<br>2.8V–5.5V Wide Voltage · Easy to Integrate</strong></p>
<h3>INTERFACE DEFINITION</h3>
<table>
<tr><th>No.</th><th>Interface</th><th>Description</th></tr>
<tr><td>1</td><td>Vcc</td><td>Power Supply</td></tr>
<tr><td>2</td><td>Trig/Rx/SCL</td><td>GPIO Mode: <b>Trig</b> — Trigger Signal<br>UART Mode: <b>Rx</b> — Receive Signal<br>IIC Mode: <b>SCL</b> — Clock Signal</td></tr>
<tr><td>3</td><td>Echo/Tx/SDA</td><td>GPIO Mode: <b>Echo</b> — Echo Signal<br>UART Mode: <b>Tx</b> — Transmit Signal<br>IIC Mode: <b>SDA</b> — Data Signal</td></tr>
<tr><td>4</td><td>Gnd</td><td>Ground</td></tr>
</table>
<h3>MODE SELECTION</h3>
<table>
<tr><th>No.</th><th>Mode</th><th colspan="2">R4/R5 Resistor Configuration</th><th></th></tr>
<tr><td>1</td><td>GPIO</td><td>R4=NC</td><td>R5=NC</td><td>(Default)</td></tr>
<tr><td>2</td><td>UART</td><td>R4=NC</td><td>R5=10K</td><td></td></tr>
<tr><td>3</td><td>IIC</td><td>R4=10K</td><td>R5=NC</td><td></td></tr>
</table>
</td>
<td width="50%" valign="top">
<p align="center"><img src="images/sensor-rear.png" width="390" alt="Rear of the sensor board, marked RCWL-9610 design"></p>
<p align="center"><img src="images/sensor-front.png" width="390" alt="Front of the HC-SR04 sensor with two ultrasonic transducers"></p>
<table>
<tr><td><strong>Four Interface Modes:</strong><br>1. GPIO<br>3. IIC<br>3. UART Serial Port<br>4. 1-WIRE Single Bus</td><td><strong>2.8V–5.5V</strong><br><br><strong>Default Compatible<br>with HC-SR04</strong></td></tr>
</table>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<h3>1. GPIO Mode</h3>
<p><img src="images/gpio-timing-diagram.png" width="490" alt="GPIO ultrasonic timing diagram: trigger over 10 µs, minimum 200 ms cycle, eight pulses at 40 kHz, and echo pulse width proportional to distance"></p>
<p>This mode is the same as HC-SR04. The external MCU sends a trigger pulse of more than 10us to the Trig pin; the module will output a high-level pulse proportional to the distance, and the pulse width T can be used to calculate the distance:</p>
<p align="center"><strong>Distance = T * C / 2</strong> &nbsp; (C is the speed of sound)</p>
<p align="center">Speed of sound formula: <strong>c = (331.45 + 0.61 * t / °C) m/s</strong><br><em>(where 330.45 is at 0°C)</em></p>
<p align="center">Sound speed at 0°C: &nbsp; 330.45 m/s<br>Sound speed at 20°C: &nbsp; 342.62 m/s<br>Sound speed at 40°C: &nbsp; 354.85 m/s</p>
<p>The error of sound speed from 0°C to 40°C is about ±7%.<br>In practical applications, temperature compensation should be considered if high accuracy is required.</p>
</td>
<td width="50%" valign="top">
<h3>2. UART Mode</h3>
<p><strong>UART Mode Baud Rate Setting: &nbsp; 9600 N 1</strong></p>
<table>
<tr><th>Command</th><th>Return Value</th><th>Description</th></tr>
<tr><td>0xA0</td><td>BYTE_H<br>BYTE_M<br>BYTE_L</td><td>Distance (mm):<br><code>((BYTE_H &lt;&lt; 16) + (BYTE_M &lt;&lt; 8) + BYTE_L) / 1000</code><br>Unit: mm</td></tr>
<tr><td>0xF1</td><td>—</td><td>Company and version information</td></tr>
</table>
<p>Connect the serial port. After the external MCU or PC sends command 0xA0, the module will return 3 distance data bytes: BYTE_H, BYTE_M, and BYTE_L.</p>
<p>Distance calculation (unit: mm):</p>
<p><strong>Distance = ((BYTE_H &lt;&lt; 16) + (BYTE_M &lt;&lt; 8) + BYTE_L) / 1000</strong></p>
<br>
<p align="center"><strong>✦ Fortune Cookie: May your logic never fail, and may<br>you always navigate around life's obstacles. ✦</strong></p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<h3>3. IIC Mode</h3>
<p><strong>IIC Address: &nbsp; 0x57<br>IIC Data Format:</strong></p>
<p><img src="images/iic-write-read-diagrams.png" width="490" alt="IIC write and read data diagrams: start condition, slave address, write or read bit, ACK, data transfers, final ACK or NAK, and stop condition"></p>
<p><strong>Command Format:</strong></p>
<table>
<tr><th>Address</th><th>Command</th><th>Return Value</th><th>Description</th></tr>
<tr><td>Write Address<br>0xAE</td><td>0x01</td><td>—</td><td>Start measurement command</td></tr>
<tr><td>Read Address<br>0xAF</td><td></td><td>BYTE_H<br>BYTE_M<br>BYTE_L</td><td>Distance (mm):<br><code>((BYTE_H &lt;&lt; 16) + (BYTE_M &lt;&lt; 8) + BYTE_L) / 1000</code><br>Unit: mm</td></tr>
</table>
<p>Write command 0x01 to start measurement, then wait for at least 200ms (maximum measurement time of the module).<br>Then read 3 distance data bytes, BYTE_H, BYTE_M, and BYTE_L.</p>
<p>Distance calculation (unit: mm):</p>
<p><strong>Distance = ((BYTE_H &lt;&lt; 16) + (BYTE_M &lt;&lt; 8) + BYTE_L) / 1000</strong></p>
</td>
<td width="50%" valign="top">
<h3>4. 1-WIRE Bus Mode</h3>
<p><img src="images/one-wire-timing-diagram.png" width="490" alt="1-WIRE ultrasonic timing diagram showing MCU output and input states, a 10 µs TTL trigger, customizable 50 ms interval, eight pulses at 40 kHz, and the echo pulse"></p>
<p>The external MCU initializes the module as output, and the module I/O pin is held high for more than 10us. after the echo space signal is output, the MCU sets the pin as input mode and waits for the module to output a high-level pulse proportional to distance; after measurement is completed, the MCU sets the pin back to output mode for the next measurement. The speed of sound can be calculated from the pulse width “T” as shown below:</p>
<p align="center"><strong>Distance = T * C / 2</strong> &nbsp; (C is the speed of sound)</p>
<p align="center">Speed of sound formula: <strong>c = (331.45 + 0.61 * t / °C) m/s</strong><br><em>(where 330.45 is at 0°C)</em></p>
<p align="center">Sound speed at 0°C: &nbsp; 330.45 m/s<br>Sound speed at 20°C: &nbsp; 342.62 m/s<br>Sound speed at 40°C: &nbsp; 354.85 m/s</p>
</td>
</tr>
</table>

---

*Transcribed from the scanned leaflet. Original numbering and numerical inconsistencies are preserved. The two-column layout requires a Markdown viewer with HTML table support.*

[Original full-page scan](images/page-001-image-001.jpg) · [Plain-text transcription](text.txt)
