The-Eye-written-in-C-cuz-i-hate-myself-

A graphical ASCII-physics simulation built in C using GTK+ 3.0 and Cairo. It features interactive particle displacement, parent-child coordinate nesting, (i could have added a function to shut down your os but i didnt)
Requirements

To compile and run this, you need the GTK+ 3.0 development libraries.
1. Install Dependencies

On Linux (Ubuntu/Debian), run:
Bash

sudo apt update
sudo apt install libgtk-3-dev build-essential

2. Compilation

You cannot compile this like a normal C program; you must link the GTK libraries and the math library:
Bash

gcc ascii_eye.c -o eye_sim `pkg-config --cflags --libs gtk+-3.0` -lm

How to Use

    Interaction: Move the mouse to make the eye follow you.

    Click: Hold Left-Click on the eye to scatter the ASCII characters


