from flask import Flask, request
import requests

app = Flask(__name__)

# Telegram Bot credentials
BOT_TOKEN = "8350972909:AAHCuMUDReplqergSPEjdOsBNoYZfoH2NPU"
CHAT_ID = "1424251457"

@app.route('/send_notification', methods=['GET', 'POST'])
def send_notification():
    # Read message from the request
    message = request.args.get('message', 'No message provided')

    telegram_url = f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage"
    data = {
        "chat_id": CHAT_ID,
        "text": message
    }

    response = requests.post(telegram_url, data=data)
    if response.status_code == 200:
        return "Notification sent successfully!", 200
    return f"Failed to send notification. Error: {response.text}", 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
