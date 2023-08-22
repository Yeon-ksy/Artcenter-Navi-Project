import pyaudio
import requests
import wave
import io
import json
import base64

# Naver Cloud STT 함수
def naver_cloud_stt(wav_data, sample_rate, client_id, client_secret):

    LANGUAGE_CODE = 'Kor'
    headers = {'Content-Type': 'application/octet-stream',
            'X-NCP-APIGW-API-KEY-ID': client_id,
            'X-NCP-APIGW-API-KEY': client_secret}

    url = f'https://naveropenapi.apigw.ntruss.com/recog/v1/stt?lang={LANGUAGE_CODE}&sampleRate={sample_rate}'
            
    try:
        response = requests.post(url,
                                headers=headers,
                                data=wav_data)

        if response.status_code == 200:
            result = json.loads(response.text)
            return result['text']
        else:
            print("Error:", response.status_code)
            print(response.text)
            return None
    except Exception as e:
        print("Error:", e)
        return None
    

def get_wav_info(response):
    wav_file = io.BytesIO(response.content)

    with wave.open(wav_file, 'rb') as w:
        sample_rate = w.getframerate()
        channels = w.getnchannels()
        wav_data = w.readframes(w.getnframes())

    return sample_rate, channels

URL = "http://localhost:8000/STT_test.wav"
response = requests.get(URL)
sample_rate, channels = get_wav_info(response) 

BUFF_SIZE = 512
FORMAT = pyaudio.paInt16
CHANNELS = channels
RATE = sample_rate 

def stream_audio():
    p = pyaudio.PyAudio()
    stream = p.open(format=FORMAT, channels=CHANNELS, rate=RATE, output=True, frames_per_buffer=BUFF_SIZE)
    response = requests.get(URL, stream=True)

    wav_header_size = 44
    count = 0

    wav_data = bytearray()

    try:
        for chunk in response.iter_content(chunk_size=BUFF_SIZE):
            if count > wav_header_size:
                wav_data += chunk
                stream.write(chunk)

            count += BUFF_SIZE
    except KeyboardInterrupt:
        pass

    stream.stop_stream()
    stream.close()
    p.terminate()

    client_id = 'ypt9t3829a'
    client_secret = '05qWZxHMMILm36mSuaCz1zUVsjx5W90OGy5W2gpo'

    transcribed_text = naver_cloud_stt(wav_data,sample_rate, client_id, client_secret)
    print("음성 전환된 텍스트: ", transcribed_text)

if __name__ == "__main__":
    stream_audio()