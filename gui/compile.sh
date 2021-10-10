#!/bin/bash
cython3 main.py --embed
gcc $(python-config --cflags) main.c $(python-config --libs)
# sudo chown root a.out
# sudo chmod +s a.out
rm main.c