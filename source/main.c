#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <malloc.h>

// Timer states
typedef enum {
    TIMER_STOPPED,
    TIMER_RUNNING,
    TIMER_PAUSED,
    TIMER_DONE
} TimerState;

// Global variables for timer
static TimerState timerState = TIMER_STOPPED;
static u64 startTime = 0;
static u64 pausedTime = 0;
static u64 durationMs = 0; // Total duration for the current run

// Time setting mode variables
static bool settingMode = false;
static int selectedField = 0; // 0 = minutes, 1 = seconds
static u64 customTime = 0;    // Custom time in milliseconds
static int blinkCounter = 0;

// D-pad acceleration variables
static u64 upHoldTime = 0;
static u64 downHoldTime = 0;
static const u64 ACCELERATION_THRESHOLD = 20; // Frames to hold before acceleration starts
static const u64 MAX_ACCELERATION = 10;       // Maximum speed multiplier

// Audio constants
#define SAMPLERATE 48000
#define CHANNELCOUNT 2
#define FRAMERATE (1000 / 30)
#define SAMPLECOUNT (SAMPLERATE / FRAMERATE)
#define BYTESPERSAMPLE 2

// Global variables for vibration
static bool vibrating = false;
static HidVibrationDeviceHandle vib_handles[2][2];
static HidVibrationValue vibrationValue;
static HidVibrationValue vibrationStop;
static bool vib_inited = false;

// Audio buffer and state
static AudioOutBuffer audout_buf;
static AudioOutBuffer *audout_released_buf;
static u8* out_buf_data = NULL;
static size_t audio_buffer_size = 0;
static bool audio_initialized = false;

static void init_vibration(PadState* pad) {
    if (vib_inited) return;
    
    // Initialize vibration for both handheld and connected controllers
    Result rc = hidInitializeVibrationDevices(vib_handles[0], 2, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
    if (R_SUCCEEDED(rc)) {
        rc = hidInitializeVibrationDevices(vib_handles[1], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);
    }
    
    // Setup vibration values
    vibrationValue.amp_low = 0.8f;
    vibrationValue.freq_low = 160.0f;
    vibrationValue.amp_high = 0.8f;
    vibrationValue.freq_high = 320.0f;
    
    // Setup stop values
    memset(&vibrationStop, 0, sizeof(HidVibrationValue));
    vibrationStop.freq_low = 160.0f;
    vibrationStop.freq_high = 320.0f;
    
    vib_inited = true;
}

static void start_vibration() {
    if (!vib_inited) return;
    
    HidVibrationValue values[2];
    memcpy(&values[0], &vibrationValue, sizeof(HidVibrationValue));
    memcpy(&values[1], &vibrationValue, sizeof(HidVibrationValue));
    
    // Send to both handheld and controller
    hidSendVibrationValues(vib_handles[0], values, 2);
    hidSendVibrationValues(vib_handles[1], values, 2);
    
    vibrating = true;
}

static void stop_vibration() {
    if (!vib_inited) return;
    
    HidVibrationValue values[2];
    memcpy(&values[0], &vibrationStop, sizeof(HidVibrationValue));
    memcpy(&values[1], &vibrationStop, sizeof(HidVibrationValue));
    
    // Stop both handheld and controller
    hidSendVibrationValues(vib_handles[0], values, 2);
    hidSendVibrationValues(vib_handles[1], values, 2);
    
    vibrating = false;
}

// Function to format time as MM:SS.mmm
void formatTime(char* buffer, size_t bufferSize, u64 milliseconds) {
    u64 total_seconds = milliseconds / 1000;
    u64 minutes = total_seconds / 60;
    u64 seconds = total_seconds % 60;
    u64 millis = milliseconds % 1000;
    snprintf(buffer, bufferSize, "%02lu:%02lu.%03lu", (unsigned long)minutes, (unsigned long)seconds, (unsigned long)millis);
}

// Function to get current time in milliseconds
u64 getCurrentTime() {
    return armGetSystemTick() * 1000 / armGetSystemTickFreq();
}

// Fill audio buffer with a specific frequency
void fill_audio_buffer(void* audio_buffer, size_t offset, size_t size, int frequency) {
    if (audio_buffer == NULL) return;
    
    u32* dest = (u32*) audio_buffer;
    for (int i = 0; i < size/4; i++) {
        // Simple sine wave with the specified frequency
        s16 sample = 0.3 * 0x7FFF * sin(frequency * (2 * M_PI) * (offset + i) / SAMPLERATE);
        // Stereo samples are interleaved: left and right channels
        dest[i] = (sample << 16) | (sample & 0xffff);
    }
}

// Play a beep sound
void play_beep(int frequency, int duration_ms) {
    if (!audio_initialized) return;
    
    // Calculate number of samples needed for the duration
    size_t num_samples = (SAMPLERATE * duration_ms) / 1000;
    size_t data_size = num_samples * CHANNELCOUNT * BYTESPERSAMPLE;
    
    // Fill the buffer with the beep sound
    fill_audio_buffer(out_buf_data, 0, data_size, frequency);
    
    // Prepare the audio buffer
    audout_buf.next = NULL;
    audout_buf.buffer = out_buf_data;
    audout_buf.buffer_size = audio_buffer_size;
    audout_buf.data_size = data_size;
    audout_buf.data_offset = 0;
    
    // Play the buffer
    audoutPlayBuffer(&audout_buf, &audout_released_buf);
}

// Play 3 beeps when timer reaches zero
// Returns true if the beeps were played (or started playing), false if B was pressed
bool play_timer_complete_sound(PadState* pad) {
    // Check if B is already pressed to prevent playing if user is quick to stop
    padUpdate(pad);
    if (padGetButtons(pad) & HidNpadButton_B) {
        return false;
    }
    
    for (int i = 0; i < 3; i++) {
        // Check for B button press between beeps
        padUpdate(pad);
        if (padGetButtons(pad) & HidNpadButton_B) {
            return false;
        }
        
        // Start vibration and beep simultaneously
        start_vibration();
        play_beep(880, 200); // A5 note, 200ms
        
        // Keep vibration on for the duration of the beep (200ms)
        svcSleepThread(200 * 1000000);
        
        // Stop vibration when beep ends
        stop_vibration();
        
        // Check for B button press during the pause between beeps
        for (int j = 0; j < 20; j++) { // 20 * 10ms = 200ms
            svcSleepThread(10 * 1000000); // 10ms
            padUpdate(pad);
            if (padGetButtons(pad) & HidNpadButton_B) {
                return false;
            }
        }
    }
    return true;
}

int main(int argc, char **argv) {
    Result rc = 0;
    
    // Initialize console
    consoleInit(NULL);
    
    // Configure our supported input
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    
    // Initialize the default gamepad
    PadState pad;
    padInitializeDefault(&pad);
    
    // Initialize vibration system
    init_vibration(&pad);
    
    // Initialize audio
    rc = audoutInitialize();
    if (R_SUCCEEDED(rc)) {
        // Calculate buffer size aligned to 0x1000 bytes
        u32 data_size = (SAMPLECOUNT * CHANNELCOUNT * BYTESPERSAMPLE);
        audio_buffer_size = (data_size + 0xfff) & ~0xfff;
        
        // Allocate the buffer
        out_buf_data = memalign(0x1000, audio_buffer_size);
        if (out_buf_data != NULL) {
            memset(out_buf_data, 0, audio_buffer_size);
            rc = audoutStartAudioOut();
            if (R_SUCCEEDED(rc)) {
                audio_initialized = true;
            }
        }
    }
    
    // Main loop
    while(appletMainLoop()) {
        // Scan the gamepad
        padUpdate(&pad);
        
        // Get the current state of the buttons
        u64 kDown = padGetButtonsDown(&pad);
        
        // Toggle setting mode with Y button
        if (kDown & HidNpadButton_Y) {
            settingMode = !settingMode;
            if (settingMode) {
                // Entering setting mode - save current time
                if (timerState == TIMER_RUNNING) {
                    u64 elapsed = getCurrentTime() - startTime;
                    // Remaining time of current run
                    u64 remaining = (durationMs > elapsed) ? (durationMs - elapsed) : 0;
                    customTime = remaining;
                } else {
                    customTime = pausedTime > 0 ? pausedTime : (durationMs > 0 ? durationMs : (5 * 60 * 1000));
                }
                timerState = TIMER_PAUSED;
            } else {
                // Exiting setting mode - ensure we have a valid time and apply it
                if (customTime == 0) {
                    customTime = 5 * 60 * 1000; // Default to 5 minutes if time is 0
                }
                pausedTime = customTime;
                durationMs = customTime;
            }
        }

        // Handle time adjustment in setting mode
        if (settingMode) {
            u64 minutes = customTime / 60000;
            u64 seconds = (customTime % 60000) / 1000;
            u64 millis = customTime % 1000;
            
            // Handle D-pad up with acceleration
            bool upPressed = padGetButtons(&pad) & HidNpadButton_Up;
            bool upJustPressed = padGetButtonsDown(&pad) & HidNpadButton_Up;
            
            if (upJustPressed) {
                // Single press - increment by 1
                if (selectedField == 0) { // Minutes
                    if (minutes < 99) minutes++;
                } else { // Seconds
                    if (seconds < 59) seconds++;
                    else seconds = 0;
                }
                customTime = minutes * 60000 + seconds * 1000 + millis;
                upHoldTime = 1;
            } else if (upPressed) {
                // Handle hold acceleration
                upHoldTime++;
                if (upHoldTime > ACCELERATION_THRESHOLD) {
                    u64 acceleration = (upHoldTime - ACCELERATION_THRESHOLD) / 5 + 1;
                    if (acceleration > MAX_ACCELERATION) acceleration = MAX_ACCELERATION;
                    
                    for (u64 i = 0; i < acceleration; i++) {
                        if (selectedField == 0) { // Minutes
                            if (minutes < 99) minutes++;
                        } else { // Seconds
                            if (seconds < 59) seconds++;
                            else seconds = 0;
                        }
                    }
                    customTime = minutes * 60000 + seconds * 1000 + millis;
                }
            } else {
                upHoldTime = 0;
            }
            
            // Handle D-pad down with acceleration
            bool downPressed = padGetButtons(&pad) & HidNpadButton_Down;
            bool downJustPressed = padGetButtonsDown(&pad) & HidNpadButton_Down;
            
            if (downJustPressed) {
                // Single press - decrement by 1
                if (selectedField == 0) { // Minutes
                    if (minutes > 0) minutes--;
                } else { // Seconds
                    if (seconds > 0) seconds--;
                    else seconds = 59;
                }
                customTime = minutes * 60000 + seconds * 1000 + millis;
                downHoldTime = 1;
            } else if (downPressed) {
                // Handle hold acceleration
                downHoldTime++;
                if (downHoldTime > ACCELERATION_THRESHOLD) {
                    u64 acceleration = (downHoldTime - ACCELERATION_THRESHOLD) / 5 + 1;
                    if (acceleration > MAX_ACCELERATION) acceleration = MAX_ACCELERATION;
                    
                    for (u64 i = 0; i < acceleration; i++) {
                        if (selectedField == 0) { // Minutes
                            if (minutes > 0) minutes--;
                        } else { // Seconds
                            if (seconds > 0) seconds--;
                            else seconds = 59;
                        }
                    }
                    customTime = minutes * 60000 + seconds * 1000 + millis;
                }
            } else {
                downHoldTime = 0;
            }
            
            if (padGetButtonsDown(&pad) & (HidNpadButton_Left | HidNpadButton_Right)) {
                selectedField = !selectedField; // Toggle between minutes and seconds
            }
            
            // Update blink counter for cursor
            blinkCounter = (blinkCounter + 1) % 60;
        }
        
        // Start/Pause/Resume timer with A button (only if not in setting mode)
        if ((padGetButtonsDown(&pad) & HidNpadButton_A) && !settingMode) {
            if (timerState == TIMER_DONE) {
                // If timer was done, reset to stopped state
                timerState = TIMER_STOPPED;
                if (vibrating) {
                    stop_vibration();
                }
            } else if (timerState == TIMER_STOPPED) {
                // Start the timer if stopped
                timerState = TIMER_RUNNING;
                if (pausedTime > 0) {
                    // Use remaining time from paused state
                    durationMs = pausedTime;
                } else if (durationMs == 0) {
                    // If no duration set, default to 5 minutes
                    durationMs = 5 * 60 * 1000;
                }
                startTime = getCurrentTime();
            } else if (timerState == TIMER_PAUSED) {
                // Resume from pause
                timerState = TIMER_RUNNING;
                if (pausedTime > 0) {
                    durationMs = pausedTime;
                }
                startTime = getCurrentTime();
            } else if (timerState == TIMER_RUNNING) {
                // Pause the running timer
                timerState = TIMER_PAUSED;
                u64 now = getCurrentTime();
                u64 elapsed = now - startTime;
                pausedTime = (elapsed < durationMs) ? (durationMs - elapsed) : 0;
            }
        }
        
        // Stop timer with L button (only if not in setting mode)
        if ((kDown & HidNpadButton_L) && !settingMode) {
            timerState = TIMER_STOPPED;
            startTime = 0;
            durationMs = 5 * 60 * 1000; // Reset to 5 minutes when stopped
            pausedTime = durationMs;
            customTime = 0;
            selectedField = 0;
            if (vibrating) {
                stop_vibration();
            }
        }
        
        // Reset timer with X button (only if not in setting mode)
        if ((kDown & HidNpadButton_X) && !settingMode) {
            // Reset to 5:00
            if (timerState == TIMER_RUNNING) {
                durationMs = 5 * 60 * 1000;
                startTime = getCurrentTime();
                pausedTime = 0;
            } else {
                durationMs = 5 * 60 * 1000;
                pausedTime = durationMs;
            }
            if (timerState == TIMER_DONE) {
                timerState = TIMER_STOPPED;
                if (vibrating) {
                    stop_vibration();
                }
            }
        }

        // Stop vibration and beeps with B button
        if (kDown & HidNpadButton_B) {
            if (vibrating) {
                stop_vibration();
            }
            // If timer is done, pressing B will stop the beeps
            if (timerState == TIMER_DONE) {
                timerState = TIMER_STOPPED;
            }
        }
        
        // Exit with + button
        if (kDown & HidNpadButton_Plus) {
            if (vibrating) {
                stop_vibration();
            }
            break;
        }
        
        // Calculate current time (counting down)
        u64 currentTime;
        if (timerState == TIMER_RUNNING) {
            u64 elapsed = getCurrentTime() - startTime;
            if (elapsed < durationMs) {
                currentTime = durationMs - elapsed;
                // Keep pausedTime in sync while running
                pausedTime = currentTime;
            } else {
                // Timer reached zero
                currentTime = 0;
                timerState = TIMER_DONE;
                startTime = 0;
                durationMs = 0;
                pausedTime = 0;
                start_vibration();
                
                // Keep playing beeps until B is pressed
                while (timerState == TIMER_DONE) {
                    // Update the console to show DONE status
                    consoleClear();
                    printf("Switch Timer\n");
                    printf("------------\n\n");
                    printf("Time: 00:00.000\n\n");
                    printf("Controls:\n");
                    printf("B - Stop Timer\n");
                    printf("\nStatus: DONE (Press B to stop)");
                    consoleUpdate(NULL);
                    
                    if (!play_timer_complete_sound(&pad)) {
                        // B was pressed, stop vibration and break the loop
                        if (vibrating) {
                            stop_vibration();
                        }
                        timerState = TIMER_STOPPED;  // Explicitly set to STOPPED when B is pressed
                        break;
                    }
                    
                    // Small delay to prevent 100% CPU usage
                    svcSleepThread(10000000); // 10ms
                    
                    // Check for B button press between beep sequences
                    padUpdate(&pad);
                    if (padGetButtons(&pad) & HidNpadButton_B) {
                        if (vibrating) {
                            stop_vibration();
                        }
                        timerState = TIMER_STOPPED;  // Explicitly set to STOPPED when B is pressed
                        break;
                        break;
                    }
                }
            }
        } else if (timerState == TIMER_PAUSED) {
            // When paused, show remaining time (or duration if set)
            if (pausedTime > 0) currentTime = pausedTime;
            else if (durationMs > 0) currentTime = durationMs;
            else currentTime = 5 * 60 * 1000;
        } else if (customTime > 0) {
            currentTime = customTime;
        } else {
            // Default to 5 minutes if nothing is set
            currentTime = 5 * 60 * 1000;
        }
        
        // Clear screen
        consoleClear();
        
        // Print header
        if (settingMode) {
            printf("\x1b[1;33m");  // Set yellow color for header in set time menu
        }
        printf("Switch Timer\n");
        printf("------------\n\n");
        
        // Print current time with blinking effect in setting mode
        char timeStr[32];
        if (settingMode) {
            // Ensure we don't show negative time
            if (customTime == 0) {
                customTime = 1; // Minimum 1ms to avoid display issues
            }
            
            // Always show full time without blinking
            u64 minutes = customTime / 60000;
            u64 seconds = (customTime % 60000) / 1000;
            u64 millis = customTime % 1000;
            snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu.%03lu", 
                    (unsigned long)minutes, (unsigned long)seconds, (unsigned long)millis);
            printf("\x1b[1;33m");  // Set yellow color
            printf("Set Time: %s\n", timeStr);
            
            // Display arrow above the selected field (minutes or seconds)
            if (blinkCounter < 30) {  // Blink effect (on for 30 frames, off for 30 frames)
                if (selectedField == 0) {
                    printf("           ^^         \n");  // Arrow above minutes (aligned with MM in MM:SS.mmm)
                } else {
                    printf("              ^^      \n");  // Arrow above seconds (aligned with SS in MM:SS.mmm)
                }
            } else {
                printf("                        \n");  // Empty line when arrow is off
            }
            
            printf("\nSetting Mode (Y to exit)\n");
            printf("D-Pad: Adjust | Left/Right: Select field\n\n");
            printf("\x1b[0m");  // Reset color
        } else {
            // Normal mode - show current time
            formatTime(timeStr, sizeof(timeStr), currentTime);
            printf("Time: %s\n\n", timeStr);
            
            // Print controls
            printf("Controls:\n");
            printf("A - Start/Pause/Resume\n");
            printf("X - Reset to 5:00\n");
            printf("L - Stop Timer\n");
            printf("Y - Set Time\n");
            printf("+  - Exit\n");
        }
        
        // Print status
        const char* statusText;
        switch (timerState) {
            case TIMER_RUNNING: statusText = "RUNNING"; break;
            case TIMER_PAUSED:  statusText = "PAUSED"; break;
            case TIMER_DONE:    statusText = "DONE"; break;
            case TIMER_STOPPED: 
            default:            statusText = "STOPPED"; break;
        }
        
        printf("\n");
        if (settingMode) {
            printf("\x1b[1;33m");  // Set yellow color for status in set time menu
        }
        printf("Status: %s%s\n", statusText,
               (timerState == TIMER_STOPPED && startTime == 0) ? " (Ready)" : "");
        if (settingMode) {
            printf("\x1b[0m");  // Reset color after status in set time menu
        }
        
        // Update the console
        consoleUpdate(NULL);
    }
    
    // Stop audio if it was initialized
    if (audio_initialized) {
        audoutStopAudioOut();
        audoutExit();
    }

    // Free audio buffer if it was allocated
    if (out_buf_data != NULL) {
        free(out_buf_data);
    }
    
    consoleExit(NULL);
    return 0;
}
