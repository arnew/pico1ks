/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

#include "arduino.h"
#include "Adafruit_TinyUSB.h"

/* This sketch demonstrates USB HID keyboard.
 * - PIN A0-A5 is used to send digit '0' to '5' respectively
 *   (On the RP2040, pins D0-D5 used)
 * - LED and/or Neopixels will be used as Capslock indicator
 */

// HID report descriptor using TinyUSB's template
// Single Report (no ID) descriptor
uint8_t const desc_hid_report[] =
    {
        TUD_HID_REPORT_DESC_KEYBOARD()};

// USB HID object. For ESP32 these values cannot be changed after this declaration
// desc report, desc len, protocol, interval, use out endpoint
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_KEYBOARD, 2, false);

//------------- Input Pins -------------//
// Array of pins and its keycode.
// Notes: these pins can be replaced by PIN_BUTTONn if defined in setup()

uint8_t pins[] = {0, 3, 7, 11};

// number of pins
uint8_t pincount = sizeof(pins) / sizeof(pins[0]);

// For keycode definition check out https://github.com/hathach/tinyusb/blob/master/src/class/hid/hid.h
uint8_t hidcode[] = {HID_KEY_PAGE_DOWN};

// Output report callback for LED indicator such as Caplocks
void hid_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
  (void)report_id;
  (void)bufsize;

  // LED indicator is output report with only 1 byte length
  if (report_type != HID_REPORT_TYPE_OUTPUT)
    return;

  // The LED bit map is as follows: (also defined by KEYBOARD_LED_* )
  // Kana (4) | Compose (3) | ScrollLock (2) | CapsLock (1) | Numlock (0)
  uint8_t ledIndicator = buffer[0];
}

// the setup function runs once when you press reset or power the board
void setup()
{
#if defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040)
  // Manual begin() is required on core without built-in support for TinyUSB such as mbed rp2040
  TinyUSB_Device_Init(0);
#endif

  // Notes: following commented-out functions has no affect on ESP32
  // usb_hid.setBootProtocol(HID_ITF_PROTOCOL_KEYBOARD);
  // usb_hid.setPollInterval(2);
  // usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  // usb_hid.setStringDescriptor("TinyUSB Keyboard");

  // Set up output report (on control endpoint) for Capslock indicator
  usb_hid.setReportCallback(NULL, hid_report_callback);

  usb_hid.begin();

  SerialTinyUSB.begin(115200);

  SerialTinyUSB.println("Alive!");

  // Set up pin as input
  for (uint8_t i = 0; i < pincount; i++)
  {
    pinMode(pins[i], INPUT_PULLUP);
  }

  // wait until device mounted
  while (!TinyUSBDevice.mounted())
    delay(1);
}

void loop()
{
  // poll gpio once each 2 ms
  delay(2);

  // used to avoid send multiple consecutive zero report for keyboard
  static bool keyPressedPreviously = false;

  uint8_t count = 0;
  bool inputs[pincount] = {0};

  // scan inputs
  for (uint8_t i = 0; i < pincount; i++)
  {
    // if pin is active (low), add its hid code to key report
    inputs[i] = digitalRead(pins[i]) == LOW;
  }

  uint8_t keycode[6] = {0};
  static enum {
    init,
    start,
    hold
  } state;
  static int wait = 100, slides = 10;
  static unsigned long timeout = 0;

  SerialTinyUSB.print("now:");
  SerialTinyUSB.print(millis());
  SerialTinyUSB.print(", state:");
  SerialTinyUSB.print(state);
  SerialTinyUSB.print(", wait:");
  SerialTinyUSB.print(wait);
  SerialTinyUSB.print(", slides:");
  SerialTinyUSB.print(slides);
  SerialTinyUSB.print(", timeout:");
  SerialTinyUSB.println(timeout);

  if (inputs[3])
  {

    state = init;
    keycode[0] = HID_KEY_HOME;
    keycode[1] = HID_KEY_R;
    keycode[2] = HID_KEY_T;
  }

  switch (state)
  {
  case init:
    wait = inputs[0] ? 5 : 100;
    slides = 5;
    if (inputs[2])
    {
      keycode[0] = HID_KEY_T;
      state = start;
    }
    break;
  case start:
    if (slides)
    {
      slides--;
      state = hold;
      timeout = millis() + wait * 1000;
      keycode[0] = HID_KEY_PAGE_DOWN;
    }
    else
    {
      keycode[0] = HID_KEY_END;
      state = init;
    }
    break;
  case hold:
    if (timeout < millis()) {
      state = start;
    }
    break;
  default:
    state = init;
    break;
  }

  if (TinyUSBDevice.suspended() && count)
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    TinyUSBDevice.remoteWakeup();
  }

  // skip if hid is not ready e.g still transferring previous report
  if (!usb_hid.ready())
    return;

  if (keycode[0])
  {
    // Send report if there is key pressed
    uint8_t const report_id = 0;
    uint8_t const modifier = 0;

    keyPressedPreviously = true;
    usb_hid.keyboardReport(report_id, modifier, keycode);
  }
  else
  {
    // Send All-zero report to indicate there is no keys pressed
    // Most of the time, it is, though we don't need to send zero report
    // every loop(), only a key is pressed in previous loop()
    if (keyPressedPreviously)
    {
      keyPressedPreviously = false;
      usb_hid.keyboardRelease(0);
    }
  }
}
