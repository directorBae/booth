// ============================================
// Parent ESP32-C5 Controller
// ============================================
// 역할: 버튼 입력을 받아 UART로 명령 전송 + SYNC 신호 생성
// ※ SD 카드 없음, LED Strip 없음 (컨트롤러 전용)

// === 버튼 핀 설정 ===
const int BUTTON_1 = 9;     // 파일 1 재생
const int BUTTON_2 = 10;    // 파일 2 재생
const int BUTTON_3 = 13;    // 파일 3 재생
const int BUTTON_4 = 14;    // 파일 4 재생
const int BUTTON_STOP = 26; // 재생 정지

// === 동기화 신호 핀 ===
const int SYNC_PIN = 0; // 모든 Child GPIO 0에 연결

// === UART 설정 ===
// TX: GPIO 23 (모든 Child GPIO 23에 연결)
HardwareSerial SerialUART(1); // Serial1 사용

// === 재생 설정 ===
const unsigned long FRAME_DELAY_MS = 44; // 약 22 FPS (1000ms / 22 = 45ms)
                                         // ※ 매 프레임마다 SYNC 신호 전송
                                         // → Child들이 동시에 FastLED.show() 호출
const unsigned long DEBOUNCE_DELAY = 200;

// === 전역 변수 ===
uint8_t currentFile = 0; // 0: 정지, 1-4: 파일 번호
bool isPlaying = false;
unsigned long lastButtonTime = 0;
unsigned long lastFrameTime = 0;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=========================================");
    Serial.println("  Parent ESP32-C5 Controller");
    Serial.println("  - Button Input");
    Serial.println("  - UART Command Broadcast");
    Serial.println("  - SYNC Signal Generation");
    Serial.println("=========================================");

    // === 버튼 핀 초기화 ===
    pinMode(BUTTON_1, INPUT_PULLUP);
    pinMode(BUTTON_2, INPUT_PULLUP);
    pinMode(BUTTON_3, INPUT_PULLUP);
    pinMode(BUTTON_4, INPUT_PULLUP);
    pinMode(BUTTON_STOP, INPUT_PULLUP);
    Serial.println("\n[1/3] ✓ Buttons initialized");

    // === SYNC 핀 초기화 ===
    pinMode(SYNC_PIN, OUTPUT);
    digitalWrite(SYNC_PIN, LOW);
    Serial.println("[2/3] ✓ SYNC pin initialized (GPIO 0)");

    // === UART 초기화 ===
    // TX: GPIO 23, RX: 사용 안 함
    SerialUART.begin(115200, SERIAL_8N1, -1, 23);
    Serial.println("[3/3] ✓ UART initialized (TX: GPIO 23, 115200 baud)");

    Serial.println("\n=========================================");
    Serial.println("  Parent Ready!");
    Serial.println("  Waiting for button input...");
    Serial.println("=========================================\n");
}

void loop()
{
    checkButtons();

    if (isPlaying)
    {
        unsigned long now = millis();
        if (now - lastFrameTime >= FRAME_DELAY_MS)
        {
            sendSyncSignal();
            lastFrameTime = now;
        }
    }
    else
    {
        delay(10);
    }
}

void checkButtons()
{
    unsigned long now = millis();
    if (now - lastButtonTime < DEBOUNCE_DELAY)
        return;

    // 버튼 1: 파일 1 재생
    if (digitalRead(BUTTON_1) == LOW)
    {
        lastButtonTime = now;
        startPlayback(1);
    }
    // 버튼 2: 파일 2 재생
    else if (digitalRead(BUTTON_2) == LOW)
    {
        lastButtonTime = now;
        startPlayback(2);
    }
    // 버튼 3: 파일 3 재생
    else if (digitalRead(BUTTON_3) == LOW)
    {
        lastButtonTime = now;
        startPlayback(3);
    }
    // 버튼 4: 파일 4 재생
    else if (digitalRead(BUTTON_4) == LOW)
    {
        lastButtonTime = now;
        startPlayback(4);
    }
    // 정지 버튼
    else if (digitalRead(BUTTON_STOP) == LOW)
    {
        lastButtonTime = now;
        stopPlayback();
    }
}

void startPlayback(uint8_t fileNum)
{
    if (isPlaying && currentFile == fileNum)
    {
        // 이미 같은 파일 재생 중
        return;
    }

    currentFile = fileNum;
    isPlaying = true;
    lastFrameTime = millis();

    // UART로 Child들에게 명령 전송
    SerialUART.write(fileNum);

    Serial.printf("\n[START] Playing file %d\n", fileNum);
    Serial.println("  Command sent to all Children via UART");
    Serial.println("  SYNC signals will be sent every 44ms (22 FPS)");
}

void stopPlayback()
{
    if (!isPlaying)
    {
        return;
    }

    isPlaying = false;
    currentFile = 0;

    // UART로 정지 명령 전송 (0 = 정지)
    SerialUART.write(0);

    Serial.println("\n[STOP] Playback stopped");
    Serial.println("  Stop command sent to all Children");
}

void sendSyncSignal()
{
    // SYNC 신호: HIGH → 100μs 대기 → LOW
    digitalWrite(SYNC_PIN, HIGH);
    delayMicroseconds(100);
    digitalWrite(SYNC_PIN, LOW);

    // 주기적으로 상태 출력 (매 1초마다)
    static unsigned long lastStatusTime = 0;
    unsigned long now = millis();
    if (now - lastStatusTime >= 1000)
    {
        Serial.printf("[SYNC] File %d playing... (22 FPS)\n", currentFile);
        lastStatusTime = now;
    }
}
