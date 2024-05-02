#include<iostream>
#include<cmath>
#include<unistd.h>
#include<pthread.h>
#include<chrono>
#include"portaudio.h"

// Include ImGui headers
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 256

float frequency = 440.0;
int waveform = 0;
// ---- SHAPE -|
// 0: sine     |
// 1: sawtooth |
// 2: square   |

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
float attack = 0.5;
float decay = 0;
float sustain = 1;
float release = 0;



bool running = true;


// Function to update the envelope volume
void *update_envelope(void *arg) {
    auto previousTime = std::chrono::steady_clock::now();

    while(running) {
        // update phase
        if(key_pressed) {
            printf("key pressed: ");
            if(attack == 0) {
                if(decay == 0) {
                    // HOLD
                    trigger_phase = 3;
                    printf("hold\n");
                }
                else {
                    // DECAY
                    trigger_phase = 2;
                    printf("decay\n");
                }
            }
            else {
                // ATTACK
                trigger_phase = 1;
                printf("attack\n");
            }
            key_pressed = false;
        }
        if(key_released) {
            printf("key released: ");
            if(release == 0) {
                // IDLE
                trigger_phase = 0;
                printf("idle\n");
            }
            else {
                // RELEASE
                trigger_phase = 4;
                printf("release\n");
            }
            key_released = false;
        }

        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = currentTime - previousTime;
        float elapsed_f = elapsed.count();

        // convert to seconds
        elapsed_f /= 1000000000;

        // update envelope volume

        printf  ("elapsed: %f\n", elapsed_f);

        // ATTACK
        if(trigger_phase == 1) {
            env_volume += elapsed_f / attack;
            if(env_volume >= 1.0) {
                printf("decay\n");
                trigger_phase = 2;
                env_volume = 1.0;
            }
        }
        // DECAY
        else if(trigger_phase == 2) {
            env_volume -= elapsed_f / decay;
            if(env_volume <= sustain) {
                printf("hold\n");
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
                printf("idle\n");
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

// Audio callback function
int audioCallback(const void *inputBuffer, void *outputBuffer,
                  unsigned long framesPerBuffer,
                  const PaStreamCallbackTimeInfo *timeInfo,
                  PaStreamCallbackFlags statusFlags,
                  void *userData)
{
    float *out = (float*)outputBuffer;
    (void) inputBuffer;
    double outputTime = (double) timeInfo->outputBufferDacTime;

    float original_samples[FRAMES_PER_BUFFER];

    for(unsigned int i = 0; i < framesPerBuffer; i++)
    {
        float sample = 0.0;
        if(waveform == 0) {
            sample = sin(frequency * 2 * M_PI * (outputTime + (double) i / SAMPLE_RATE)); // Simple sine wave
        }
        else if(waveform == 1) {
            sample = 2.0 * fmod((frequency * (outputTime + (double) i / SAMPLE_RATE)), 1.0) - 1.0; // Simple sawtooth wave
        }
        else if(waveform == 2) {
            sample = fmod((frequency * (outputTime + (double) i / SAMPLE_RATE)), 1.0) < 0.5 ? 1.0 : -1.0; // Simple square wave
        }


        // Apply volume envelope
        // sample *= env_volume;

        // *out++ = sample; // Left channel
        // *out++ = sample; // Right channel
    }
    

    return paContinue;
}

void *parseUserInput(void *arg)
{
    while (running)
    {
        char option;
        std::cout << "Enter option (f:frequency, w:waveform, q:quit)\n>";
        std::cin >> option;
        switch(option) {
            case 'f':
                std::cout << "Enter new frequency: ";
                std::cin >> frequency;
                break;
            case 'w':
                std::cout << "Enter new waveform (0:sine, 1:sawtooth, 2:square): ";
                std::cin >> waveform;
                break;
            case 'q':
                running = false;
                break;
            default:
                std::cout << "Invalid option" << std::endl;
                break;
        }
    }
    return NULL;
}

int main()
{
    PaError err;
    PaStream *stream;
    pthread_t adsr_thread;


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

    // Make the window's context current
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");



    pthread_create(&adsr_thread, NULL, update_envelope, NULL);


    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui interface code
        ImGui::Begin("Simple Synthesizer");

        ImGui::Text("[DEBUG] Frequency: %.2f Hz", frequency);
        ImGui::SliderFloat("Set Frequency", &frequency, 20.0f, 2000.0f);

        // ImGui::Text("Cutoff Frequency: %.2f Hz", cutoff);
        // ImGui::SliderFloat("Set Cutoff Frequency", &cutoff, 20.0f, 5000.0f);

        ImGui::Text("Waveform Type: %d", waveform);
        const char* waveformTypes[] = { "Sine", "Square", "Triangle" };
        ImGui::Combo("Set Waveform Type", &waveform, waveformTypes, IM_ARRAYSIZE(waveformTypes));
        

        ImGui::End();

        ImGui::Begin("ADSR Envelope");

        ImGui::Text("Attack: %.2f s", attack);
        ImGui::SliderFloat("Set Attack", &attack, 0.0f, 5.0f);

        ImGui::Text("Decay: %.2f s", decay);
        ImGui::SliderFloat("Set Decay", &decay, 0.0f, 5.0f);

        ImGui::Text("Sustain: %.2f", sustain);
        ImGui::SliderFloat("Set Sustain", &sustain, 0.0f, 1.0f);

        ImGui::Text("Release: %.2f s", release);
        ImGui::SliderFloat("Set Release", &release, 0.0f, 5.0f);

        ImGui::Text("[DEBUG] Envelope Volume: %.2f", env_volume);

        ImGui::End();

        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


        // Detect key press and release
        if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !last_key_state) {
            key_pressed = true;
            last_key_state = true;
        }
        if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE && last_key_state) {
            key_released = true;
            last_key_state = false;
        }

        glfwSwapBuffers(window);
    }

    running = false;
    pthread_join(adsr_thread, NULL); // Wait for thread to finish

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

    return 0;
}