#ifndef BUTTONS_H
#define BUTTONS_H

// Initialize buttons and attach callbacks
void initButtons();

// Read all button states (call in loop)
void readButtons();

// Button callback functions
void onButton1Pressed();
void onButton2Pressed();
void onButton3Pressed();
void onButton4Pressed();

#endif // BUTTONS_H
