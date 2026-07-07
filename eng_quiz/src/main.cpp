#include <M5Core2.h>

// ---------- Data ----------

struct QA {
  const char* word;
  const char* meaning;
};

// Predefined words and meanings
QA items[] = {  
  // unit XX
  {"big",  "velky"},      
  {"cat",  "macka"},      
  {"elephant",  "slon"},          
  {"fan",  "ventilator"},          
  {"giraffe",  "zirafa"},            
  {"little",  "maly"},          
  {"man",  "muz"},          
  {"sad",  "smutny"},          
  {"parrot",  "papagaj"},          
  {"polar bear",  "ladovy medved"},
  {"seal",  "tulen"},          
  {"snake",  "had"},          
  {"tall",  "vysoky"},                                    
  {"tiger",  "tiger"},              
};

const int N = sizeof(items) / sizeof(items[0]);
const int NUM_CHOICES = 4;
const int ROUND_LEN = (N < 10) ? N : 10; // ask up to 10, or N if N<10

// ---------- State ----------

int questionIndex = 0;            // index of the correct item for current question
int optionIdx[NUM_CHOICES];       // indices of the four options
int correctOption = -1;           // which option (0..3) is correct
int score = 0;                    // total correct
int totalAsked = 0;               // total questions asked in the round

int qOrder[N];                    // shuffled question order (no repeats in round)
int qPos = 0;                     // position in qOrder

bool waitingForNext = false;
bool finished = false;
bool touchLocked = false;         // prevents press-hold from causing multiple taps

// ---------- Layout (Core2 320x240) ----------

struct Rect { int x, y, w, h; };
Rect buttons[NUM_CHOICES];

const int SCR_W = 320;
const int SCR_H = 240;

const int MARGIN = 8;
const int GUTTER = 8;
const int BTN_W  = 148;           // (320 - 2*MARGIN - GUTTER) / 2 = 148
const int BTN_H  = 52;            // fits two rows comfortably

void computeLayout() {
  // 2×2 grid at the bottom half:
  // Rows at y = 120 and 180; Cols at x = 8 and 164.
  int col1X = MARGIN;
  int col2X = MARGIN + BTN_W + GUTTER;
  int row1Y = 120;
  int row2Y = row1Y + BTN_H + GUTTER;

  // Order: left-top, left-bottom, right-top, right-bottom
  buttons[0] = { col1X, row1Y, BTN_W, BTN_H };
  buttons[1] = { col1X, row2Y, BTN_W, BTN_H };
  buttons[2] = { col2X, row1Y, BTN_W, BTN_H };
  buttons[3] = { col2X, row2Y, BTN_W, BTN_H };
}

// ---------- Helpers ----------

void drawHeader(const char* title) {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(8, 6);
  M5.Lcd.printf("%s", title);

  M5.Lcd.setCursor(8, 34);
  M5.Lcd.printf("Score: %d / %d", score, totalAsked);
}

// Word-wrapped text drawing constrained to a box width and bottom limit.
// Adds "..." if there's no more vertical room.
// Assumes caller has already set text size & font on M5.Lcd.
void drawWrappedTextBox(const char* text,
                        int x, int y,
                        int boxWidth, int lineHeight,
                        int bottomLimit,
                        uint16_t color, uint16_t bg) {
  M5.Lcd.setTextColor(color, bg);

  String s(text);
  String line = "";
  int cy = y;
  int i = 0;
  int n = s.length();

  // Helper: print current buffered line at (x, cy)
//   auto printBufferedLine = & {
    M5.Lcd.setCursor(x, cy);
    M5.Lcd.print(line);
//   };

  // NOTE: The above is a lambda only used internally here; if you prefer absolutely no lambdas at all,
  // replace the two calls to printBufferedLine(...) with the two lines:
  //    M5.Lcd.setCursor(x, cy);
  //    M5.Lcd.print(line);

  while (i < n) {
    // Handle explicit newline
    if (s[i] == '\n') {
      if (cy + lineHeight > bottomLimit) {
        if (bottomLimit - lineHeight >= y) {
          M5.Lcd.setCursor(x, bottomLimit - lineHeight);
          M5.Lcd.print("...");
        }
        return;
      }
      // print current line and move down
      M5.Lcd.setCursor(x, cy);
      M5.Lcd.print(line);
      cy += lineHeight;
      line = "";
      i++;
      continue;
    }

    // Skip spaces/tabs; keep at most one space between words
    bool sawSpace = false;
    while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) {
      sawSpace = true;
      i++;
    }
    String sep = (sawSpace && line.length() > 0) ? " " : "";

    // Read the next word
    String word = "";
    while (i < n && s[i] != ' ' && s[i] != '\t' && s[i] != '\r' && s[i] != '\n') {
      word += s[i++];
    }
    if (word.length() == 0) {
      // multiple spaces; continue
      continue;
    }

    // Candidate if we append this word (plus a single space if needed)
    String candidate;
    if (line.length()) candidate = line + sep + word;
    else               candidate = word;

    int w = M5.Lcd.textWidth(candidate);
    if (w <= boxWidth) {
      // fits on current line
      line = candidate;
    } else {
      // It doesn't fit on this line.
      if (line.length() == 0) {
        // Single word too long to fit: break it char-by-char
        for (int k = 0; k < (int)word.length(); ++k) {
          String ch = String(word[k]);
          int w2 = M5.Lcd.textWidth(line + ch);
          if (w2 <= boxWidth) {
            line += ch;
          } else {
            // need to wrap before adding ch
            if (cy + lineHeight > bottomLimit) {
              if (bottomLimit - lineHeight >= y) {
                M5.Lcd.setCursor(x, bottomLimit - lineHeight);
                M5.Lcd.print("...");
              }
              return;
            }
            // print line and move down
            M5.Lcd.setCursor(x, cy);
            M5.Lcd.print(line);
            cy += lineHeight;
            line = ch; // start new line with the char we couldn't fit
          }
        }
      } else {
        // We have some text in the line already; wrap before this word
        if (cy + lineHeight > bottomLimit) {
          if (bottomLimit - lineHeight >= y) {
            M5.Lcd.setCursor(x, bottomLimit - lineHeight);
            M5.Lcd.print("...");
          }
          return;
        }
        // print current line and move down
        M5.Lcd.setCursor(x, cy);
        M5.Lcd.print(line);
        cy += lineHeight;
        line = word; // start the new line with this word
      }
    }
  }

  // Print any remaining text if there's room; else ellipsis
  if (line.length() && cy + lineHeight <= bottomLimit) {
    M5.Lcd.setCursor(x, cy);
    M5.Lcd.print(line);
  } else if (line.length() && bottomLimit - lineHeight >= y) {
    M5.Lcd.setCursor(x, bottomLimit - lineHeight);
    M5.Lcd.print("...");
  }
}

void drawButtonWithLabel(const Rect& r, uint16_t fill, uint16_t border, uint16_t textColor, const char* label) {
  M5.Lcd.fillRoundRect(r.x, r.y, r.w, r.h, 6, fill);
  M5.Lcd.drawRoundRect(r.x, r.y, r.w, r.h, 6, border);

  // Auto-scale label (size 2 if it fits, else 1)
  int textSize = 2;
  M5.Lcd.setTextSize(textSize);
  int tw = M5.Lcd.textWidth(label);
  if (tw > (r.w - 16)) { // 8px padding each side
    textSize = 1;
    M5.Lcd.setTextSize(textSize);
  }

  // Approximate text height for GLCD font: 8 * textSize
  int th = 8 * textSize;
  int tx = r.x + 10;
  int ty = r.y + (r.h - th) / 2;
  M5.Lcd.setTextColor(textColor, fill);
  M5.Lcd.setCursor(tx, ty);
  M5.Lcd.print(label);
}

void drawButtons() {
  for (int i = 0; i < NUM_CHOICES; i++) {
    const char* label = items[optionIdx[i]].word;
    drawButtonWithLabel(buttons[i], DARKGREY, WHITE, WHITE, label);
  }
}

int hitTest(int x, int y) {
  for (int i = 0; i < NUM_CHOICES; i++) {
    Rect r = buttons[i];
    if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) return i;
  }
  return -1;
}

void shuffleIndices(int* arr, int n) {
  for (int i = n - 1; i > 0; --i) {
    int j = random(i + 1);
    int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
  }
}

// ---------- Quiz Flow ----------

void makeQuestion() {
  questionIndex = qOrder[qPos];

  // Pick which slot will hold the correct answer
  correctOption = random(NUM_CHOICES);

  // Ensure unique options
  bool used[N] = {false};
  used[questionIndex] = true;
  optionIdx[correctOption] = questionIndex;

  // Fill other option slots with unique random indices
  for (int i = 0; i < NUM_CHOICES; i++) {
    if (i == correctOption) continue;
    int r;
    do { r = random(N); } while (used[r]);
    used[r] = true;
    optionIdx[i] = r;
  }
}

void showQuestion() {
  drawHeader("Word Quiz"); // clears screen and shows current score

  // Meaning text: wrapped in the top area (above the first row of buttons)
  M5.Lcd.setTextSize(2);
  int wrapX = 8;
  int wrapY = 60;                  // below header
  int wrapW = SCR_W - 2 * MARGIN;  // 304
  int lineH = 20;                  // comfortable line spacing for size 2
  int bottomLimit = buttons[0].y - 8; // keep a small gap above buttons
  drawWrappedTextBox(items[questionIndex].meaning, wrapX, wrapY, wrapW, lineH, bottomLimit, YELLOW, BLACK);

  drawButtons();
}

void feedback(int tappedOption) {
  // Color the chosen button
  uint32_t color = (tappedOption == correctOption) ? GREEN : RED;
  const char* tappedLabel = items[optionIdx[tappedOption]].word;
  drawButtonWithLabel(buttons[tappedOption], color, WHITE, WHITE, tappedLabel);

  // Also highlight the correct one if wrong
  if (tappedOption != correctOption) {
    const char* correctLabel = items[optionIdx[correctOption]].word;
    drawButtonWithLabel(buttons[correctOption], GREEN, WHITE, WHITE, correctLabel);
  }

  // Small result message close to bottom (stay within 320x240)
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.fillRect(8, SCR_H - 18, SCR_W - 16, 14, BLACK); // clear previous
  M5.Lcd.setCursor(8, SCR_H - 18);
  if (tappedOption == correctOption) {
    M5.Lcd.print("Correct! Great job!");
  } else {
    M5.Lcd.printf("Oops! Correct: %s", items[questionIndex].word);
  }
}

void showFinal() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(20, 80);
  M5.Lcd.printf("Final score: %d / %d", score, totalAsked);

  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(20, 140);
  M5.Lcd.print("Tap to play again");
}

void startNewRound() {
  for (int i = 0; i < N; ++i) qOrder[i] = i;
  shuffleIndices(qOrder, N);
  qPos = 0;
  score = 0;
  totalAsked = 0;
  finished = false;
}

void waitForTouchRelease() {
  // Wait until no finger on screen to avoid accidental next taps
  while (true) {
    M5.update();
    TouchPoint_t p = M5.Touch.getPressPoint();
    if (p.x == -1) break;
    delay(10);
  }
}

// ---------- Arduino Setup / Loop ----------

void setup() {
  M5.begin(true, true, true, true); // LCD, SD, Serial, I2C
  M5.Axp.SetLcdVoltage(2800);       // Stable brightness on Core2
  randomSeed(millis());             // Portable seeding

  computeLayout();
  startNewRound();
  makeQuestion();
  showQuestion();
}

void loop() {
  M5.update();

  TouchPoint_t p = M5.Touch.getPressPoint();
  bool isPressed = (p.x != -1);

  // Unlock on finger release
  if (!isPressed) touchLocked = false;

  if (finished) {
    if (isPressed && !touchLocked) {
      touchLocked = true;
      waitForTouchRelease();
      startNewRound();
      makeQuestion();
      showQuestion();
    }
    return;
  }

  if (isPressed && !waitingForNext && !touchLocked) {
    touchLocked = true; // lock until release

    int tapped = hitTest(p.x, p.y);
    if (tapped != -1) {
      waitingForNext = true;
      totalAsked++;
      if (tapped == correctOption) score++;

      feedback(tapped);

      // Short pause so the user can see feedback
      delay(900);

      // Next question or finish
      qPos++;
      if (qPos >= ROUND_LEN) {
        finished = true;
        showFinal();
      } else {
        makeQuestion();
        showQuestion();
      }

      waitingForNext = false;
      waitForTouchRelease();
    }
  }
}