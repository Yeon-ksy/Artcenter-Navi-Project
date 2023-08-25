import pyaudio
import requests
import wave
import io
import json
import base64

# Naver Cloud STT 함수
def naver_cloud_stt(wav_file_data, client_id, client_secret):    
    lang = "Kor"
    url = "https://naveropenapi.apigw.ntruss.com/recog/v1/stt?lang=" + lang
    data = io.BytesIO(wav_file_data)
    headers = {
        "X-NCP-APIGW-API-KEY-ID": client_id,
        "X-NCP-APIGW-API-KEY": client_secret,
        "Content-Type": "application/octet-stream"
    }
    response = requests.post(url,  data=data, headers=headers)
    rescode = response.status_code
    if(rescode == 200):
        return response.text
    else:
        print("Error : " + response.text)
        return None

if __name__ == "__main__":
    client_id = 'ypt9t3829a'
    client_secret = '05qWZxHMMILm36mSuaCz1zUVsjx5W90OGy5W2gpo'

    URL = "http://localhost:8000/STT_test.wav"
    response = requests.get(URL)
    
    transcribed_text = naver_cloud_stt(response.content, client_id, client_secret)
    print("음성 전환된 텍스트: ", transcribed_text)

