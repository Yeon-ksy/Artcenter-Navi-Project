import pyaudio
import requests
import wave
import io

def get_wav_info(response):
    wav_file = io.BytesIO(response.content)

    with wave.open(wav_file, 'rb') as w:
        sample_rate = w.getframerate()
        channels = w.getnchannels()
    
    print("샘플 레이트:", sample_rate)
    print("채널 수:", channels)
    
    return sample_rate, channels

URL = "http://localhost:8000/STT_test.wav"
response = requests.get(URL)
sample_rate, channels = get_wav_info(response)


BUFF_SIZE = 1024
FORMAT = pyaudio.paInt16
CHANNELS = channels
RATE = sample_rate

def stream_audio():
    p = pyaudio.PyAudio()
    stream = p.open(format=FORMAT, channels=CHANNELS, rate=RATE, output=True, frames_per_buffer=BUFF_SIZE)

    response = requests.get(URL, stream=True)

    wav_header_size = 44
    count = 0

    try:
        for chunk in response.iter_content(chunk_size=BUFF_SIZE):
            if count > wav_header_size:
                stream.write(chunk)
            count += BUFF_SIZE
    except KeyboardInterrupt:
        pass

    stream.stop_stream()
    stream.close()
    p.terminate()

if __name__ == "__main__":
    # audio_thread = threading.Thread(target=stream_audio)
    # audio_thread.start()
    # audio_thread.join()
    stream_audio()