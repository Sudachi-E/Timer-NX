#include <switch.h>
#include <borealis.hpp>
#include <string>
#include <malloc.h>
#include <cmath>
#include <vector>
#include <cstdio>
#include <sys/stat.h>
#include "activity/main_activity.hpp"

enum TimerState {
    TIMER_STOPPED,
    TIMER_RUNNING,
    TIMER_PAUSED,
    TIMER_DONE
};

static bool vibrating = false;
static HidVibrationDeviceHandle vib_handles[2][2];
static HidVibrationValue vibrationValue;
static HidVibrationValue vibrationStop;
static bool vib_inited = false;

static const int SAMPLERATE = 48000;
static const int CHANNELCOUNT = 2;
static const int BYTESPERSAMPLE = 2;
static const int MAX_BEEP_MS = 200;
static constexpr double kPi = 3.14159265358979323846;
static AudioOutBuffer audout_buf;
static AudioOutBuffer* audout_released_buf = nullptr;
static u8* out_buf_data = nullptr;
static size_t audio_buffer_size = 0;
static bool audio_initialized = false;

static void init_vibration() {
    if (vib_inited) return;
    Result rc = hidInitializeVibrationDevices(vib_handles[0], 2, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
    if (R_SUCCEEDED(rc)) rc = hidInitializeVibrationDevices(vib_handles[1], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);
    vibrationValue.amp_low = 0.8f;
    vibrationValue.freq_low = 160.0f;
    vibrationValue.amp_high = 0.8f;
    vibrationValue.freq_high = 320.0f;
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
    hidSendVibrationValues(vib_handles[0], values, 2);
    hidSendVibrationValues(vib_handles[1], values, 2);
    vibrating = true;
}

static void stop_vibration() {
    if (!vib_inited) return;
    HidVibrationValue values[2];
    memcpy(&values[0], &vibrationStop, sizeof(HidVibrationValue));
    memcpy(&values[1], &vibrationStop, sizeof(HidVibrationValue));
    hidSendVibrationValues(vib_handles[0], values, 2);
    hidSendVibrationValues(vib_handles[1], values, 2);
    vibrating = false;
}

static void fill_audio_buffer(void* audio_buffer, size_t offset, size_t size, int frequency) {
    if (!audio_buffer) return;
    u32* dest = (u32*)audio_buffer;
    for (size_t i = 0; i < size / 4; i++) {
        s16 sample = (s16)(0.3 * 0x7FFF * std::sin(frequency * (2 * kPi) * (offset + i) / SAMPLERATE));
        dest[i] = ((u32)sample << 16) | ((u32)sample & 0xffff);
    }
}

static void play_beep(int frequency, int duration_ms) {
    if (!audio_initialized) return;
    size_t num_samples = (SAMPLERATE * (size_t)duration_ms) / 1000;
    size_t data_size = num_samples * CHANNELCOUNT * BYTESPERSAMPLE;
    fill_audio_buffer(out_buf_data, 0, data_size, frequency);
    audout_buf.next = nullptr;
    audout_buf.buffer = out_buf_data;
    audout_buf.buffer_size = audio_buffer_size;
    audout_buf.data_size = data_size;
    audout_buf.data_offset = 0;
    audoutPlayBuffer(&audout_buf, &audout_released_buf);
}

static std::string format_mmss(unsigned long long ms) {
    unsigned long long total_seconds = ms / 1000;
    unsigned long long minutes = total_seconds / 60;
    unsigned long long seconds = total_seconds % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02llu:%02llu", minutes, seconds);
    return std::string(buf);
}

static const char* TIMERS_DIR = "sdmc:/switch/SwitchTimer";
static const char* TIMERS_FILE = "sdmc:/switch/SwitchTimer/saved_timers.bin";

static std::vector<unsigned long long> load_saved_timers() {
    std::vector<unsigned long long> timers;
    FILE* f = fopen(TIMERS_FILE, "rb");
    if (!f) return timers;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size > 0 && (size % sizeof(unsigned long long)) == 0) {
        size_t count = size / sizeof(unsigned long long);
        timers.resize(count);
        fread(timers.data(), sizeof(unsigned long long), count, f);
    }
    
    fclose(f);
    return timers;
}

static void save_timers_list(const std::vector<unsigned long long>& timers) {
    mkdir(TIMERS_DIR, 0777);
    FILE* f = fopen(TIMERS_FILE, "wb");
    if (!f) return;
    
    if (!timers.empty()) {
        fwrite(timers.data(), sizeof(unsigned long long), timers.size(), f);
    }
    
    fclose(f);
}

class SavedTimersActivity : public brls::Activity {
  public:
    SavedTimersActivity(std::vector<unsigned long long>* timers, std::function<void(unsigned long long)> onSelect)
        : savedTimers(timers), onSelectCallback(onSelect) {}

    brls::View* createContentView() override {
        auto container = new brls::Box(brls::Axis::COLUMN);
        container->setMargins(40, 40, 40, 40);
        
        auto title = new brls::Label();
        title->setText("Saved Timers");
        title->setFontSize(36);
        title->setMargins(0, 0, 0, 20);
        container->addView(title);
        
        if (savedTimers->empty()) {
            auto emptyLabel = new brls::Label();
            emptyLabel->setText("No saved timers yet.\n\nUse RB to save the current timer.");
            emptyLabel->setFontSize(24);
            container->addView(emptyLabel);
        } else {
            auto scrollView = new brls::ScrollingFrame();
            scrollView->setGrow(1.0f);
            
            auto list = new brls::Box(brls::Axis::COLUMN);
            
            for (size_t i = 0; i < savedTimers->size(); i++) {
                unsigned long long ms = (*savedTimers)[i];
                
                auto itemBox = new brls::Box(brls::Axis::ROW);
                itemBox->setHeight(60);
                itemBox->setCornerRadius(4);
                itemBox->setBackgroundColor(nvgRGB(45, 45, 45));
                itemBox->setAlignItems(brls::AlignItems::CENTER);
                itemBox->setPadding(20, 20, 20, 20);
                itemBox->setMargins(0, 5, 0, 5);
                itemBox->setFocusable(true);
                
                auto label = new brls::Label();
                label->setText(format_mmss(ms));
                label->setFontSize(32);
                label->setGrow(1.0f);
                itemBox->addView(label);
                
                itemBox->registerClickAction([this, ms](brls::View* view) {
                    onSelectCallback(ms);
                    brls::Application::popActivity(brls::TransitionAnimation::FADE);
                    return true;
                });
                
                list->addView(itemBox);
            }
            
            scrollView->setContentView(list);
            container->addView(scrollView);
        }
        
        auto frame = new brls::AppletFrame(container);
        frame->setFooterVisibility(brls::Visibility::VISIBLE);
        frame->setHeaderVisibility(brls::Visibility::VISIBLE);
        frame->setTitle("Saved Timers");
        
        return frame;
    }
    
    void onContentAvailable() override {
        registerAction("Back", brls::BUTTON_B, [](brls::View*) {
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        });
    }

  private:
    std::vector<unsigned long long>* savedTimers;
    std::function<void(unsigned long long)> onSelectCallback;
};

class TimerActivity : public brls::Activity {
  public:
    TimerActivity() {
        savedTimeMs = 5 * 60 * 1000ULL;
        durationMs = savedTimeMs;
        pausedMs = savedTimeMs;
        settingMode = false;
        selectedField = 0;
        state = TIMER_STOPPED;
        savedTimers = load_saved_timers();
    }

    brls::View* createContentView() override {
        auto container = new brls::Box(brls::Axis::COLUMN);
        container->setGrow(1.0f);
        container->setJustifyContent(brls::JustifyContent::CENTER);
        container->setAlignItems(brls::AlignItems::CENTER);

        auto timeRow = new brls::Box(brls::Axis::ROW);
        timeRow->setAlignItems(brls::AlignItems::CENTER);

        minutesCol = new brls::Box(brls::Axis::COLUMN);
        minutesLabel = new brls::Label();
        minutesLabel->setFontSize(128.0f);
        minutesLabel->setSingleLine(true);
        minutesLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        minutesLabel->setVerticalAlign(brls::VerticalAlign::CENTER);
        minutesCaret = new brls::Label();
        minutesCaret->setFontSize(48.0f);
        minutesCaret->setSingleLine(true);
        minutesCaret->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        minutesCaret->setVerticalAlign(brls::VerticalAlign::CENTER);
        minutesCaret->setText("");
        minutesCol->setHeight(128.0f);
        minutesCaret->detach();
        minutesCol->addView(minutesLabel);
        minutesCol->addView(minutesCaret);

        colonLabel = new brls::Label();
        colonLabel->setFontSize(128.0f);
        colonLabel->setSingleLine(true);
        colonLabel->setText(":");

        secondsCol = new brls::Box(brls::Axis::COLUMN);
        secondsLabel = new brls::Label();
        secondsLabel->setFontSize(128.0f);
        secondsLabel->setSingleLine(true);
        secondsLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        secondsLabel->setVerticalAlign(brls::VerticalAlign::CENTER);
        secondsCaret = new brls::Label();
        secondsCaret->setFontSize(48.0f);
        secondsCaret->setSingleLine(true);
        secondsCaret->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        secondsCaret->setVerticalAlign(brls::VerticalAlign::CENTER);
        secondsCaret->setText("");
        secondsCol->setHeight(128.0f);
        secondsCaret->detach();
        secondsCol->addView(secondsLabel);
        secondsCol->addView(secondsCaret);

        timeRow->addView(minutesCol);
        timeRow->addView(colonLabel);
        timeRow->addView(secondsCol);
        container->addView(timeRow);

        focusAnchor = new brls::Box(brls::Axis::ROW);
        focusAnchor->setSize(brls::Size(1, 1));
        focusAnchor->setFocusable(true);
        focusAnchor->setHideHighlightBackground(true);
        focusAnchor->setHideHighlightBorder(true);
        focusAnchor->setAlpha(0.0f);
        container->addView(focusAnchor);

        container->getAppletFrameItem()->title = "Switch Timer";
        container->getAppletFrameItem()->setIconFromRes("img/inappicon copy.png");

        auto frame = new brls::AppletFrame(container);
        frame->setFooterVisibility(brls::Visibility::VISIBLE);
        frame->setHeaderVisibility(brls::Visibility::VISIBLE);
        updateLabel();
        return frame;
    }

    void onContentAvailable() override {
        registerAction("Start/Pause", brls::BUTTON_A, [this](brls::View*) {
            if (settingMode) return false;
            if (state == TIMER_DONE) {
                state = TIMER_STOPPED;
                if (vibrating) stop_vibration();
                vibrationTimer.stop();
                pausedMs = savedTimeMs;
                durationMs = savedTimeMs;
                updateLabel();
                this->getContentView()->setActionAvailable(brls::BUTTON_Y, true);
                return true;
            } else if (state == TIMER_STOPPED) {
                state = TIMER_RUNNING;
                if (pausedMs > 0) durationMs = pausedMs;
                else if (durationMs == 0) durationMs = savedTimeMs;
                startUsec = brls::getCPUTimeUsec();
                this->getContentView()->setActionAvailable(brls::BUTTON_Y, false);
                return true;
            } else if (state == TIMER_RUNNING) {
                unsigned long long elapsedMs = (brls::getCPUTimeUsec() - startUsec) / 1000ULL;
                if (elapsedMs < durationMs) pausedMs = durationMs - elapsedMs; else pausedMs = 0;
                state = TIMER_PAUSED;
                updateLabel();
                this->getContentView()->setActionAvailable(brls::BUTTON_Y, true);
                return true;
            } else if (state == TIMER_PAUSED) {
                state = TIMER_RUNNING;
                durationMs = pausedMs > 0 ? pausedMs : savedTimeMs;
                startUsec = brls::getCPUTimeUsec();
                this->getContentView()->setActionAvailable(brls::BUTTON_Y, false);
                return true;
            }
            return false;
        });
        registerAction("Reset", brls::BUTTON_X, [this](brls::View*) {
            unsigned long long defaultMs = 5ULL * 60ULL * 1000ULL;
            if (state == TIMER_RUNNING) {
                durationMs = defaultMs;
                startUsec = brls::getCPUTimeUsec();
                pausedMs = 0;
            } else {
                state = TIMER_STOPPED;
                durationMs = defaultMs;
                pausedMs = defaultMs;
                this->getContentView()->setActionAvailable(brls::BUTTON_Y, true);
            }
            if (vibrating) stop_vibration();
            vibrationTimer.stop();
            updateLabel();
            return true;
        });
        registerAction("Stop", brls::BUTTON_LB, [this](brls::View*) {
            state = TIMER_STOPPED;
            durationMs = savedTimeMs;
            pausedMs = savedTimeMs;
            if (vibrating) stop_vibration();
            vibrationTimer.stop();
            updateLabel();
            this->getContentView()->setActionAvailable(brls::BUTTON_Y, true);
            return true;
        });
        registerAction("Set", brls::BUTTON_Y, [this](brls::View*) {
            if (state == TIMER_RUNNING) return false;
            settingMode = !settingMode;
            selectedField = 0;
            updateLabel();
            updateCaret();
            this->getContentView()->updateActionHint(brls::BUTTON_Y, settingMode ? "Done" : "Set");
            this->getContentView()->setActionAvailable(brls::BUTTON_A, !settingMode);
            return true;
        });
        registerAction("Save", brls::BUTTON_RB, [this](brls::View*) {
            addSavedTimerEntry(savedTimeMs);
            return true;
        });
        registerAction("Load", brls::BUTTON_LT, [this](brls::View*) {
            auto callback = [this](unsigned long long ms) {
                state = TIMER_STOPPED;
                if (vibrating) stop_vibration();
                savedTimeMs = ms;
                durationMs = savedTimeMs;
                pausedMs = savedTimeMs;
                updateLabel();
            };
            brls::Application::pushActivity(new SavedTimersActivity(&savedTimers, callback), brls::TransitionAnimation::FADE);
            return true;
        });
        registerAction("Exit", brls::BUTTON_START, [this](brls::View*) {
            if (vibrating) stop_vibration();
            brls::Application::quit();
            return true;
        });
        registerAction("", brls::BUTTON_UP, [this](brls::View*) {
            if (!settingMode) return false;
            unsigned long long minutes = (pausedMs > 0 ? pausedMs : durationMs) / 60000ULL;
            unsigned long long seconds = ((pausedMs > 0 ? pausedMs : durationMs) % 60000ULL) / 1000ULL;
            if (selectedField == 0) {
                if (minutes < 99) minutes++;
            } else {
                if (seconds < 59) seconds++;
            }
            savedTimeMs = minutes * 60000ULL + seconds * 1000ULL;
            pausedMs = savedTimeMs;
            durationMs = savedTimeMs;
            updateLabel();
            return true;
        }, true, true);
        registerAction("", brls::BUTTON_DOWN, [this](brls::View*) {
            if (!settingMode) return false;
            unsigned long long minutes = (pausedMs > 0 ? pausedMs : durationMs) / 60000ULL;
            unsigned long long seconds = ((pausedMs > 0 ? pausedMs : durationMs) % 60000ULL) / 1000ULL;
            if (selectedField == 0) {
                if (minutes > 0) minutes--; else minutes = 0;
            } else {
                if (seconds > 0) seconds--; else seconds = 59;
            }
            savedTimeMs = minutes * 60000ULL + seconds * 1000ULL;
            pausedMs = savedTimeMs;
            durationMs = savedTimeMs;
            updateLabel();
            return true;
        }, true, true);
        registerAction("", brls::BUTTON_LEFT, [this](brls::View*) {
            if (settingMode) {
                selectedField = 0;
                updateCaret();
                return true;
            }
            return false;
        }, true);
        registerAction("", brls::BUTTON_RIGHT, [this](brls::View*) {
            if (!settingMode) return false;
            selectedField = 1;
            updateCaret();
            return true;
        }, true);
        updateTimer.setCallback([this]() {
            if (state == TIMER_RUNNING) {
                unsigned long long elapsedMs = (brls::getCPUTimeUsec() - startUsec) / 1000ULL;
                if (elapsedMs < durationMs) {
                    unsigned long long remaining = durationMs - elapsedMs;
                    unsigned long long total_seconds = remaining / 1000ULL;
                    unsigned long long minutes = total_seconds / 60ULL;
                    unsigned long long seconds = total_seconds % 60ULL;
                    char bufM[8], bufS[8];
                    std::snprintf(bufM, sizeof(bufM), "%02llu", minutes);
                    std::snprintf(bufS, sizeof(bufS), "%02llu", seconds);
                    minutesLabel->setText(bufM);
                    secondsLabel->setText(bufS);
                } else {
                    minutesLabel->setText("00");
                    secondsLabel->setText("00");
                    state = TIMER_DONE;
                    vibStep = 0;
                    vibrationTimer.setCallback([this]() {
                        if (state != TIMER_DONE) {
                            vibrationTimer.stop();
                            if (vibrating) stop_vibration();
                            return;
                        }
                        if ((vibStep % 2) == 0) {
                            start_vibration();
                            play_beep(880, 200);
                        } else {
                            stop_vibration();
                        }
                        vibStep++;
                        if (vibStep > 6) vibStep = 0;
                    });
                    vibrationTimer.start(200);
                }
            }
        });
        updateTimer.start(100);
    }

  private:
    void updateCaret() {
        if (settingMode) {
            minutesCaret->setText(selectedField == 0 ? "^" : "");
            secondsCaret->setText(selectedField == 1 ? "^" : "");
            float mX = minutesCol->getWidth() / 2.0f - minutesCaret->getWidth() / 2.0f;
            float sX = secondsCol->getWidth() / 2.0f - secondsCaret->getWidth() / 2.0f;
            minutesCaret->setDetachedPosition(mX, minutesCol->getHeight() - 20.0f);
            secondsCaret->setDetachedPosition(sX, secondsCol->getHeight() - 20.0f);
        } else {
            minutesCaret->setText("");
            secondsCaret->setText("");
        }
    }

    void updateLabel() {
        unsigned long long ms = 0;
        if (state == TIMER_RUNNING) {
            unsigned long long elapsedMs = (brls::getCPUTimeUsec() - startUsec) / 1000ULL;
            ms = elapsedMs < durationMs ? durationMs - elapsedMs : 0;
        } else if (state == TIMER_PAUSED) {
            ms = pausedMs > 0 ? pausedMs : savedTimeMs;
        } else if (state == TIMER_STOPPED || state == TIMER_DONE) {
            ms = savedTimeMs;
        }
        unsigned long long total_seconds = ms / 1000ULL;
        unsigned long long minutes = total_seconds / 60ULL;
        unsigned long long seconds = total_seconds % 60ULL;
        char bufM[8], bufS[8];
        std::snprintf(bufM, sizeof(bufM), "%02llu", minutes);
        std::snprintf(bufS, sizeof(bufS), "%02llu", seconds);
        minutesLabel->setText(bufM);
        secondsLabel->setText(bufS);
    }

    void buildSavedTimersSidebar() {
        // No longer needed - kept for compatibility
    }
    
    void addSavedTimerEntry(unsigned long long ms) {
        savedTimers.push_back(ms);
        save_timers_list(savedTimers);
    }

    brls::Box* minutesCol = nullptr;
    brls::Box* secondsCol = nullptr;
    brls::Label* minutesLabel = nullptr;
    brls::Label* secondsLabel = nullptr;
    brls::Label* colonLabel = nullptr;
    brls::Label* minutesCaret = nullptr;
    brls::Label* secondsCaret = nullptr;
    brls::Box* focusAnchor = nullptr;
    int vibStep = 0;
    brls::RepeatingTimer vibrationTimer;
    TimerState state;
    bool settingMode;
    int selectedField;
    unsigned long long savedTimeMs;
    unsigned long long durationMs;
    unsigned long long pausedMs;
    unsigned long long startUsec = 0;
    brls::RepeatingTimer updateTimer;
    std::vector<unsigned long long> savedTimers;
};

int main(int argc, char* argv[]) {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    init_vibration();
    Result rc = audoutInitialize();
    if (R_SUCCEEDED(rc)) {
        size_t max_samples = (SAMPLERATE * (size_t)MAX_BEEP_MS) / 1000;
        size_t max_size = max_samples * CHANNELCOUNT * BYTESPERSAMPLE;
        audio_buffer_size = (max_size + 0xfff) & ~0xfff;
        out_buf_data = (u8*)memalign(0x1000, audio_buffer_size);
        if (out_buf_data) {
            memset(out_buf_data, 0, audio_buffer_size);
            rc = audoutStartAudioOut();
            if (R_SUCCEEDED(rc)) audio_initialized = true;
        }
    }
    if (!brls::Application::init()) return EXIT_FAILURE;
    brls::Application::createWindow("Switch Timer");
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
    brls::Application::setGlobalQuit(false);
    brls::Application::pushActivity(new TimerActivity());
    while (brls::Application::mainLoop()) {}
    if (audio_initialized) {
        audoutStopAudioOut();
        audoutExit();
    }
    if (out_buf_data) free(out_buf_data);
    return EXIT_SUCCESS;
}
