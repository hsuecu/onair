#include "../include/onair.h"

Uplink::DifferentialEncoderOscillator::DifferentialEncoderOscillator(void){

}

Uplink::DifferentialEncoderOscillator::DifferentialEncoderOscillator(float _q1, float _q0, float sr){
    q1 = _q1; q0 = _q0;
    sample_rate = sr;
    factor_high = PI_2 * q1 / sample_rate;
    factor_low = PI_2 * q0 / sample_rate;
}

float Uplink::DifferentialEncoderOscillator::sample(int state) {
    if(angle > PI_2) angle = fmod(angle, PI_2);
    if(state > 0) angle += factor_high;
    else angle += factor_low;
    return 0.5 * sin(angle);
}

vector<int> Uplink::HexPulseConvertor::convert(char data) {
    vector<int> ret, tmp;
    ret = encoding_map[(data) & 0x0f];
    tmp = encoding_map[(data >> 4) & 0x0f];
    ret.insert(ret.begin(), tmp.begin(), tmp.end());
    return ret;
}

vector<int> Uplink::HexPulseConvertor::sync() {
    return encoding_map[16];
}

vector<int> Uplink::HexPulseConvertor::finish() {
    return encoding_map[17];
}


Uplink::Sender::Sender(float q1, float q0){
    // upon observation sample_width must be equal to frames size
    sample_width = 1920;
    sample_rate = 384000;
    frames = 1920;
    oscillator = DifferentialEncoderOscillator(q1, q0, sample_rate);
    convertor = HexPulseConvertor();

    pcm = snd_pcm_open(&(pcm_handle), "default", SND_PCM_STREAM_PLAYBACK, 0);
    assert(pcm >= 0);
    snd_pcm_hw_params_alloca(&(params));
    pcm = snd_pcm_hw_params_any(pcm_handle, params);
    assert(pcm >= 0);
    pcm = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    assert(pcm >= 0);
    pcm = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S32);
    assert(pcm >= 0);
    pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &(sample_rate), &(dir));
    assert(pcm >= 0);
    pcm = snd_pcm_hw_params_set_period_size_near(pcm_handle, params, &(frames), &(dir));
    assert(pcm >= 0);
    pcm = snd_pcm_hw_params(pcm_handle, params);
    assert(pcm >= 0);
    snd_pcm_hw_params_get_period_size(params, &(frames), &(dir));
    buffer_size = frames;
    buffer = (int32_t*) malloc(sizeof(int32_t)*(buffer_size));
}

void Uplink::Sender::flush() {
    pcm = snd_pcm_writei(pcm_handle, buffer, buffer_size);
    if(pcm < 0) cout << snd_strerror(pcm) << endl;
    assert(pcm >= 0);
}

void Uplink::Sender::relayFlag(FlagType type) {
    vector<int> ret;
    if(type == sender_sync_flg) {
        ret = convertor.sync();
    }
    else {
        ret = convertor.finish();
    }
    for(int times = 0; times < FLG_REPEAT_COUNT; times++){
        for(int bit: ret) {
            for(int n = 0; n < sample_width; n++) {
                buffer[(buffer_counter)++] = max_amp * oscillator.sample(bit);
                if(buffer_counter == buffer_size) {
                    flush();
                    buffer_counter = 0;
                }
            }
        }
    }
}

void Uplink::Sender::sleep_estimator() {
    unsigned int diff = end_time - start_time;
    unsigned int ret = frame_cnt / 200; // each frame is 1920 byte long and it takes 200 micro seconds
                                        // to relay at 384000
    ret = ret - diff;
    if(ret > 0) {
        sleep(ret+1);
    }
    else {
        usleep(5000 * frame_cnt + 500000);
    }
}

void Uplink::Sender::sync() {
    usleep(500000);
    frame_cnt = 3;
    start_time = time(NULL);
    pcm = snd_pcm_prepare(pcm_handle);
    assert(pcm >= 0);
    relayFlag(sender_sync_flg);
}

void Uplink::Sender::dsync() {
    frame_cnt += 3;
    relayFlag(sender_fin_flg);
    end_time = time(NULL);
}

void Uplink::Sender::put(char c) {
    auto ret = convertor.convert(c);
    for(auto bit: ret) {
        frame_cnt ++;
        for(int n = 0; n < sample_width; n++) {
            buffer[buffer_counter++] = max_amp * oscillator.sample(bit);
            if(buffer_counter == buffer_size) {
                flush();
                buffer_counter = 0;
            }
        }
    }
}

void Uplink::Sender::puts(string str) {
    this->sync();
    for(auto c: str) {
        put(c);
    }
    this->dsync();
    this->sleep_estimator();
}

string Uplink::Sender::getInput() {
    cout << "<<< ";
    cout.flush();
    string line;
    getline(cin, line, '\n');
    line = trim(line);
    return line;
}

Uplink::Sender::~Sender() {
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    free(buffer);
}

Downlink::Decoder::Decoder(){
    translator["."] = 0x00;
    translator["-"] = 0x01;
    translator[".."] = 0x02;
    translator[".-"] = 0x03;
    translator["-."] = 0x04;
    translator["--"] = 0x05;
    translator["..."] = 0x06;
    translator["..-"] = 0x07;
    translator[".-."] = 0x08;
    translator[".--"] = 0x09;
    translator["-.."] = 0x0A;
    translator["-.-"] = 0x0B;
    translator["--."] = 0x0C;
    translator["---"] = 0x0D;
    translator["...."] = 0x0E;
    translator["...-"] = 0x0F;
    translator[""] = 0x00;
    working = false;
    was_word_seen = false;
};

void Downlink::Decoder::clean_buffer() {
    buffer = "";
    ret_word = 0x00;
}

bool Downlink::Decoder::isWorking() {
    return working;
}

void Downlink::Decoder::char_cntReset(){
    was_word_seen = false;
}

bool Downlink::Decoder::char_cntGet() {
    return was_word_seen;
}

void Downlink::Decoder::trySync(int value) {
    if(sync_state == true) return;
    if(procState == gather_sync_flg_bits) {
        if(value == 0) count++;
        else{
            // synced and now find expected length
            expected_len = (int32_t)ceil( (double)count / (sync_flg_bitlen*sync_flg_pop_times));
            count = 1;
            flip = 1;
            sync_state = true;
            procState = init;
            cout << "> ";
            cout.flush();
            ret_word = 0x00;
            buffer = "";
        }
        return;
    }
    if(value == 0) count++;
    else count = 0;
    if(count > sync_flg_len) {
        procState = gather_sync_flg_bits;
    }
}

int Downlink::Decoder::process(int value) {
    trySync(value);
    if(sync_state == false) return 1;
    if((value == 1 and flip == 1) or (value == 0 and flip == 0)){
        count ++;
        return 1;
    }
    if(flip == 1) {
        // bits dash
        if(count >= 0.3 * expected_len and count <= 1.5 * expected_len) {
            buffer.push_back('.');
        }
        else if(count > 1.5 * expected_len and count < 2.5 * expected_len) {
            buffer.push_back('-');
        }
        else {
            // cerr << ":desynced:";
            if(!(count > 2.8 * expected_len)) {
                cerr << endl << "::transmission failed::" << endl;
            }else{
                cout << ret_word;
            }
            sync_state = false;
            count = 0;
            flip = 0;
            procState = init;
            ret_word = 0x00;
            return 0;
        }
    }
    else if(flip == 0) {
        // same word or seperate word
        if(count >= 0.3 * expected_len and count < 1.5 * expected_len) {
            // same word
        }
        if(count >= 1.5 * expected_len and count <= 2.5 * expected_len) {
            if(flip_flop == 2) {
                cout << ret_word;
                char_cnt = char_cnt | 1;
                cout.flush();
                flip_flop = 1;
                ret_word = 0x00;
                ret_word = translator[buffer];
            }
            else{
                flip_flop++;
                ret_word = ret_word << 4;
                ret_word = ret_word | translator[buffer];
                was_word_seen = true;
            }
            buffer = "";
        }
    }
    flip = value;
    count = 1;
    return 1;
}

Downlink::Receiver::Receiver(float q1, float q0){
    q_1=q1;
    q_0=q0,
    sample_rate=384000;
    /*default variables setup*/
    float a = (2 * M_PI * q_1)/sample_rate;
    float b = (2 * M_PI * q_0)/sample_rate;
    float curr_q1 = 0.0f;
    float curr_q2 = 0.0f;
    dft_factor_high = vector<complex<float>>(dft_window, complex<float>{0.0f, 0.0f});
    dft_factor_low = vector<complex<float>>(dft_window, complex<float>{0.0f, 0.0f});
    int dft_window_factor_remainder = (int32_t)sample_rate % dft_repeat_factor;
    assert(dft_window_factor_remainder == 0);
    int size = (int)sample_rate/dft_repeat_factor;
    reading_high = vector<float>(size, 0.0f);
    reading_low = vector<float>(size, 0.0f);

    /*precomputing dft multiplication factors*/
    for(int i =0; i<dft_window; i++){
        dft_factor_high[i] = exp(complex<float>{0.0f, curr_q1});
        dft_factor_low[i] = exp(complex<float>{0.0f, curr_q2});
        curr_q1 += a;
        curr_q2 += b;
    }

    /*configuring alsa sound system*/
    pcm = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_CAPTURE, 0);
    assert(pcm >=0);
    snd_pcm_hw_params_alloca(&params);
    pcm = snd_pcm_hw_params_any(pcm_handle, params);
    assert(pcm >=0);
    pcm = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    assert(pcm >=0);
    pcm = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S32);
    assert(pcm >=0);
    pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, 1);
    assert(pcm >=0);
    unsigned int _sr = sample_rate;
    pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &_sr, &dir);
    assert(pcm >=0);
    frames = 1920;
    pcm = snd_pcm_hw_params_set_period_size_near(pcm_handle, params, &frames, &dir);
    assert(pcm >=0);
    pcm = snd_pcm_hw_params(pcm_handle, params);
    assert(pcm >=0);
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    /*buffer setup*/
    buffer_size = 3 * frames;
    buffer = (int32_t*) malloc(sizeof(int32_t)*buffer_size);
}

void Downlink::Receiver::listen() {
    // using this method only to encounter when dft_window, frames, sampling rate are
    // not realted via factors and are not dividing in respective manner.
    memcpy(buffer, buffer + 2 * frames, sizeof(int32_t) * frames);
    snd_pcm_readi(pcm_handle, buffer+frames, 2 * frames);
}

void Downlink::Receiver::start() {
    complex<float> x_q1(0.0f, 0.0f);
    complex<float> x_q0(0.0f, 0.0f);
    bool working = true;
    cout << ">>";
    cout.flush();
    decoder.char_cntReset();
    snd_pcm_prepare(pcm_handle);
    do{
        listen();
        for(int k=0, r=0; k < 2 * frames and working;k+=dft_repeat_factor, r++) {
            x_q1 = {0, 0};
            x_q0 = {0, 0};
            for(int m=0; m < dft_window; m++) {
                x_q1 += (float)buffer[k+m] * dft_factor_high[m];
                x_q0 += (float)buffer[k+m] * dft_factor_low[m];
            }
            int value = (abs(x_q1) >= abs(x_q0) ? 1 : 0);
            if(decoder.process(value));
            else working = false;
        }
    }while(working);
    cout << endl;
    cout.flush();
}


bool Downlink::Receiver::isBreak() {
    return !decoder.char_cntGet();
}

Downlink::Receiver::~Receiver() {
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    free(buffer);
}

string trim(string line){
    string newString;
    for (char ch : line){
        if (ch == '\n' || ch == '\r')
            continue;
        newString += ch;
    }
    return newString;
}