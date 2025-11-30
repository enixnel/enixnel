# This is Enixnel v0.1 (beta)
The Enixnel kernel is made by literally no one, instead it is made by AI. The joke is since enix likes to vibecode so much, we made a vibecoded kernel, hence the name **Enix**nel

This was made using GPT 5.1 (medium reasoning level), there is no license. Do whatever the fuck you want with it
# Building
There are no prebuilt ISOs for beta versions, so you will have to build yourself, HOWEVER since this is the first version, we will provide a prebuilt iso. This is assuming there will ever be any other betas past this point

(We used Ubuntu 24.04 to test Enixnel versions.)

First install the packages required to build:

`sudo apt update && sudo apt install build-essential grub-pc-bin xorriso qemu-system-x86`

Then, use build.sh (THIS WILL TEST THE ISO BEFOREHAND TO ESSURE IT WORKS USING QEMU.)

`./build.sh`

## Building without testing

Just do these 2 commands

`make CC="gcc -m32" LD=ld`

`make iso`

To do a fresh build, ensure you do `make clean` first.



