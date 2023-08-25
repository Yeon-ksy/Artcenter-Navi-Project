import pyaudio
import requests
import wave
import io
import json

def naver_cloud_stt(wav_file_data, client_id, client_secret):    
    lang = "Kor"
    url = "https://naveropenapi.apigw.ntruss.com/recog/v1/stt?lang=" + lang

    headers = {
        "X-NCP-APIGW-API-KEY-ID": client_id,
        "X-NCP-APIGW-API-KEY": client_secret,
        "Content-Type": "application/octet-stream"
    }

    response = requests.post(url,  data=wav_file_data, headers=headers)
    rescode = response.status_code
    
    if(rescode == 200):
        return response.text
    else:
        print("Error : " + response.text)
        return None

def get_wav_info(wav_data):
    
    wav_file = io.BytesIO(wav_data)

    with wave.open(wav_file, 'rb') as w:
        sample_rate = w.getframerate()
        channels = w.getnchannels()

    return sample_rate, channels


URL = "http://localhost:8000/STT_test.wav"
response=requests.get(URL)

wav_data=response.content 
sample_rate, channels=get_wav_info(wav_data) 

BUFF_SIZE=512 
FORMAT=pyaudio.paInt16 
CHANNELS=channels 
RATE=sample_rate 

def stream_audio():
    
   p=pyaudio.PyAudio()

   stream=p.open(format=FORMAT,
                 channels=CHANNELS,
                 rate=RATE,
                 output=True,
                 frames_per_buffer=BUFF_SIZE)

   wav_header_size=44 
   count=0 

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

client_id='ypt9t3829a' 
client_secret='05qWZxHMMILm36mSuaCz1zUVsjx5W90OGy5W2gpo'

transcribed_text=naver_cloud_stt(response.content, client_id, client_secret) 
print("음성 전환된 텍스트: ", transcribed_text)

if __name__ == "__main__":
    stream_audio()
