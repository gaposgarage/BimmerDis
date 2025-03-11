# BimmerDis

**BimmerDis** is a disassembler for the _"Beschreibungssprache für Steuergeräte"_ – BMW’s proprietary description language for control units used within the EDIABAS diagnostic system. It reads binary BEST files (commonly distributed as PRG files), decrypts their content (by XOR‑ing each byte with 0xF7), and produces a human‑readable disassembly in the form of a `.b1v` (or `.b1g`) file.

## Usage

Run the executable from a command prompt, passing the BEST file (or its directory) as an argument:

```bash
BimmerDis.exe "C:\path\to\your\BESTfile.prg"
```

BimmerDis will process the file, produce a decrypted and disassembled output (with a `.b1v` or `.b1g` extension), and save it in the same directory as the input file.
