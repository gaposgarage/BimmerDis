# BimmerDis

**BimmerDis** is a disassembler for the _"Beschreibungssprache für Steuergeräte"_, BMW’s proprietary description language used to define diagnostic routines for control units within the EDIABAS diagnostic system.
Originally designed to interpret ECU description files (often distributed as PRG or GRP files), BimmerDis decrypts these files by XOR‑ing each byte with 0xF7 and produces a human‑readable disassembly in a standardized output format (with a `.b1v` or `.b1g` extension).
This output provides detailed insight into the diagnostic jobs defined in the file, such as sensor readings, error memory, and self‑tests.

## Features

- **Decryption:** Automatically decrypts binary BEST files by XOR‑ing every byte with 0xF7.
- **Disassembly:** Transforms the proprietary binary code into a human‑readable assembly-like format.
- **Output Formats:** Generates output files with `.b1v` (or `.b1g` for group files) extensions that preserve the structure and details of the original ECU description.
- **Diagnostic Job Details:** Clearly lists job names, comments, and instruction sequences for further analysis.
- **Integration:** Designed in C++ for high performance and easy integration into diagnostic workflows.

## Usage

Run the executable from a command prompt (or terminal), passing the path to a BEST file (or its directory) as an argument:

```bash
BimmerDis.exe "C:\path\to\your\BESTfile.prg"
```

When executed, BimmerDis will:

1. Open the specified file.
2. Decrypt its content by XOR‑ing every byte with 0xF7.
3. Disassemble the decrypted binary into human‑readable instructions.
4. Save the output in the same directory as the input file with the appropriate extension (`.b1v` or `.b1g`).

## Background

EDIABAS (Electronic Diagnostic Basic System) is a diagnostic platform developed by Softing on behalf of BMW. It utilizes the "Beschreibungssprache für Steuergeräte" – a specialized language designed for defining diagnostic routines, sensor measurements, and error memory checks in modern BMW vehicles.
BimmerDis helps engineers, developers, and enthusiasts to reverse engineer and better understand these ECU description files.

## Credits

BimmerDis builds on community efforts and is part of a larger collection of diagnostic tools.
For further details, additional tools, and background on the project, please refer to:  
[ediabaslib on GitHub](https://github.com/uholeschak/ediabaslib/tree/master/Tools/BESTDIS)

---