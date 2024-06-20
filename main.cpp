#include<iostream>
#include<fstream>
#include<sstream>
#include<cmath>
#include<unistd.h>
#include<pthread.h>
#include<chrono>
#include"portaudio.h"

// ImGui headers
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

// my code
#include <utils/BiQuad.cpp>
#include <utils/imstyle.cpp>

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512
#define OSCILLATOR_COUNT 3


const int keyboard_mapping[12] = {GLFW_KEY_Z, GLFW_KEY_S, GLFW_KEY_X, GLFW_KEY_D, GLFW_KEY_C, GLFW_KEY_V, GLFW_KEY_G, GLFW_KEY_B, GLFW_KEY_H, GLFW_KEY_N, GLFW_KEY_J, GLFW_KEY_M};
// C # D # E  F # G # A # B
// Z S X D C  V G B H N J M


int current_note = 49;
int last_keyboard_press = -1;
int global_octave = 4;
float cutoff = 1000;

struct oscillator {
    bool enabled = true;
    int detune_cts = 0;
    int detune_st = 0;
    int rel_octave = 0;
    int waveform = 0;
    float pan = 0;
    float volume = 1;
    BiQuadFilter filter = BiQuadFilter(SAMPLE_RATE, cutoff);
} osc[OSCILLATOR_COUNT];

int trigger_phase = 0;
// -- ENVELOPE PHASE -|
// 0: idle            |
// 1: attack          |
// 2: decay           |
// 3: sustain         |
// 4: release         |

// for detecting pressed and released keys
bool key_pressed = false;
bool key_released = false;
bool last_key_state = false;

// envelope parameters
double env_volume = 0;
float attack = 0;
float decay = 0;
float sustain = 1;
float release = 0.2;

BiQuadFilter filter1 = BiQuadFilter(SAMPLE_RATE, cutoff);
BiQuadFilter filter2 = BiQuadFilter(SAMPLE_RATE, cutoff);


bool running = true;



float keyToFreq(int key, int detune_cts = 0) {
    float result_key = key + (float)(detune_cts)/100;
    float result = pow(2, ((result_key-49) / 12)) * 440.0;
    // printf("%.2f: %.2f\n", result_key, result);
    return result;
}

void *keyboard_input(void *arg) {
    GLFWwindow* window = (GLFWwindow*) arg;
    while(running) {
        for(int i=0; i<12; i++) {
            if(glfwGetKey(window, keyboard_mapping[i]) == GLFW_PRESS && !last_key_state) {
                // printf("doink %d\n", i);
                key_pressed = true;
                last_key_state = true;
                last_keyboard_press = i;
                current_note = 12*(global_octave-1) + i+4;
                trigger_phase = 0;
                i=12;
            }
        }
        if(last_key_state && glfwGetKey(window, keyboard_mapping[last_keyboard_press]) == GLFW_RELEASE) {
            // printf("undoink %d\n", last_keyboard_press);
            key_released = true;
            last_key_state = false;
        }
    }
    

    return NULL;
}

// Envelope volume
void *update_envelope(void *arg) {
    auto previousTime = std::chrono::steady_clock::now();

    while(running) {
        // update phase
        if(key_pressed) {
            if(attack == 0.0) {
                if(decay == 0.0) {
                    // HOLD
                    trigger_phase = 3;
                }
                else {
                    // DECAY
                    env_volume = 1.0;
                    trigger_phase = 2;
                }
            }
            else {
                // ATTACK
                trigger_phase = 1;
            }
            key_pressed = false;
        }
        if(key_released) {
            if(release == 0) {
                // IDLE
                trigger_phase = 0;
            }
            else {
                // RELEASE
                trigger_phase = 4;
            }
            key_released = false;
        }

        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = currentTime - previousTime;
        float elapsed_f = elapsed.count();

        // convert to seconds
        elapsed_f /= 1000000000;

        // update envelope volume

        // ATTACK
        if(trigger_phase == 1) {
            env_volume += elapsed_f / attack;
            if(env_volume >= 1.0) {
                trigger_phase = 2;
                env_volume = 1.0;
            }
        }
        // DECAY
        else if(trigger_phase == 2) {
            env_volume -= elapsed_f / decay;
            if(env_volume <= sustain) {
                trigger_phase = 3;
                env_volume = sustain;
            }
        }
        // HOLD
        else if(trigger_phase == 3) {
            env_volume = sustain;
        }
        // RELEASE
        else if(trigger_phase == 4) {
            env_volume -= elapsed_f / release;
            if(env_volume <= 0.0) {
                trigger_phase = 0;
                env_volume = 0.0;
            }
        }
        else {
            env_volume = 0.0;
        }

        previousTime = currentTime;

    }

    return NULL;
}

void reset_preset() {
    cutoff = 1000;
    attack = 0;
    decay = 0;
    sustain = 1;
    release = 0;
    global_octave = 4;
    for(int i=0; i<OSCILLATOR_COUNT; i++) {
        osc[i].enabled = true;
        osc[i].detune_cts = 0;
        osc[i].detune_st = 0;
        osc[i].rel_octave = 0;
        osc[i].waveform = 0;
        osc[i].pan = 0;
        osc[i].volume = 1;
    }

}

void save_preset(char* filename) {
    std::ofstream file(filename);
    if(file.is_open()) {
        file << "CUTOFF " << cutoff << std::endl;
        file << "ATTACK " << attack << std::endl;
        file << "DECAY " << decay << std::endl;
        file << "SUSTAIN " << sustain << std::endl;
        file << "RELEASE " << release << std::endl;
        file << "GLOBAL_OCTAVE " << global_octave << std::endl;
        for(int i=0; i<OSCILLATOR_COUNT; i++) {
            file << "OSCILLATOR " << i << std::endl;
            file << "ENABLED " << osc[i].enabled << std::endl;
            file << "DETUNE_C " << osc[i].detune_cts << std::endl;
            file << "DETUNE_S " << osc[i].detune_st << std::endl;
            file << "OCTAVE " << osc[i].rel_octave << std::endl;
            file << "WAVEFORM " << osc[i].waveform << std::endl;
            file << "PAN " << osc[i].pan << std::endl;
            file << "VOLUME " << osc[i].volume << std::endl;
            file << "OSC_END" << std::endl;
        }
        file.close();
    }
    else {
        std::cerr << "Could not open file " << filename << std::endl;
    }    
}

void load_preset(char* filename) {
    std::ifstream file(filename);
    if(file.is_open()) {
        std::string line;
        while(std::getline(file, line)) {
            std::string token;
            std::istringstream iss(line);
            iss >> token;
            if(token == "CUTOFF") {
                iss >> cutoff;
            }
            else if(token == "ATTACK") {
                iss >> attack;
            }
            else if(token == "DECAY") {
                iss >> decay;
            }
            else if(token == "SUSTAIN") {
                iss >> sustain;
            }
            else if(token == "RELEASE") {
                iss >> release;
            }
            else if(token == "GLOBAL_OCTAVE") {
                iss >> global_octave;
            }
            else if(token == "OSCILLATOR") {
                int osc_num;
                iss >> osc_num;
                while(std::getline(file, line)) {
                    std::istringstream iss(line);
                    iss >> token;
                    if(token == "ENABLED") {
                        iss >> osc[osc_num].enabled;
                        printf("osc %d enabled: %d\n", osc_num, osc[osc_num].enabled);
                    }
                    else if(token == "DETUNE_C") {
                        iss >> osc[osc_num].detune_cts;
                        printf("osc %d detune_c: %d\n", osc_num, osc[osc_num].detune_cts);
                    }
                    else if(token == "DETUNE_S") {
                        iss >> osc[osc_num].detune_st;
                        printf("osc %d detune_s: %d\n", osc_num, osc[osc_num].detune_st);
                    }
                    else if(token == "OCTAVE") {
                        iss >> osc[osc_num].rel_octave;
                        printf("osc %d octave: %d\n", osc_num, osc[osc_num].rel_octave);
                    }
                    else if(token == "WAVEFORM") {
                        iss >> osc[osc_num].waveform;
                        printf("osc %d waveform: %d\n", osc_num, osc[osc_num].waveform);
                    }
                    else if(token == "PAN") {
                        iss >> osc[osc_num].pan;
                        printf("osc %d pan: %f\n", osc_num, osc[osc_num].pan);
                    }
                    else if(token == "VOLUME") {
                        iss >> osc[osc_num].volume;
                        printf("osc %d volume: %f\n", osc_num, osc[osc_num].volume);
                    }
                    else if(token == "OSC_END") {
                        break;
                    }
                    else {
                        std::cerr << "Unknown token " << token << std::endl;
                    }
                }
            }
        
        }
    }
    else {
        std::cerr << "Could not open file " << filename << std::endl;
    }
}


float original_samples[FRAMES_PER_BUFFER];

// Audio callback
int audioCallback(const void *inputBuffer, void *outputBuffer,
                  unsigned long framesPerBuffer,
                  const PaStreamCallbackTimeInfo *timeInfo,
                  PaStreamCallbackFlags statusFlags,
                  void *userData)
{
    float *out = (float*)outputBuffer;
    (void) inputBuffer;
    double outputTime = (double) timeInfo->outputBufferDacTime;

    float final_samples_L[framesPerBuffer];
    float final_samples_R[framesPerBuffer];
    for(unsigned int i=0; i<framesPerBuffer; i++) final_samples_L[i]=0;
    for(unsigned int i=0; i<framesPerBuffer; i++) final_samples_R[i]=0;

    if(filter1.cutoffFreq != cutoff) { filter1.switchCutoff(cutoff); filter2.switchCutoff(cutoff); }

    int active_osc = 0;
    for(int o=0; o<OSCILLATOR_COUNT; o++) {
        if(osc[o].enabled) {
            active_osc++;

            float frequency = keyToFreq(current_note+12*osc[o].rel_octave+osc[o].detune_st, osc[o].detune_cts);
            if(osc[o].filter.cutoffFreq != cutoff) osc[o].filter.switchCutoff(cutoff);

            // initial waveform
            for(unsigned int i = 0; i < framesPerBuffer; i++)
            {
                // SINE
                if(osc[o].waveform == 0) {
                    original_samples[i] = sin(frequency * 2 * M_PI * (outputTime + (double) i / SAMPLE_RATE));
                }
                // SAWTOOTH
                else if(osc[o].waveform == 1) {
                    original_samples[i] = 2.0 * fmod((frequency * (outputTime + (double) i / SAMPLE_RATE)), 1.0) - 1.0;
                }
                // SQUARE
                else if(osc[o].waveform == 2) {
                    original_samples[i] = (sin(frequency * 2 * M_PI * (outputTime + (double) i / SAMPLE_RATE))) > 0 ? 1 : -1;
                }
                // TRIANGLE
                else if(osc[o].waveform == 3) {
                    original_samples[i] = 2/M_PI * asin( sin(2 * M_PI * frequency * (outputTime + (double) i / SAMPLE_RATE)) );
                }
            }

            // additional processing
            for(unsigned int i = 0; i < framesPerBuffer; i++)
            {
                // FILTER
                // float sample = osc[o].filter.process(original_samples[i]);
                // float sample = original_samples[i];

                // PAN
                final_samples_L[i] += original_samples[i] * std::min(1.0f, (1-osc[o].pan)) * osc[o].volume;
                final_samples_R[i] += original_samples[i] * std::min(1.0f, (1+osc[o].pan)) * osc[o].volume;
            }
        }
    }

    // send to audio stream
    if(active_osc)
    for(unsigned int i=0; i<framesPerBuffer; i++) {
        final_samples_L[i] = filter1.process(final_samples_L[i]);
        final_samples_L[i] = filter2.process(final_samples_R[i]);
        final_samples_L[i] = (final_samples_L[i]) * env_volume;
        final_samples_R[i] = (final_samples_R[i]) * env_volume;
        *out++ = final_samples_L[i];
        *out++ = final_samples_R[i];

        // *out++ = original_samples[i]*env_volume;
        // *out++ = original_samples[i]*env_volume;
    }

    return paContinue;
}

int main()
{
    PaError err;
    PaStream *stream;
    pthread_t adsr_thread, input_thread;


    err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    err = Pa_OpenDefaultStream(&stream,
                               0, // No input channels
                               2, // Stereo output
                               paFloat32, // 32-bit floating point output
                               SAMPLE_RATE,
                               FRAMES_PER_BUFFER,
                               audioCallback,
                               NULL); // No user data

    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        return 1;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(stream);
        Pa_Terminate();
        return 1;
    }

    load_preset("presets/latest.preset");

    // Initialize ImGui
    if (!glfwInit()) {
        std::cerr << "GLFW initialization failed" << std::endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(1280, 720, "ImGui Example", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    SetupImGuiStyle();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");


    pthread_create(&adsr_thread, NULL, update_envelope, NULL);
    pthread_create(&input_thread, NULL, keyboard_input, window);

    char name[32] = "", filename[48];

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui interface code
        ImGui::Begin("Simple Synthesizer");

        // ImGui::PlotLines("Samples", original_samples, FRAMES_PER_BUFFER);

        ImGui::SliderInt("Set Current Octave", &global_octave, 1, 7);
        ImGui::SliderFloat("Set Cutoff Frequency", &cutoff, 20.0f, 5000.0f);
        if(ImGui::Button("Big Reset Button")) {
            reset_preset();
        }

        ImGui::End();


        // generate window for each oscillator
        for(int i=0; i<OSCILLATOR_COUNT; i++) {
            char title[16];
            sprintf(title, "Oscillator #%d", i+1);
            ImGui::Begin(title);

            if(i) ImGui::Checkbox("Enabled", &osc[i].enabled);

            const char* waveformTypes[] = { "Sine", "Sawtooth", "Square", "Triangle"};
            ImGui::Combo("Waveform Type", &osc[i].waveform, waveformTypes, IM_ARRAYSIZE(waveformTypes));

            ImGui::SliderFloat("Volume", &osc[i].volume, 0, 2);
            ImGui::SliderInt("Dt Cents", &osc[i].detune_cts, 0, 100);
            ImGui::SliderInt("Dt Semitones", &osc[i].detune_st, -12, 12);
            ImGui::SliderInt("Octave", &osc[i].rel_octave, -3, 3);
            ImGui::SliderFloat("Pan", &osc[i].pan, -1, 1);

            ImGui::End();
        }

        


        ImGui::Begin("ADSR Envelope");

        ImGui::Text("Attack: %.2f s", attack);
        ImGui::SliderFloat("Set Attack", &attack, 0.0f, 5.0f);

        ImGui::Text("Decay: %.2f s", decay);
        ImGui::SliderFloat("Set Decay", &decay, 0.0f, 5.0f);

        ImGui::Text("Sustain: %.2f", sustain);
        ImGui::SliderFloat("Set Sustain", &sustain, 0.0f, 1.0f);

        ImGui::Text("Release: %.2f s", release);
        ImGui::SliderFloat("Set Release", &release, 0.0f, 5.0f);

        ImGui::End();



        ImGui::Begin("DEBOOG");
        //  BASIC
        // ImGui::Text("Frequency: %.2f Hz", keyToFreq(current_note));
        //  ENVELOPE
        ImGui::Text("Envelope Volume: %.2f", env_volume);
        ImGui::Text("Envelope Phase: %d", trigger_phase);
        //  KEYS
        ImGui::Text("Last Key: %d", last_key_state);
        ImGui::Text("Key Pressed? %s", key_pressed ? "yes" : "no");
        ImGui::Text("Key Released? %s", key_released ? "yes" : "no");
        ImGui::Text("Last Key State: %s", last_key_state ? "yes" : "no");
        //  PARAMS


        ImGui::End();


        ImGui::Begin("Presets");
        ImGui::InputText("Preset Name", name, 48);
        sprintf(filename, "presets/%s.preset", name);
        if(ImGui::Button("Save Preset")) {
            save_preset(filename);
        }
        if(ImGui::Button("Load Preset")) {
            load_preset(filename);
        }
        ImGui::End();


        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.3f, 0.3f, 0.3f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    running = false;
    pthread_join(adsr_thread, NULL);
    pthread_join(input_thread, NULL);

    err = Pa_StopStream(stream);
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    }

    err = Pa_CloseStream(stream);
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
    }

    Pa_Terminate();


    // Shutdown ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    save_preset("presets/latest.preset");

    return 0;
}