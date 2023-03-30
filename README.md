# 64x64_LED_Matrix
Creates following "values". Each value accept JSON configs down below. If the any key is missing from JSON config, device will not accept the configuration and will notify which key is not present.

Mono Bitmap - accepts following

`{
  "x": 0,
  "y": 0,
  "w": 64,
  "h": 64,
  "color": "0xffff",
  "bitmap": "183c7edb18181818"
}`

"color" key accepts a string with an RGB565 color in 2Byte HEX format.
"bitmap" is Bitmap data itself represented as string with HEX values in 1byte numbers without '0x', e.g "183c7edb18181818".

RGB565 Bitmap - accepts following

`{
  "x": 0,
  "y": 0,
  "w": 64,
  "h": 64,
  "bitmap": ""
}`

"bitmap" key accepts a URL string leading to a .bmp image in RGB565 color format. Image should not exeed maximum screen resolusion.

Text input - accepts following

`{
  "x": 0,
  "y": 0,
  "w": 64,
  "h": 64,
  "tColor": "0xffff",
  "bColor": "0x0000",
  "tSize": 1,
  "text": "",
  "drawBG": true
}`

"tColor" and "bColor" keys accept string with an RGB565 color in 2Byte HEX format.
"tSize" key is text size and it accepts int values where 1 is the default text size. On a 64x64 screen can maximum go up to 8 leading to 1 character drawn on the screen.
"text" key is text string to be written on a screen. For a 64x64 screen with text area of "w": 64,"h": 64 and text size 1, there is place for up to 80 characters in total.
"drawBG" key is boolean based on which the "bColor" will or will not be used to draw background color for given rectangle with keys "w" and "h".
