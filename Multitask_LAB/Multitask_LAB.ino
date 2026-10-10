#include "flasher.h"
Flasher led1(12, 100, 400);
Flasher led2(13, 350, 350);

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:
  led1.Update();
  led2.Update();
}
