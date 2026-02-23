Telegram server setup (local)

1) Install Python packages:
   pip install -r requirements.txt

2) Run the server:
   python telegram_server.py

3) Set your computer IP in wifi.cpp (TELEGRAM_SERVER_IP) so the ESP01 can reach this server.
   You can find your IP with `ipconfig` in a command prompt.

Notes
- The STM32 sends plain HTTP to this server because ESP01 does not support HTTPS.
- This server sends the HTTPS request to Telegram using the bot token and chat id.
