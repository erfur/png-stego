# png-stego
Apply steganography on .png files


Usage:
```
	png-stego -i inputFile [-x] [-b bitDepth] [-e embedFile] [-o outFile]
```

To read file headers:
```
	png-stego -i image.png
```

To embed files:
```
	png-stego -i image.png -b 4 -e embedThis.txt -o out.png
```

To extract embedded files:
```
	png-stego -x -i image.png -b 4 -o extracted.dat
```

Note: This program is written for **experimental** purposes and is probably not suitable for critical tasks. Use at your own risk.
