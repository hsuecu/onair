
#include <iostream>
#include <fstream>

#include <complex>
#include <string>
#include <map>
#include <cmath>
#include <vector>

#include <inttypes.h>
#include <unistd.h>
#include <ctime>
#include <alsa/asoundlib.h>

/*alsa specific*/
#define ALSA_PCM_NEW_HW_PARAMS_API

#define HIGH 1
#define LOW 0
#define PI_2 6.283185307179586

/*how many times the flag will be send before and after
  data transmission
*/
#define FLG_REPEAT_COUNT 3

using std::cin;
using std::cout;
using std::vector;
using std::string;
using std::map;
using std::complex;
using std::endl;
using std::cerr;

namespace Uplink
{
    enum FlagType
    {
        sender_sync_flg,
        sender_fin_flg
    };

    class DifferentialEncoderOscillator
    {
        public:
        float q1, q0, sample_rate, angle, factor_high, factor_low;
        DifferentialEncoderOscillator(void);
        DifferentialEncoderOscillator(float _q1, float _q0, float sr);
        float sample(int state);
    };

    class HexPulseConvertor {
        vector<vector<int>> encoding_map = {
            {1,0,0},               // 0 .
            {1,1,0,0},             // 1 -
            {1,0,1,0,0},           // 2 ..
            {1,0,1,1,0,0},         // 3 .-
            {1,1,0,1,0,0},         // 4 -.
            {1,1,0,1,1,0,0},       // 5 --
            {1,0,1,0,1,0,0},       // 6 ...
            {1,0,1,0,1,1,0,0},     // 7 ..-
            {1,0,1,1,0,1,0,0},     // 8 .-.
            {1,0,1,1,0,1,1,0,0},   // 9 .--
            {1,1,0,1,0,1,0,0},     // A -..
            {1,1,0,1,0,1,1,0,0},   // B -.-
            {1,1,0,1,1,0,1,0,0},   // C --.
            {1,1,0,1,1,0,1,1,0,0}, // D ---
            {1,0,1,0,1,0,1,0,0},   // E ....
            {1,0,1,0,1,0,1,1,0,0}, // F ...-
            {0,0,0,0,0,0,0,0,0,0}, // SYNC
            {1,1,1,1,1,1,1,1,1,1}  // FIN
        };
    public:
        vector<int> convert(char data);
        vector<int> sync();
        vector<int> finish();
    };

    class Sender {
        /*sender config variable*/
        int32_t sample_width, max_amp = 2147483647, bit_depth = 32;
        unsigned int sample_rate, period;
        DifferentialEncoderOscillator oscillator;
        HexPulseConvertor convertor;

        /*alsa control variables*/
        int pcm, dir;
        snd_pcm_t* pcm_handle;
        snd_pcm_hw_params_t* params;
        snd_pcm_uframes_t frames;

        /*buffer configuration*/
        int32_t* buffer;
        int32_t buffer_size = 0, buffer_counter = 0;

        /*sleep estimator*/
        unsigned long frame_cnt = 0;
        unsigned int start_time;
        unsigned int end_time;

    public:
        Sender(float q1, float q0);
        void flush();
        void relayFlag(FlagType type);
        void sleep_estimator();
        void sync();
        void dsync();
        void put(char c);
        void puts(string str);
        string getInput();
        ~Sender();
    };

};



namespace Downlink{
    enum ProcState{ 
        init,
        gather_sync_flg_bits
    };

    class Decoder{
        int32_t count = 0, flip = 0, expected_len = 0;
        const int32_t sync_flg_len = 250;
        const int32_t sync_flg_bitlen = 10;
        const int32_t sync_flg_pop_times = 3;
        bool sync_state = false;
        ProcState procState = init;
        string buffer = "";
        char ret_word = 0x00;
        int flip_flop = 0;

        bool working;
        bool was_word_seen;

        unsigned int char_cnt;

        map<string, char> translator;
    public:
        Decoder();
        void clean_buffer();
        bool isWorking();
        void char_cntReset();
        bool char_cntGet();
        void trySync(int value);
        int process(int value);
    };

    class Receiver {
        // dft control variables
        float q_1, q_0, sample_rate;
        int32_t dft_window = 1920;
        int32_t dft_repeat_factor = 96;
        vector<complex<float>> dft_factor_high;
        vector<complex<float>> dft_factor_low;
        vector<float> reading_high;
        vector<float> reading_low;

        // alsa variables
        int pcm, dir;
        snd_pcm_t* pcm_handle;
        snd_pcm_hw_params_t* params;
        snd_pcm_uframes_t frames;

        // buffer control variables
        int32_t* buffer;
        int32_t buffer_size;
        Decoder decoder;

    public:
        Receiver(void);
        Receiver(float q1, float q0);
        void listen();
        void start();
        
        /*used to know that if an empty string was being send from sender
          as empty string is symbolic to switching between modes.*/
        bool isBreak();
        ~Receiver();
    };
};

string trim(string);