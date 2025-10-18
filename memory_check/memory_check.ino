// ESP32-C5 Memory Information Tool

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n==== ESP32-C5 Memory Information ====\n");

    // 1. 힙 메모리 정보
    Serial.println("=== HEAP MEMORY ===");
    Serial.printf("Total heap size:        %u bytes (%.2f KB)\n",
                  ESP.getHeapSize(), ESP.getHeapSize() / 1024.0);
    Serial.printf("Free heap:              %u bytes (%.2f KB)\n",
                  ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0);
    Serial.printf("Used heap:              %u bytes (%.2f KB)\n",
                  ESP.getHeapSize() - ESP.getFreeHeap(),
                  (ESP.getHeapSize() - ESP.getFreeHeap()) / 1024.0);
    Serial.printf("Largest free block:     %u bytes (%.2f KB)\n",
                  ESP.getMaxAllocHeap(), ESP.getMaxAllocHeap() / 1024.0);
    Serial.printf("Min free heap:          %u bytes (%.2f KB)\n",
                  ESP.getMinFreeHeap(), ESP.getMinFreeHeap() / 1024.0);

    // 2. PSRAM 정보 (있을 경우)
    Serial.println("\n=== PSRAM (External RAM) ===");
    if (ESP.getPsramSize() > 0)
    {
        Serial.printf("Total PSRAM size:       %u bytes (%.2f MB)\n",
                      ESP.getPsramSize(), ESP.getPsramSize() / (1024.0 * 1024.0));
        Serial.printf("Free PSRAM:             %u bytes (%.2f MB)\n",
                      ESP.getFreePsram(), ESP.getFreePsram() / (1024.0 * 1024.0));
    }
    else
    {
        Serial.println("PSRAM not available or not enabled");
    }

    // 3. 플래시 메모리 정보
    Serial.println("\n=== FLASH MEMORY ===");
    Serial.printf("Flash chip size:        %u bytes (%.2f MB)\n",
                  ESP.getFlashChipSize(), ESP.getFlashChipSize() / (1024.0 * 1024.0));
    Serial.printf("Flash chip speed:       %u MHz\n",
                  ESP.getFlashChipSpeed() / 1000000);

    // 4. 스케치 정보
    Serial.println("\n=== SKETCH INFORMATION ===");
    Serial.printf("Sketch size:            %u bytes (%.2f KB)\n",
                  ESP.getSketchSize(), ESP.getSketchSize() / 1024.0);
    Serial.printf("Free sketch space:      %u bytes (%.2f KB)\n",
                  ESP.getFreeSketchSpace(), ESP.getFreeSketchSpace() / 1024.0);

    // 5. 칩 정보
    Serial.println("\n=== CHIP INFORMATION ===");
    Serial.printf("Chip model:             %s\n", ESP.getChipModel());
    Serial.printf("Chip revision:          %d\n", ESP.getChipRevision());
    Serial.printf("CPU cores:              %d\n", ESP.getChipCores());
    Serial.printf("CPU frequency:          %u MHz\n", ESP.getCpuFreqMHz());

    // 6. 메모리 할당 테스트
    Serial.println("\n=== MEMORY ALLOCATION TEST ===");
    testMemoryAllocation();

    Serial.println("\n==== Memory Check Complete ====");
}

void loop()
{
    delay(5000);

    // 5초마다 현재 힙 상태 출력
    Serial.println("\n--- Current Heap Status ---");
    Serial.printf("Free heap: %u bytes (%.2f KB)\n",
                  ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0);
    Serial.printf("Largest free block: %u bytes (%.2f KB)\n",
                  ESP.getMaxAllocHeap(), ESP.getMaxAllocHeap() / 1024.0);
}

void testMemoryAllocation()
{
    // LED 배열 크기 계산
    const int sizes[] = {100, 500, 1000, 2000, 4000, 8000, 8640, 17280};
    const int numSizes = sizeof(sizes) / sizeof(sizes[0]);

    Serial.println("\nTesting LED array allocations (3 bytes per LED):");
    Serial.println("Size (LEDs) | Bytes     | Result");
    Serial.println("------------|-----------|--------");

    for (int i = 0; i < numSizes; i++)
    {
        int numLeds = sizes[i];
        int numBytes = numLeds * 3; // RGB = 3 bytes per LED

        // 할당 시도
        uint8_t *testArray = (uint8_t *)malloc(numBytes);

        if (testArray != nullptr)
        {
            Serial.printf("%11d | %9d | ✓ OK\n", numLeds, numBytes);
            free(testArray);
            delay(10);
        }
        else
        {
            Serial.printf("%11d | %9d | ✗ FAILED\n", numLeds, numBytes);
        }
    }

    // 최대 연속 할당 가능 크기 찾기
    Serial.println("\nFinding maximum continuous allocation:");
    size_t maxSize = ESP.getMaxAllocHeap();
    Serial.printf("System reports max block: %u bytes (%.2f KB)\n",
                  maxSize, maxSize / 1024.0);

    // 실제 할당 테스트
    uint8_t *bigArray = (uint8_t *)malloc(maxSize);
    if (bigArray != nullptr)
    {
        Serial.printf("Successfully allocated %u bytes!\n", maxSize);
        Serial.printf("This equals %d LEDs (3 bytes each)\n", maxSize / 3);
        free(bigArray);
    }
    else
    {
        Serial.println("Failed to allocate reported max size.");
    }
}
