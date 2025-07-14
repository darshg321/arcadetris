#include <Adafruit_Protomatter.h>

#define WIDTH 64
#define HEIGHT 32

// Pin configuration
uint8_t rgbPins[] = {2, 10, 3, 4, 11, 5};   // R1, G1, B1, R2, G2, B2
uint8_t addrPins[] = {6, 12, 7, 13};        // A, B, C, D
int clockPin = 8;
int latchPin = 14;
int oePin = 9;

// Create matrix object
Adafruit_Protomatter matrix(
  WIDTH,
  1,
  1,
  rgbPins,
  4, addrPins,
  clockPin,
  latchPin,
  oePin,
  false);

// Simple sand simulation grid
bool sand[HEIGHT][WIDTH] = {false};

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  ProtomatterStatus status = matrix.begin();
  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  if (status != PROTOMATTER_OK) {
    Serial.println("Matrix failed to begin");
    while (1);
  }

  matrix.fillScreen(0x0000);
  matrix.show();
}

// Returns a 16-bit RGB565 yellow color
uint16_t sandColor() {
  return matrix.color565(255, 0, 0); // Red only, 1 color channel
}

void loop() {
  // Add new sand grain at a random top location
  int x = random(WIDTH);
  if (!sand[0][x]) {
    sand[0][x] = true;
  }

  // Update sand positions
  for (int y = HEIGHT - 2; y >= 0; y--) {
    for (int x = 0; x < WIDTH; x++) {
      if (sand[y][x] && !sand[y + 1][x]) {
        // Move sand down
        sand[y][x] = false;
        sand[y + 1][x] = true;
      }
    }
  }

  // Draw to matrix
  matrix.fillScreen(0x0000); // Clear screen
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      if (sand[y][x]) {
        matrix.drawPixel(x, y, sandColor());
      }
    }
  }

  matrix.show();
  delay(200); // Adjust for animation speed
}
